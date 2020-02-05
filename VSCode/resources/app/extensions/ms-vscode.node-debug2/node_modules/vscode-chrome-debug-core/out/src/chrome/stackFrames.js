"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const vscode_debugadapter_1 = require("vscode-debugadapter");
const ChromeUtils = require("./chromeUtils");
const utils = require("../utils");
const path = require("path");
const nls = require("vscode-nls");
const errors = require("../errors");
const variables_1 = require("./variables");
let localize = nls.loadMessageBundle(__filename);
class StackFrames {
    constructor() {
        this._frameHandles = new vscode_debugadapter_1.Handles();
    }
    /**
     * Clear the currently stored stack frames
     */
    reset() {
        this._frameHandles.reset();
    }
    /**
     * Get a stack frame by its id
     */
    getFrame(frameId) {
        return this._frameHandles.get(frameId);
    }
    getStackTrace({ args, scripts, originProvider, scriptSkipper, smartStepper, transformers, pauseEvent }) {
        return __awaiter(this, void 0, void 0, function* () {
            let stackFrames = pauseEvent.callFrames.map(frame => this.callFrameToStackFrame(frame, scripts, originProvider))
                .concat(this.asyncFrames(pauseEvent.asyncStackTrace, scripts, originProvider));
            const totalFrames = stackFrames.length;
            if (typeof args.startFrame === 'number') {
                stackFrames = stackFrames.slice(args.startFrame);
            }
            if (typeof args.levels === 'number') {
                stackFrames = stackFrames.slice(0, args.levels);
            }
            const stackTraceResponse = {
                stackFrames,
                totalFrames
            };
            yield transformers.pathTransformer.stackTraceResponse(stackTraceResponse);
            yield transformers.sourceMapTransformer.stackTraceResponse(stackTraceResponse);
            yield Promise.all(stackTraceResponse.stackFrames.map((frame) => __awaiter(this, void 0, void 0, function* () {
                // Remove isSourceMapped to convert back to DebugProtocol.StackFrame
                const isSourceMapped = frame.isSourceMapped;
                delete frame.isSourceMapped;
                if (!frame.source) {
                    return;
                }
                // Apply hints to skipped frames
                const getSkipReason = reason => localize(0, null, reason);
                if (frame.source.path && scriptSkipper.shouldSkipSource(frame.source.path)) {
                    frame.source.origin = (frame.source.origin ? frame.source.origin + ' ' : '') + getSkipReason('skipFiles');
                    frame.source.presentationHint = 'deemphasize';
                }
                else if (!isSourceMapped && (yield smartStepper.shouldSmartStep(frame, transformers.pathTransformer, transformers.sourceMapTransformer))) {
                    // TODO !isSourceMapped is a bit of a hack here
                    frame.source.origin = (frame.source.origin ? frame.source.origin + ' ' : '') + getSkipReason('smartStep');
                    frame.presentationHint = 'deemphasize';
                }
                // Allow consumer to adjust final path
                if (frame.source.path && frame.source.sourceReference) {
                    frame.source.path = scripts.realPathToDisplayPath(frame.source.path);
                }
                // And finally, remove the fake eval path and fix the name, if it was never resolved to a real path
                if (frame.source.path && ChromeUtils.isEvalScript(frame.source.path)) {
                    frame.source.path = undefined;
                    frame.source.name = scripts.displayNameForSourceReference(frame.source.sourceReference);
                }
            })));
            transformers.lineColTransformer.stackTraceResponse(stackTraceResponse);
            stackTraceResponse.stackFrames.forEach(frame => frame.name = this.formatStackFrameName(frame, args.format));
            return stackTraceResponse;
        });
    }
    getScopes({ args, scripts, transformers, variables, pauseEvent, currentException }) {
        const currentFrame = this._frameHandles.get(args.frameId);
        if (!currentFrame || !currentFrame.location || !currentFrame.callFrameId) {
            throw errors.stackFrameNotValid();
        }
        if (!currentFrame.callFrameId) {
            return { scopes: [] };
        }
        const currentScript = scripts.getScriptById(currentFrame.location.scriptId);
        const currentScriptUrl = currentScript && currentScript.url;
        const currentScriptPath = (currentScriptUrl && transformers.pathTransformer.getClientPathFromTargetPath(currentScriptUrl)) || currentScriptUrl;
        const scopes = currentFrame.scopeChain.map((scope, i) => {
            // The first scope should include 'this'. Keep the RemoteObject reference for use by the variables request
            const thisObj = i === 0 && currentFrame.this;
            const returnValue = i === 0 && currentFrame.returnValue;
            const variablesReference = variables.createHandle(new variables_1.ScopeContainer(currentFrame.callFrameId, i, scope.object.objectId, thisObj, returnValue));
            const resultScope = {
                name: scope.type.substr(0, 1).toUpperCase() + scope.type.substr(1),
                variablesReference,
                expensive: scope.type === 'global'
            };
            if (scope.startLocation && scope.endLocation) {
                resultScope.column = scope.startLocation.columnNumber;
                resultScope.line = scope.startLocation.lineNumber;
                resultScope.endColumn = scope.endLocation.columnNumber;
                resultScope.endLine = scope.endLocation.lineNumber;
            }
            return resultScope;
        });
        if (currentException && this.lookupFrameIndex(args.frameId, pauseEvent) === 0) {
            scopes.unshift({
                name: localize(1, null),
                variablesReference: variables.createHandle(variables_1.ExceptionContainer.create(currentException))
            });
        }
        const scopesResponse = { scopes };
        if (currentScriptPath) {
            transformers.sourceMapTransformer.scopesResponse(currentScriptPath, scopesResponse);
            transformers.lineColTransformer.scopeResponse(scopesResponse);
        }
        return scopesResponse;
    }
    mapCallFrame(frame, transformers, scripts, originProvider) {
        return __awaiter(this, void 0, void 0, function* () {
            const debuggerCF = this.runtimeCFToDebuggerCF(frame);
            const stackFrame = this.callFrameToStackFrame(debuggerCF, scripts, originProvider);
            yield transformers.pathTransformer.fixSource(stackFrame.source);
            yield transformers.sourceMapTransformer.fixSourceLocation(stackFrame);
            transformers.lineColTransformer.convertDebuggerLocationToClient(stackFrame);
            return stackFrame;
        });
    }
    // We parse stack trace from `formattedException`, source map it and return a new string
    mapFormattedException(formattedException, transformers) {
        return __awaiter(this, void 0, void 0, function* () {
            const exceptionLines = formattedException.split(/\r?\n/);
            for (let i = 0, len = exceptionLines.length; i < len; ++i) {
                const line = exceptionLines[i];
                const matches = line.match(/^\s+at (.*?)\s*\(?([^ ]+):(\d+):(\d+)\)?$/);
                if (!matches)
                    continue;
                const linePath = matches[2];
                const lineNum = parseInt(matches[3], 10);
                const adjustedLineNum = lineNum - 1;
                const columnNum = parseInt(matches[4], 10);
                const clientPath = transformers.pathTransformer.getClientPathFromTargetPath(linePath);
                const mapped = yield transformers.sourceMapTransformer.mapToAuthored(clientPath || linePath, adjustedLineNum, columnNum);
                if (mapped && mapped.source && utils.isNumber(mapped.line) && utils.isNumber(mapped.column) && utils.existsSync(mapped.source)) {
                    transformers.lineColTransformer.mappedExceptionStack(mapped);
                    exceptionLines[i] = exceptionLines[i].replace(`${linePath}:${lineNum}:${columnNum}`, `${mapped.source}:${mapped.line}:${mapped.column}`);
                }
                else if (clientPath && clientPath !== linePath) {
                    const location = { line: adjustedLineNum, column: columnNum };
                    transformers.lineColTransformer.mappedExceptionStack(location);
                    exceptionLines[i] = exceptionLines[i].replace(`${linePath}:${lineNum}:${columnNum}`, `${clientPath}:${location.line}:${location.column}`);
                }
            }
            return exceptionLines.join('\n');
        });
    }
    asyncFrames(stackTrace, scripts, originProvider) {
        if (stackTrace) {
            const frames = stackTrace.callFrames
                .map(frame => this.runtimeCFToDebuggerCF(frame))
                .map(frame => this.callFrameToStackFrame(frame, scripts, originProvider));
            frames.unshift({
                id: this._frameHandles.create(null),
                name: `[ ${stackTrace.description} ]`,
                source: undefined,
                line: undefined,
                column: undefined,
                presentationHint: 'label'
            });
            return frames.concat(this.asyncFrames(stackTrace.parent, scripts, originProvider));
        }
        else {
            return [];
        }
    }
    runtimeCFToDebuggerCF(frame) {
        return {
            callFrameId: undefined,
            scopeChain: undefined,
            this: undefined,
            location: {
                lineNumber: frame.lineNumber,
                columnNumber: frame.columnNumber,
                scriptId: frame.scriptId
            },
            url: frame.url,
            functionName: frame.functionName
        };
    }
    formatStackFrameName(frame, formatArgs) {
        let formattedName = frame.name;
        if (frame.source && formatArgs) {
            if (formatArgs.module) {
                formattedName += ` [${frame.source.name}]`;
            }
            if (formatArgs.line) {
                formattedName += ` Line ${frame.line}`;
            }
        }
        return formattedName;
    }
    callFrameToStackFrame(frame, scripts, originProvider) {
        const { location, functionName } = frame;
        const line = location.lineNumber;
        const column = location.columnNumber;
        const script = scripts.getScriptById(location.scriptId);
        try {
            // When the script has a url and isn't one we're ignoring, send the name and path fields. PathTransformer will
            // attempt to resolve it to a script in the workspace. Otherwise, send the name and sourceReference fields.
            const sourceReference = scripts.getSourceReferenceForScriptId(script.scriptId);
            const source = {
                name: path.basename(script.url),
                path: script.url,
                sourceReference,
                origin: originProvider(script.url)
            };
            // If the frame doesn't have a function name, it's either an anonymous function
            // or eval script. If its source has a name, it's probably an anonymous function.
            const frameName = functionName || (script.url ? '(anonymous function)' : '(eval code)');
            return {
                id: this._frameHandles.create(frame),
                name: frameName,
                source,
                line,
                column
            };
        }
        catch (e) {
            // Some targets such as the iOS simulator behave badly and return nonsense callFrames.
            // In these cases, return a dummy stack frame
            const evalUnknown = `${ChromeUtils.EVAL_NAME_PREFIX}_Unknown`;
            return {
                id: this._frameHandles.create({}),
                name: evalUnknown,
                source: { name: evalUnknown, path: evalUnknown },
                line,
                column
            };
        }
    }
    /**
     * Try to lookup the index of the frame with given ID. Returns -1 for async frames and unknown frames.
     */
    lookupFrameIndex(frameId, pauseEvent) {
        const currentFrame = this._frameHandles.get(frameId);
        if (!currentFrame || !currentFrame.callFrameId || !pauseEvent) {
            return -1;
        }
        return pauseEvent.callFrames.findIndex(frame => frame.callFrameId === currentFrame.callFrameId);
    }
}
exports.StackFrames = StackFrames;

//# sourceMappingURL=stackFrames.js.map
