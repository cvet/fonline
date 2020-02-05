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
const assert = require("assert");
const __1 = require("..");
class BreakOnLoadHelper {
    constructor(chromeDebugAdapter, breakOnLoadStrategy) {
        this._doesDOMInstrumentationRecieveExtraEvent = false;
        this._instrumentationBreakpointSet = false;
        // Break on load: Store some mapping between the requested file names, the regex for the file, and the chrome breakpoint id to perform lookup operations efficiently
        this._stopOnEntryBreakpointIdToRequestedFileName = new Map();
        this._stopOnEntryRequestedFileNameToBreakpointId = new Map();
        this._stopOnEntryRegexToBreakpointId = new Map();
        this.validateStrategy(breakOnLoadStrategy);
        this._chromeDebugAdapter = chromeDebugAdapter;
        this._breakOnLoadStrategy = breakOnLoadStrategy;
    }
    validateStrategy(breakOnLoadStrategy) {
        if (breakOnLoadStrategy !== 'regex' && breakOnLoadStrategy !== 'instrument') {
            throw new Error('Invalid breakOnLoadStrategy: ' + breakOnLoadStrategy);
        }
    }
    get stopOnEntryRequestedFileNameToBreakpointId() {
        return this._stopOnEntryRequestedFileNameToBreakpointId;
    }
    get stopOnEntryBreakpointIdToRequestedFileName() {
        return this._stopOnEntryBreakpointIdToRequestedFileName;
    }
    get instrumentationBreakpointSet() {
        return this._instrumentationBreakpointSet;
    }
    getScriptUrlFromId(scriptId) {
        return __1.utils.canonicalizeUrl(this._chromeDebugAdapter.scriptsById.get(scriptId).url);
    }
    setBrowserVersion(version) {
        return __awaiter(this, void 0, void 0, function* () {
            // On version 69 Chrome stopped sending an extra event for DOM Instrumentation: See https://bugs.chromium.org/p/chromium/issues/detail?id=882909
            // On Chrome 68 we were relying on that event to make Break on load work on breakpoints on the first line of a file. On Chrome 69 we need an alternative way to make it work.
            this._doesDOMInstrumentationRecieveExtraEvent = !version.isAtLeastVersion(69, 0);
        });
    }
    /**
     * Handles the onpaused event.
     * Checks if the event is caused by a stopOnEntry breakpoint of using the regex approach, or the paused event due to the Chrome's instrument approach
     * Returns whether we should continue or not on this paused event
     */
    handleOnPaused(notification) {
        return __awaiter(this, void 0, void 0, function* () {
            if (notification.hitBreakpoints && notification.hitBreakpoints.length) {
                // If breakOnLoadStrategy is set to regex, we may have hit a stopOnEntry breakpoint we put.
                // So we need to resolve all the pending breakpoints in this script and then decide to continue or not
                if (this._breakOnLoadStrategy === 'regex') {
                    let shouldContinue = yield this.handleStopOnEntryBreakpointAndContinue(notification);
                    return shouldContinue;
                }
            }
            else if (this.isInstrumentationPause(notification)) {
                // This is fired when Chrome stops on the first line of a script when using the setInstrumentationBreakpoint API
                const pausedScriptId = notification.callFrames[0].location.scriptId;
                // Now we wait for all the pending breakpoints to be resolved and then continue
                yield this._chromeDebugAdapter.getBreakpointsResolvedDefer(pausedScriptId).promise;
                vscode_debugadapter_1.logger.log('BreakOnLoadHelper: Finished waiting for breakpoints to get resolved.');
                let shouldContinue = this._doesDOMInstrumentationRecieveExtraEvent || (yield this.handleStopOnEntryBreakpointAndContinue(notification));
                return shouldContinue;
            }
            return false;
        });
    }
    isInstrumentationPause(notification) {
        return (notification.reason === 'EventListener' && notification.data.eventName === 'instrumentation:scriptFirstStatement') ||
            (notification.reason === 'ambiguous' && Array.isArray(notification.data.reasons) &&
                notification.data.reasons.every(r => r.reason === 'EventListener' && r.auxData.eventName === 'instrumentation:scriptFirstStatement'));
    }
    /**
     * Returns whether we should continue on hitting a stopOnEntry breakpoint
     * Only used when using regex approach for break on load
     */
    shouldContinueOnStopOnEntryBreakpoint(pausedLocation) {
        return __awaiter(this, void 0, void 0, function* () {
            // If the file has no unbound breakpoints or none of the resolved breakpoints are at (1,1), we should continue after hitting the stopOnEntry breakpoint
            let shouldContinue = true;
            // Important: For the logic that verifies if a user breakpoint is set in the paused location, we need to resolve pending breakpoints, and commit them, before
            // using committedBreakpointsByUrl for our logic.
            yield this._chromeDebugAdapter.getBreakpointsResolvedDefer(pausedLocation.scriptId).promise;
            const pausedScriptUrl = this.getScriptUrlFromId(pausedLocation.scriptId);
            // Important: We need to get the committed breakpoints only after all the pending breakpoints for this file have been resolved. If not this logic won't work
            const committedBps = this._chromeDebugAdapter.committedBreakpointsByUrl.get(pausedScriptUrl) || [];
            const anyBreakpointsAtPausedLocation = committedBps.filter(bp => bp.actualLocation &&
                bp.actualLocation.lineNumber === pausedLocation.lineNumber &&
                bp.actualLocation.columnNumber === pausedLocation.columnNumber).length > 0;
            // If there were any breakpoints at this location (Which generally should be (1,1)) we shouldn't continue
            if (anyBreakpointsAtPausedLocation) {
                // Here we need to store this information per file, but since we can safely assume that scriptParsed would immediately be followed by onPaused event
                // for the breakonload files, this implementation should be fine
                shouldContinue = false;
            }
            return shouldContinue;
        });
    }
    /**
     * Handles a script with a stop on entry breakpoint and returns whether we should continue or not on hitting that breakpoint
     * Only used when using regex approach for break on load
     */
    handleStopOnEntryBreakpointAndContinue(notification) {
        return __awaiter(this, void 0, void 0, function* () {
            const hitBreakpoints = notification.hitBreakpoints;
            let allStopOnEntryBreakpoints = true;
            const pausedScriptId = notification.callFrames[0].location.scriptId;
            const pausedScriptUrl = this._chromeDebugAdapter.scriptsById.get(pausedScriptId).url;
            const mappedUrl = yield this._chromeDebugAdapter.pathTransformer.scriptParsed(pausedScriptUrl);
            // If there is a breakpoint which is not a stopOnEntry breakpoint, we appear as if we hit that one
            // This is particularly done for cases when we end up with a user breakpoint and a stopOnEntry breakpoint on the same line
            for (let bp of hitBreakpoints) {
                let regexAndFileNames = this._stopOnEntryBreakpointIdToRequestedFileName.get(bp);
                if (!regexAndFileNames) {
                    notification.hitBreakpoints = [bp];
                    allStopOnEntryBreakpoints = false;
                }
                else {
                    const normalizedMappedUrl = __1.utils.canonicalizeUrl(mappedUrl);
                    if (regexAndFileNames.fileSet.has(normalizedMappedUrl)) {
                        regexAndFileNames.fileSet.delete(normalizedMappedUrl);
                        assert(this._stopOnEntryRequestedFileNameToBreakpointId.delete(normalizedMappedUrl), `Expected to delete break-on-load information associated with url: ${normalizedMappedUrl}`);
                        if (regexAndFileNames.fileSet.size === 0) {
                            vscode_debugadapter_1.logger.log(`Stop on entry breakpoint hit for last remaining file. Removing: ${bp} created for: ${normalizedMappedUrl}`);
                            yield this.removeBreakpointById(bp);
                            assert(this._stopOnEntryRegexToBreakpointId.delete(regexAndFileNames.urlRegex), `Expected to delete break-on-load information associated with regexp: ${regexAndFileNames.urlRegex}`);
                        }
                        else {
                            vscode_debugadapter_1.logger.log(`Stop on entry breakpoint hit but still has remaining files. Keeping: ${bp} that was hit for: ${normalizedMappedUrl} because it's still needed for: ${Array.from(regexAndFileNames.fileSet.entries()).join(', ')}`);
                        }
                    }
                }
            }
            // If all the breakpoints on this point are stopOnEntry breakpoints
            // This will be true in cases where it's a single breakpoint and it's a stopOnEntry breakpoint
            // This can also be true when we have multiple breakpoints and all of them are stopOnEntry breakpoints, for example in cases like index.js and index.bin.js
            // Suppose user puts breakpoints in both index.js and index.bin.js files, when the setBreakpoints function is called for index.js it will set a stopOnEntry
            // breakpoint on index.* files which will also match index.bin.js. Now when setBreakpoints is called for index.bin.js it will again put a stopOnEntry breakpoint
            // in itself. So when the file is actually loaded, we would have 2 stopOnEntry breakpoints */
            if (allStopOnEntryBreakpoints) {
                const pausedLocation = notification.callFrames[0].location;
                let shouldContinue = yield this.shouldContinueOnStopOnEntryBreakpoint(pausedLocation);
                if (shouldContinue) {
                    return true;
                }
            }
            return false;
        });
    }
    /**
     * Adds a stopOnEntry breakpoint for the given script url
     * Only used when using regex approach for break on load
     */
    addStopOnEntryBreakpoint(url) {
        return __awaiter(this, void 0, void 0, function* () {
            let responsePs;
            // Check if file already has a stop on entry breakpoint
            if (!this._stopOnEntryRequestedFileNameToBreakpointId.has(url)) {
                // Generate regex we need for the file
                const normalizedUrl = __1.utils.canonicalizeUrl(url);
                const urlRegex = ChromeUtils.getUrlRegexForBreakOnLoad(normalizedUrl);
                // Check if we already have a breakpoint for this regexp since two different files like script.ts and script.js may have the same regexp
                let breakpointId;
                breakpointId = this._stopOnEntryRegexToBreakpointId.get(urlRegex);
                // If breakpointId is undefined it means the breakpoint doesn't exist yet so we add it
                if (breakpointId === undefined) {
                    let result;
                    try {
                        result = yield this.setStopOnEntryBreakpoint(urlRegex);
                    }
                    catch (e) {
                        vscode_debugadapter_1.logger.log(`Exception occured while trying to set stop on entry breakpoint ${e.message}.`);
                    }
                    if (result) {
                        breakpointId = result.breakpointId;
                        this._stopOnEntryRegexToBreakpointId.set(urlRegex, breakpointId);
                    }
                    else {
                        vscode_debugadapter_1.logger.log(`BreakpointId was null when trying to set on urlregex ${urlRegex}. This normally happens if the breakpoint already exists.`);
                    }
                    responsePs = [result];
                }
                else {
                    responsePs = [];
                }
                // Store the new breakpointId and the file name in the right mappings
                this._stopOnEntryRequestedFileNameToBreakpointId.set(normalizedUrl, breakpointId);
                let regexAndFileNames = this._stopOnEntryBreakpointIdToRequestedFileName.get(breakpointId);
                // If there already exists an entry for the breakpoint Id, we add this file to the list of file mappings
                if (regexAndFileNames !== undefined) {
                    regexAndFileNames.fileSet.add(normalizedUrl);
                }
                else {
                    const fileSet = new Set();
                    fileSet.add(normalizedUrl);
                    this._stopOnEntryBreakpointIdToRequestedFileName.set(breakpointId, { urlRegex, fileSet });
                }
            }
            else {
                responsePs = [];
            }
            return Promise.all(responsePs);
        });
    }
    /**
     * Handles the AddBreakpoints request when break on load is active
     * Takes the action based on the strategy
     */
    handleAddBreakpoints(url, breakpoints) {
        return __awaiter(this, void 0, void 0, function* () {
            // If the strategy is set to regex, we try to match the file where user put the breakpoint through a regex and tell Chrome to put a stop on entry breakpoint there
            if (this._breakOnLoadStrategy === 'regex') {
                yield this.addStopOnEntryBreakpoint(url);
            }
            else if (this._breakOnLoadStrategy === 'instrument') {
                // Else if strategy is to use Chrome's experimental instrumentation API, we stop on all the scripts at the first statement before execution
                if (!this.instrumentationBreakpointSet) {
                    yield this.setInstrumentationBreakpoint();
                }
            }
            // Temporary fix: We return an empty element for each breakpoint that was requested
            return breakpoints.map(breakpoint => { return {}; });
        });
    }
    /**
     * Tells Chrome to set instrumentation breakpoint to stop on all the scripts before execution
     * Only used when using instrument approach for break on load
     */
    setInstrumentationBreakpoint() {
        return __awaiter(this, void 0, void 0, function* () {
            yield this._chromeDebugAdapter.chrome.DOMDebugger.setInstrumentationBreakpoint({ eventName: 'scriptFirstStatement' });
            this._instrumentationBreakpointSet = true;
        });
    }
    // Sets a breakpoint on (0,0) for the files matching the given regex
    setStopOnEntryBreakpoint(urlRegex) {
        return __awaiter(this, void 0, void 0, function* () {
            let result = yield this._chromeDebugAdapter.chrome.Debugger.setBreakpointByUrl({ urlRegex, lineNumber: 0, columnNumber: 0 });
            return result;
        });
    }
    // Removes a breakpoint by it's chrome-crdp-id
    removeBreakpointById(breakpointId) {
        return __awaiter(this, void 0, void 0, function* () {
            return yield this._chromeDebugAdapter.chrome.Debugger.removeBreakpoint({ breakpointId: breakpointId });
        });
    }
    /**
     * Checks if we need to call resolvePendingBPs on scriptParsed event
     * If break on load is active and we are using the regex approach, only call the resolvePendingBreakpoint function for files where we do not
     * set break on load breakpoints. For those files, it is called from onPaused function.
     * For the default Chrome's API approach, we don't need to call resolvePendingBPs from inside scriptParsed
     */
    shouldResolvePendingBPs(mappedUrl) {
        if (this._breakOnLoadStrategy === 'regex' && !this.stopOnEntryRequestedFileNameToBreakpointId.has(mappedUrl)) {
            return true;
        }
        return false;
    }
}
exports.BreakOnLoadHelper = BreakOnLoadHelper;

//# sourceMappingURL=breakOnLoadHelper.js.map
