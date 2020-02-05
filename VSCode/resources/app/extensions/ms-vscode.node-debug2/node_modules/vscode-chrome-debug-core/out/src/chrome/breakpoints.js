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
const internalSourceBreakpoint_1 = require("./internalSourceBreakpoint");
const utils = require("../utils");
const path = require("path");
const nls = require("vscode-nls");
let localize = nls.loadMessageBundle(__filename);
/**
 * Encapsulates all the logic surrounding breakpoints (e.g. set, unset, hit count breakpoints, etc.)
 */
class Breakpoints {
    constructor(adapter, _chromeConnection) {
        this.adapter = adapter;
        this._chromeConnection = _chromeConnection;
        this._nextUnboundBreakpointId = 0;
        // when working with _committedBreakpointsByUrl, we want to keep the url keys canonicalized for consistency
        // use methods getValueFromCommittedBreakpointsByUrl and setValueForCommittedBreakpointsByUrl
        this._committedBreakpointsByUrl = new Map();
        this._setBreakpointsRequestQ = Promise.resolve();
        this._breakpointIdHandles = new utils.ReverseHandles();
        this._pendingBreakpointsByUrl = new Map();
        this._hitConditionBreakpointsById = new Map();
    }
    get breakpointsQueueDrained() {
        return this._setBreakpointsRequestQ;
    }
    get committedBreakpointsByUrl() {
        return this._committedBreakpointsByUrl;
    }
    getValueFromCommittedBreakpointsByUrl(url) {
        let canonicalizedUrl = utils.canonicalizeUrl(url);
        return this._committedBreakpointsByUrl.get(canonicalizedUrl);
    }
    setValueForCommittedBreakpointsByUrl(url, value) {
        let canonicalizedUrl = utils.canonicalizeUrl(url);
        this._committedBreakpointsByUrl.set(canonicalizedUrl, value);
    }
    get chrome() { return this._chromeConnection.api; }
    reset() {
        this._committedBreakpointsByUrl = new Map();
        this._setBreakpointsRequestQ = Promise.resolve();
    }
    /**
     * Using the request object from the DAP, set all breakpoints on the target
     * @param args The setBreakpointRequest arguments from the DAP client
     * @param scripts The script container associated with this instance of the adapter
     * @param requestSeq The request sequence number from the DAP
     * @param ids IDs passed in for previously unverified breakpoints
     */
    setBreakpoints(args, scripts, requestSeq, ids) {
        if (args.source.path) {
            args.source.path = this.adapter.displayPathToRealPath(args.source.path);
            args.source.path = utils.canonicalizeUrl(args.source.path);
        }
        return this.validateBreakpointsPath(args)
            .then(() => {
            // Deep copy the args that we are going to modify, and keep the original values in originalArgs
            const originalArgs = args;
            args = JSON.parse(JSON.stringify(args));
            args = this.adapter.lineColTransformer.setBreakpoints(args);
            const sourceMapTransformerResponse = this.adapter.sourceMapTransformer.setBreakpoints(args, requestSeq, ids);
            if (sourceMapTransformerResponse && sourceMapTransformerResponse.args) {
                args = sourceMapTransformerResponse.args;
            }
            if (sourceMapTransformerResponse && sourceMapTransformerResponse.ids) {
                ids = sourceMapTransformerResponse.ids;
            }
            args.source = this.adapter.pathTransformer.setBreakpoints(args.source);
            // Get the target url of the script
            let targetScriptUrl;
            if (args.source.sourceReference) {
                const handle = scripts.getSource(args.source.sourceReference);
                if ((!handle || !handle.scriptId) && args.source.path) {
                    // A sourcemapped script with inline sources won't have a scriptId here, but the
                    // source.path has been fixed.
                    targetScriptUrl = args.source.path;
                }
                else {
                    const targetScript = scripts.getScriptById(handle.scriptId);
                    if (targetScript) {
                        targetScriptUrl = targetScript.url;
                    }
                }
            }
            else if (args.source.path) {
                targetScriptUrl = args.source.path;
            }
            if (targetScriptUrl) {
                // DebugProtocol sends all current breakpoints for the script. Clear all breakpoints for the script then add all of them
                const internalBPs = args.breakpoints.map(bp => new internalSourceBreakpoint_1.InternalSourceBreakpoint(bp));
                const setBreakpointsPFailOnError = this._setBreakpointsRequestQ
                    .then(() => this.clearAllBreakpoints(targetScriptUrl))
                    .then(() => this.addBreakpoints(targetScriptUrl, internalBPs, scripts))
                    .then(responses => ({ breakpoints: this.targetBreakpointResponsesToBreakpointSetResults(targetScriptUrl, responses, internalBPs, ids) }));
                const setBreakpointsPTimeout = utils.promiseTimeout(setBreakpointsPFailOnError, Breakpoints.SET_BREAKPOINTS_TIMEOUT, localize(0, null));
                // Do just one setBreakpointsRequest at a time to avoid interleaving breakpoint removed/breakpoint added requests to Crdp, which causes issues.
                // Swallow errors in the promise queue chain so it doesn't get blocked, but return the failing promise for error handling.
                this._setBreakpointsRequestQ = setBreakpointsPTimeout.catch(e => {
                    // Log the timeout, but any other error will be logged elsewhere
                    if (e.message && e.message.indexOf('timed out') >= 0) {
                        vscode_debugadapter_1.logger.error(e.stack);
                    }
                });
                // Return the setBP request, no matter how long it takes. It may take awhile in Node 7.5 - 7.7, see https://github.com/nodejs/node/issues/11589
                return setBreakpointsPFailOnError.then(setBpResultBody => {
                    const body = { breakpoints: setBpResultBody.breakpoints.map(setBpResult => setBpResult.breakpoint) };
                    if (body.breakpoints.every(bp => !bp.verified)) {
                        // We need to send the original args to avoid adjusting the line and column numbers twice here
                        return this.unverifiedBpResponseForBreakpoints(originalArgs, requestSeq, targetScriptUrl, body.breakpoints, localize(1, null));
                    }
                    body.breakpoints = this.adapter.sourceMapTransformer.setBreakpointsResponse(body.breakpoints, true, requestSeq) || body.breakpoints;
                    this.adapter.lineColTransformer.setBreakpointsResponse(body);
                    return body;
                });
            }
            else {
                return Promise.resolve(this.unverifiedBpResponse(args, requestSeq, undefined, localize(2, null)));
            }
        }, e => this.unverifiedBpResponse(args, requestSeq, undefined, e.message));
    }
    validateBreakpointsPath(args) {
        if (!args.source.path || args.source.sourceReference)
            return Promise.resolve();
        // When break on load is active, we don't need to validate the path, so return
        if (this.adapter.breakOnLoadActive) {
            return Promise.resolve();
        }
        return this.adapter.sourceMapTransformer.getGeneratedPathFromAuthoredPath(args.source.path).then(mappedPath => {
            if (!mappedPath) {
                return utils.errP(localize(3, null));
            }
            const targetPath = this.adapter.pathTransformer.getTargetPathFromClientPath(mappedPath);
            if (!targetPath) {
                return utils.errP(localize(4, null));
            }
            return undefined;
        });
    }
    /**
     * Makes the actual call to either Debugger.setBreakpoint or Debugger.setBreakpointByUrl, and returns the response.
     * Responses from setBreakpointByUrl are transformed to look like the response from setBreakpoint, so they can be
     * handled the same.
     */
    addBreakpoints(url, breakpoints, scripts) {
        return __awaiter(this, void 0, void 0, function* () {
            let responsePs;
            if (ChromeUtils.isEvalScript(url)) {
                // eval script with no real url - use debugger_setBreakpoint
                const scriptId = utils.lstrip(url, ChromeUtils.EVAL_NAME_PREFIX);
                responsePs = breakpoints.map(({ line, column = 0, condition }) => this.chrome.Debugger.setBreakpoint({ location: { scriptId, lineNumber: line, columnNumber: column }, condition }));
            }
            else {
                // script that has a url - use debugger_setBreakpointByUrl so that Chrome will rebind the breakpoint immediately
                // after refreshing the page. This is the only way to allow hitting breakpoints in code that runs immediately when
                // the page loads.
                const script = scripts.getScriptByUrl(url);
                // If script has been parsed, script object won't be undefined and we would have the mapping file on the disk and we can directly set breakpoint using that
                if (!this.adapter.breakOnLoadActive || script) {
                    const urlRegex = utils.pathToRegex(url);
                    responsePs = breakpoints.map(({ line, column = 0, condition }) => {
                        return this.addOneBreakpointByUrl(script && script.scriptId, urlRegex, line, column, condition);
                    });
                }
                else {
                    if (this.adapter.breakOnLoadActive) {
                        return yield this.adapter.breakOnLoadHelper.handleAddBreakpoints(url, breakpoints);
                    }
                }
            }
            // Join all setBreakpoint requests to a single promise
            return Promise.all(responsePs);
        });
    }
    /**
     * Adds a single breakpoint in the target using the url for the script
     * @param scriptId the chrome-devtools script id for the script on which we want to add a breakpoint
     * @param urlRegex The regular expression string which will be used to find the correct url on which to set the breakpoint
     * @param lineNumber Line number of the breakpoint
     * @param columnNumber Column number of the breakpoint
     * @param condition The (optional) breakpoint condition
     */
    addOneBreakpointByUrl(scriptId, urlRegex, lineNumber, columnNumber, condition) {
        return __awaiter(this, void 0, void 0, function* () {
            let bpLocation = { lineNumber, columnNumber };
            if (this.adapter.columnBreakpointsEnabled && scriptId) {
                try {
                    const possibleBpResponse = yield this.chrome.Debugger.getPossibleBreakpoints({
                        start: { scriptId, lineNumber, columnNumber: 0 },
                        end: { scriptId, lineNumber: lineNumber + 1, columnNumber: 0 },
                        restrictToFunction: false
                    });
                    if (possibleBpResponse.locations.length) {
                        const selectedLocation = ChromeUtils.selectBreakpointLocation(lineNumber, columnNumber, possibleBpResponse.locations);
                        bpLocation = { lineNumber: selectedLocation.lineNumber, columnNumber: selectedLocation.columnNumber || 0 };
                    }
                }
                catch (e) {
                    // getPossibleBPs not supported
                }
            }
            let result;
            try {
                result = yield this.chrome.Debugger.setBreakpointByUrl({ urlRegex, lineNumber: bpLocation.lineNumber, columnNumber: bpLocation.columnNumber, condition });
            }
            catch (e) {
                if (e.message === 'Breakpoint at specified location already exists.') {
                    return {
                        actualLocation: { lineNumber: bpLocation.lineNumber, columnNumber: bpLocation.columnNumber, scriptId }
                    };
                }
                else {
                    throw e;
                }
            }
            // Now convert the response to a SetBreakpointResponse so both response types can be handled the same
            const locations = result.locations;
            return {
                breakpointId: result.breakpointId,
                actualLocation: locations[0] && {
                    lineNumber: locations[0].lineNumber,
                    columnNumber: locations[0].columnNumber,
                    scriptId
                }
            };
        });
    }
    /**
     * Using the request object from the DAP, set all breakpoints on the target
     * @param args The setBreakpointRequest arguments from the DAP client
     * @param scripts The script container associated with this instance of the adapter
     * @param requestSeq The request sequence number from the DAP
     * @param ids IDs passed in for previously unverified breakpoints
     */
    getBreakpointsLocations(args, scripts, requestSeq) {
        return __awaiter(this, void 0, void 0, function* () {
            if (args.source.path) {
                args.source.path = this.adapter.displayPathToRealPath(args.source.path);
                args.source.path = utils.canonicalizeUrl(args.source.path);
            }
            try {
                yield this.validateBreakpointsPath(args);
            }
            catch (e) {
                vscode_debugadapter_1.logger.log('breakpointsLocations failed: ' + e.message);
                return { breakpoints: [] };
            }
            // Deep copy the args that we are going to modify, and keep the original values in originalArgs
            args = JSON.parse(JSON.stringify(args));
            args.endLine = this.adapter.lineColTransformer.convertClientLineToDebugger(typeof args.endLine === 'number' ? args.endLine : args.line + 1);
            args.endColumn = this.adapter.lineColTransformer.convertClientLineToDebugger(args.endColumn || 1);
            args.line = this.adapter.lineColTransformer.convertClientLineToDebugger(args.line);
            args.column = this.adapter.lineColTransformer.convertClientColumnToDebugger(args.column || 1);
            if (args.source.path) {
                const source1 = JSON.parse(JSON.stringify(args.source));
                const startArgs = this.adapter.sourceMapTransformer.setBreakpoints({ breakpoints: [{ line: args.line, column: args.column }], source: source1 }, requestSeq);
                args.line = startArgs.args.breakpoints[0].line;
                args.column = startArgs.args.breakpoints[0].column;
                const endArgs = this.adapter.sourceMapTransformer.setBreakpoints({ breakpoints: [{ line: args.endLine, column: args.endColumn }], source: args.source }, requestSeq);
                args.endLine = endArgs.args.breakpoints[0].line;
                args.endColumn = endArgs.args.breakpoints[0].column;
            }
            args.source = this.adapter.pathTransformer.setBreakpoints(args.source);
            // Get the target url of the script
            let targetScriptUrl;
            if (args.source.sourceReference) {
                const handle = scripts.getSource(args.source.sourceReference);
                if ((!handle || !handle.scriptId) && args.source.path) {
                    // A sourcemapped script with inline sources won't have a scriptId here, but the
                    // source.path has been fixed.
                    targetScriptUrl = args.source.path;
                }
                else {
                    const targetScript = scripts.getScriptById(handle.scriptId);
                    if (targetScript) {
                        targetScriptUrl = targetScript.url;
                    }
                }
            }
            else if (args.source.path) {
                targetScriptUrl = args.source.path;
            }
            if (targetScriptUrl) {
                const script = scripts.getScriptByUrl(targetScriptUrl);
                if (script) {
                    const end = typeof args.endLine === 'number' ?
                        { scriptId: script.scriptId, lineNumber: args.endLine, columnNumber: args.endColumn || 0 } :
                        { scriptId: script.scriptId, lineNumber: args.line + 1, columnNumber: 0 };
                    const possibleBpResponse = yield this.chrome.Debugger.getPossibleBreakpoints({
                        start: { scriptId: script.scriptId, lineNumber: args.line, columnNumber: args.column || 0 },
                        end,
                        restrictToFunction: false
                    });
                    if (possibleBpResponse.locations) {
                        let breakpoints = possibleBpResponse.locations.map(loc => {
                            return {
                                line: loc.lineNumber,
                                column: loc.columnNumber
                            };
                        });
                        breakpoints = this.adapter.sourceMapTransformer.setBreakpointsResponse(breakpoints, false, requestSeq);
                        const response = { breakpoints };
                        this.adapter.lineColTransformer.setBreakpointsResponse(response);
                        return response;
                    }
                    else {
                        return { breakpoints: [] };
                    }
                }
            }
            return null;
        });
    }
    /**
     * Transform breakpoint responses from the chrome-devtools target to the DAP response
     * @param url The URL of the script for which we are translating breakpoint responses
     * @param responses The setBreakpoint responses from the chrome-devtools target
     * @param requestBps The list of requested breakpoints pending a response
     * @param ids IDs passed in for previously unverified BPs
     */
    targetBreakpointResponsesToBreakpointSetResults(url, responses, requestBps, ids) {
        // Don't cache errored responses
        const committedBps = responses
            .filter(response => response && response.breakpointId);
        // Cache successfully set breakpoint ids from chrome in committedBreakpoints set
        this.setValueForCommittedBreakpointsByUrl(url, committedBps);
        // Map committed breakpoints to DebugProtocol response breakpoints
        return responses
            .map((response, i) => {
            // The output list needs to be the same length as the input list, so map errors to
            // unverified breakpoints.
            if (!response) {
                return {
                    isSet: false,
                    breakpoint: {
                        verified: false
                    }
                };
            }
            // response.breakpointId is undefined when no target BP is backing this BP, e.g. it's at the same location
            // as another BP
            const responseBpId = response.breakpointId || this.generateNextUnboundBreakpointId();
            let bpId;
            if (ids && ids[i]) {
                // IDs passed in for previously unverified BPs
                bpId = ids[i];
                this._breakpointIdHandles.set(bpId, responseBpId);
            }
            else {
                bpId = this._breakpointIdHandles.lookup(responseBpId) ||
                    this._breakpointIdHandles.create(responseBpId);
            }
            if (!response.actualLocation) {
                // If we don't have an actualLocation nor a breakpointId this is a pseudo-breakpoint because we are using break-on-load
                // so we mark the breakpoint as not set, so i'll be set after we load the actual script that has the breakpoint
                return {
                    isSet: response.breakpointId !== undefined,
                    breakpoint: {
                        id: bpId,
                        verified: false
                    }
                };
            }
            const thisBpRequest = requestBps[i];
            if (thisBpRequest.hitCondition) {
                if (!this.addHitConditionBreakpoint(thisBpRequest, response)) {
                    return {
                        isSet: true,
                        breakpoint: {
                            id: bpId,
                            message: localize(5, null, thisBpRequest.hitCondition),
                            verified: false
                        }
                    };
                }
            }
            return {
                isSet: true,
                breakpoint: {
                    id: bpId,
                    verified: true,
                    line: response.actualLocation.lineNumber,
                    column: response.actualLocation.columnNumber
                }
            };
        });
    }
    addHitConditionBreakpoint(requestBp, response) {
        const result = Breakpoints.HITCONDITION_MATCHER.exec(requestBp.hitCondition.trim());
        if (result && result.length >= 3) {
            let op = result[1] || '>=';
            if (op === '=')
                op = '==';
            const value = result[2];
            const expr = op === '%'
                ? `return (numHits % ${value}) === 0;`
                : `return numHits ${op} ${value};`;
            // eval safe because of the regex, and this is only a string that the current user will type in
            /* tslint:disable:no-function-constructor-with-string-args */
            const shouldPause = new Function('numHits', expr);
            /* tslint:enable:no-function-constructor-with-string-args */
            this._hitConditionBreakpointsById.set(response.breakpointId, { numHits: 0, shouldPause });
            return true;
        }
        else {
            return false;
        }
    }
    clearAllBreakpoints(url) {
        // We want to canonicalize this url because this._committedBreakpointsByUrl keeps url keys in canonicalized form
        url = utils.canonicalizeUrl(url);
        if (!this._committedBreakpointsByUrl.has(url)) {
            return Promise.resolve();
        }
        // Remove breakpoints one at a time. Seems like it would be ok to send the removes all at once,
        // but there is a chrome bug where when removing 5+ or so breakpoints at once, it gets into a weird
        // state where later adds on the same line will fail with 'breakpoint already exists' even though it
        // does not break there.
        return this._committedBreakpointsByUrl.get(url).reduce((p, bp) => {
            return p.then(() => this.chrome.Debugger.removeBreakpoint({ breakpointId: bp.breakpointId })).then(() => { });
        }, Promise.resolve()).then(() => {
            this._committedBreakpointsByUrl.delete(url);
        });
    }
    onBreakpointResolved(params, scripts) {
        const script = scripts.getScriptById(params.location.scriptId);
        const breakpointId = this._breakpointIdHandles.lookup(params.breakpointId);
        if (!script || !breakpointId) {
            // Breakpoint resolved for a script we don't know about or a breakpoint we don't know about
            return;
        }
        // If the breakpoint resolved is a stopOnEntry breakpoint, we just return since we don't need to send it to client
        if (this.adapter.breakOnLoadActive && this.adapter.breakOnLoadHelper.stopOnEntryBreakpointIdToRequestedFileName.has(params.breakpointId)) {
            return;
        }
        // committed breakpoints (this._committedBreakpointsByUrl) should always have url keys in canonicalized form
        const committedBps = this.getValueFromCommittedBreakpointsByUrl(script.url) || [];
        if (!committedBps.find(committedBp => committedBp.breakpointId === params.breakpointId)) {
            committedBps.push({ breakpointId: params.breakpointId, actualLocation: params.location });
        }
        this.setValueForCommittedBreakpointsByUrl(script.url, committedBps);
        const bp = {
            id: breakpointId,
            verified: true,
            line: params.location.lineNumber,
            column: params.location.columnNumber
        };
        // need to canonicalize this path because the following maps use paths canonicalized
        const scriptPath = utils.canonicalizeUrl(this.adapter.pathTransformer.breakpointResolved(bp, script.url));
        if (this._pendingBreakpointsByUrl.has(scriptPath)) {
            // If we set these BPs before the script was loaded, remove from the pending list
            this._pendingBreakpointsByUrl.delete(scriptPath);
        }
        this.adapter.sourceMapTransformer.breakpointResolved(bp, scriptPath);
        this.adapter.lineColTransformer.breakpointResolved(bp);
        this.adapter.session.sendEvent(new vscode_debugadapter_1.BreakpointEvent('changed', bp));
    }
    generateNextUnboundBreakpointId() {
        const unboundBreakpointUniquePrefix = '__::[vscode_chrome_debug_adapter_unbound_breakpoint]::';
        return `${unboundBreakpointUniquePrefix}${this._nextUnboundBreakpointId++}`;
    }
    unverifiedBpResponse(args, requestSeq, targetScriptUrl, message) {
        const breakpoints = args.breakpoints.map(bp => {
            return {
                verified: false,
                line: bp.line,
                column: bp.column,
                message,
                id: this._breakpointIdHandles.create(this.generateNextUnboundBreakpointId())
            };
        });
        return this.unverifiedBpResponseForBreakpoints(args, requestSeq, targetScriptUrl, breakpoints, message);
    }
    unverifiedBpResponseForBreakpoints(args, requestSeq, targetScriptUrl, breakpoints, defaultMessage) {
        breakpoints.forEach(bp => {
            if (!bp.message) {
                bp.message = defaultMessage;
            }
        });
        if (args.source.path) {
            const ids = breakpoints.map(bp => bp.id);
            // setWithPath: record whether we attempted to set the breakpoint, and if so, with which path.
            // We can use this to tell when the script is loaded whether we guessed correctly, and predict whether the BP will bind.
            this._pendingBreakpointsByUrl.set(utils.canonicalizeUrl(args.source.path), { args, ids, requestSeq, setWithPath: this.adapter.breakOnLoadActive ? '' : targetScriptUrl }); // Breakpoints need to be re-set when break-on-load is enabled
        }
        return { breakpoints };
    }
    handleScriptParsed(script, scripts, mappedUrl, sources) {
        return __awaiter(this, void 0, void 0, function* () {
            if (sources) {
                const filteredSources = sources.filter(source => source !== mappedUrl); // Tools like babel-register will produce sources with the same path as the generated script
                for (const filteredSource of filteredSources) {
                    yield this.resolvePendingBPs(filteredSource, scripts);
                }
            }
            if (utils.canonicalizeUrl(script.url) === mappedUrl && this._pendingBreakpointsByUrl.has(mappedUrl) && utils.canonicalizeUrl(this._pendingBreakpointsByUrl.get(mappedUrl).setWithPath) === utils.canonicalizeUrl(mappedUrl)) {
                // If the pathTransformer had no effect, and we attempted to set the BPs with that path earlier, then assume that they are about
                // to be resolved in this loaded script, and remove the pendingBP.
                this._pendingBreakpointsByUrl.delete(mappedUrl);
            }
            else {
                yield this.resolvePendingBPs(mappedUrl, scripts);
            }
        });
    }
    resolvePendingBPs(source, scripts) {
        return __awaiter(this, void 0, void 0, function* () {
            source = source && utils.canonicalizeUrl(source);
            const pendingBP = this._pendingBreakpointsByUrl.get(source);
            if (pendingBP && (!pendingBP.setWithPath || utils.canonicalizeUrl(pendingBP.setWithPath) === source)) {
                vscode_debugadapter_1.logger.log(`OnScriptParsed.resolvePendingBPs: Resolving pending breakpoints: ${JSON.stringify(pendingBP)}`);
                yield this.resolvePendingBreakpoint(pendingBP, scripts);
                this._pendingBreakpointsByUrl.delete(source);
            }
            else if (source) {
                const sourceFileName = path.basename(source).toLowerCase();
                if (Array.from(this._pendingBreakpointsByUrl.keys()).find(key => key.toLowerCase().indexOf(sourceFileName) > -1)) {
                    vscode_debugadapter_1.logger.log(`OnScriptParsed.resolvePendingBPs: The following pending breakpoints won't be resolved: ${JSON.stringify(pendingBP)} pendingBreakpointsByUrl = ${JSON.stringify([...this._pendingBreakpointsByUrl])} source = ${source}`);
                }
            }
        });
    }
    resolvePendingBreakpoint(pendingBP, scripts) {
        return this.setBreakpoints(pendingBP.args, scripts, pendingBP.requestSeq, pendingBP.ids).then(response => {
            response.breakpoints.forEach((bp, i) => {
                bp.id = pendingBP.ids[i];
                this.adapter.session.sendEvent(new vscode_debugadapter_1.BreakpointEvent('changed', bp));
            });
        });
    }
    handleHitCountBreakpoints(expectingStopReason, hitBreakpoints) {
        // Did we hit a hit condition breakpoint?
        for (let hitBp of hitBreakpoints) {
            if (this._hitConditionBreakpointsById.has(hitBp)) {
                // Increment the hit count and check whether to pause
                const hitConditionBp = this._hitConditionBreakpointsById.get(hitBp);
                hitConditionBp.numHits++;
                // Only resume if we didn't break for some user action (step, pause button)
                if (!expectingStopReason && !hitConditionBp.shouldPause(hitConditionBp.numHits)) {
                    this.chrome.Debugger.resume()
                        .catch(() => { });
                    return { didPause: false };
                }
            }
        }
        return null;
    }
}
Breakpoints.SET_BREAKPOINTS_TIMEOUT = 5000;
Breakpoints.HITCONDITION_MATCHER = /^(>|>=|=|<|<=|%)?\s*([0-9]+)$/;
exports.Breakpoints = Breakpoints;

//# sourceMappingURL=breakpoints.js.map
