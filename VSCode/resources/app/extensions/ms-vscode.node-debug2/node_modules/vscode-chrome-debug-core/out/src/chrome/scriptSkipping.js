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
const utils = require("../utils");
class ScriptSkipper {
    constructor(_chromeConnection, _transformers) {
        this._chromeConnection = _chromeConnection;
        this._transformers = _transformers;
        this._skipFileStatuses = new Map();
        this._blackboxedRegexes = [];
    }
    get chrome() { return this._chromeConnection.api; }
    init(skipFiles, skipFileRegExps) {
        let patterns = [];
        if (skipFiles) {
            const skipFilesArgs = skipFiles.filter(glob => {
                if (glob.startsWith('!')) {
                    vscode_debugadapter_1.logger.warn(`Warning: skipFiles entries starting with '!' aren't supported and will be ignored. ("${glob}")`);
                    return false;
                }
                return true;
            });
            patterns = skipFilesArgs.map(glob => utils.pathGlobToBlackboxedRegex(glob));
        }
        if (skipFileRegExps) {
            patterns = patterns.concat(skipFileRegExps);
        }
        if (patterns.length) {
            this._blackboxedRegexes = patterns.map(pattern => new RegExp(pattern, 'i'));
            this.refreshBlackboxPatterns();
        }
    }
    toggleSkipFileStatus(args, scripts, transformers) {
        return __awaiter(this, void 0, void 0, function* () {
            // e.g. strip <node_internals>/
            if (args.path) {
                args.path = scripts.displayPathToRealPath(args.path);
            }
            const aPath = args.path || scripts.fakeUrlForSourceReference(args.sourceReference);
            const generatedPath = yield transformers.sourceMapTransformer.getGeneratedPathFromAuthoredPath(aPath);
            if (!generatedPath) {
                vscode_debugadapter_1.logger.log(`Can't toggle the skipFile status for: ${aPath} - haven't seen it yet.`);
                return;
            }
            const sources = yield transformers.sourceMapTransformer.allSources(generatedPath);
            if (generatedPath === aPath && sources.length) {
                // Ignore toggling skip status for generated scripts with sources
                vscode_debugadapter_1.logger.log(`Can't toggle skipFile status for ${aPath} - it's a script with a sourcemap`);
                return;
            }
            const newStatus = !this.shouldSkipSource(aPath);
            vscode_debugadapter_1.logger.log(`Setting the skip file status for: ${aPath} to ${newStatus}`);
            this._skipFileStatuses.set(aPath, newStatus);
            const targetPath = transformers.pathTransformer.getTargetPathFromClientPath(generatedPath) || generatedPath;
            const script = scripts.getScriptByUrl(targetPath);
            yield this.resolveSkipFiles(script, generatedPath, sources, /*toggling=*/ true);
            if (newStatus) {
                this.makeRegexesSkip(script.url);
            }
            else {
                this.makeRegexesNotSkip(script.url);
            }
        });
    }
    resolveSkipFiles(script, mappedUrl, sources, toggling) {
        return __awaiter(this, void 0, void 0, function* () {
            if (sources && sources.length) {
                const parentIsSkipped = this.shouldSkipSource(script.url);
                const libPositions = [];
                // Figure out skip/noskip transitions within script
                let inLibRange = parentIsSkipped;
                for (let s of sources) {
                    let isSkippedFile = this.shouldSkipSource(s);
                    if (typeof isSkippedFile !== 'boolean') {
                        // Inherit the parent's status
                        isSkippedFile = parentIsSkipped;
                    }
                    this._skipFileStatuses.set(s, isSkippedFile);
                    if ((isSkippedFile && !inLibRange) || (!isSkippedFile && inLibRange)) {
                        const details = yield this._transformers.sourceMapTransformer.allSourcePathDetails(mappedUrl);
                        const detail = details.find(d => d.inferredPath === s);
                        if (detail.startPosition) {
                            libPositions.push({
                                lineNumber: detail.startPosition.line,
                                columnNumber: detail.startPosition.column
                            });
                        }
                        inLibRange = !inLibRange;
                    }
                }
                // If there's any change from the default, set proper blackboxed ranges
                if (libPositions.length || toggling) {
                    if (parentIsSkipped) {
                        libPositions.splice(0, 0, { lineNumber: 0, columnNumber: 0 });
                    }
                    if (libPositions[0].lineNumber !== 0 || libPositions[0].columnNumber !== 0) {
                        // The list of blackboxed ranges must start with 0,0 for some reason.
                        // https://github.com/Microsoft/vscode-chrome-debug/issues/667
                        libPositions[0] = {
                            lineNumber: 0,
                            columnNumber: 0
                        };
                    }
                    yield this.chrome.Debugger.setBlackboxedRanges({
                        scriptId: script.scriptId,
                        positions: []
                    }).catch(() => this.warnNoSkipFiles());
                    if (libPositions.length) {
                        this.chrome.Debugger.setBlackboxedRanges({
                            scriptId: script.scriptId,
                            positions: libPositions
                        }).catch(() => this.warnNoSkipFiles());
                    }
                }
            }
            else {
                const status = yield this.getSkipStatus(mappedUrl);
                const skippedByPattern = this.matchesSkipFilesPatterns(mappedUrl);
                if (typeof status === 'boolean' && status !== skippedByPattern) {
                    const positions = status ? [{ lineNumber: 0, columnNumber: 0 }] : [];
                    this.chrome.Debugger.setBlackboxedRanges({
                        scriptId: script.scriptId,
                        positions
                    }).catch(() => this.warnNoSkipFiles());
                }
            }
        });
    }
    makeRegexesNotSkip(noSkipPath) {
        let somethingChanged = false;
        this._blackboxedRegexes = this._blackboxedRegexes.map(regex => {
            const result = utils.makeRegexNotMatchPath(regex, noSkipPath);
            somethingChanged = somethingChanged || (result !== regex);
            return result;
        });
        if (somethingChanged) {
            this.refreshBlackboxPatterns();
        }
    }
    makeRegexesSkip(skipPath) {
        let somethingChanged = false;
        this._blackboxedRegexes = this._blackboxedRegexes.map(regex => {
            const result = utils.makeRegexMatchPath(regex, skipPath);
            somethingChanged = somethingChanged || (result !== regex);
            return result;
        });
        if (!somethingChanged) {
            this._blackboxedRegexes.push(new RegExp(utils.pathToRegex(skipPath), 'i'));
        }
        this.refreshBlackboxPatterns();
    }
    refreshBlackboxPatterns() {
        this.chrome.Debugger.setBlackboxPatterns({
            patterns: this._blackboxedRegexes.map(regex => regex.source)
        }).catch(() => this.warnNoSkipFiles());
    }
    /**
     * If the source has a saved skip status, return that, whether true or false.
     * If not, check it against the patterns list.
     */
    shouldSkipSource(sourcePath) {
        const status = this.getSkipStatus(sourcePath);
        if (typeof status === 'boolean') {
            return status;
        }
        if (this.matchesSkipFilesPatterns(sourcePath)) {
            return true;
        }
        return undefined;
    }
    /**
     * Returns true if this path matches one of the static skip patterns
     */
    matchesSkipFilesPatterns(sourcePath) {
        return this._blackboxedRegexes.some(regex => {
            return regex.test(sourcePath);
        });
    }
    /**
     * Returns the current skip status for this path, which is either an authored or generated script.
     */
    getSkipStatus(sourcePath) {
        if (this._skipFileStatuses.has(sourcePath)) {
            return this._skipFileStatuses.get(sourcePath);
        }
        return undefined;
    }
    warnNoSkipFiles() {
        vscode_debugadapter_1.logger.log('Warning: this runtime does not support skipFiles');
    }
}
exports.ScriptSkipper = ScriptSkipper;

//# sourceMappingURL=scriptSkipping.js.map
