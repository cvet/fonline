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
const os = require("os");
const vscode_debugadapter_1 = require("vscode-debugadapter");
const telemetry_1 = require("../telemetry");
const utils = require("../utils");
const executionTimingsReporter_1 = require("../executionTimingsReporter");
exports.ErrorTelemetryEventName = 'error';
function isMessage(e) {
    return !!e.format;
}
function isChromeError(e) {
    return !!e.data;
}
class ChromeDebugSession extends vscode_debugadapter_1.LoggingDebugSession {
    constructor(obsolete_debuggerLinesAndColumnsStartAt1, obsolete_isServer, opts) {
        super(opts.logFilePath, obsolete_debuggerLinesAndColumnsStartAt1, obsolete_isServer);
        this._readyForUserTimeoutInMilliseconds = 5 * 60 * 1000; // 5 Minutes = 5 * 60 seconds = 5 * 60 * 1000 milliseconds
        this.reporter = new executionTimingsReporter_1.ExecutionTimingsReporter();
        this.haveTimingsWhileStartingUpBeenReported = false;
        logVersionInfo();
        this._extensionName = opts.extensionName;
        this._debugAdapter = new opts.adapter(opts, this);
        this.events = new executionTimingsReporter_1.StepProgressEventsEmitter([this._debugAdapter.events]);
        this.configureExecutionTimingsReporting();
        const safeGetErrDetails = err => {
            let errMsg;
            try {
                errMsg = (err && err.stack) ? err.stack : JSON.stringify(err);
            }
            catch (e) {
                errMsg = 'Error while handling previous error: ' + e.stack;
            }
            return errMsg;
        };
        const reportErrorTelemetry = (err, exceptionType) => {
            let properties = {};
            properties.successful = 'false';
            properties.exceptionType = exceptionType;
            utils.fillErrorDetails(properties, err);
            /* __GDPR__
               "error" : {
                    "${include}": [
                        "${IExecutionResultTelemetryProperties}",
                        "${DebugCommonProperties}"
                    ]
               }
             */
            telemetry_1.telemetry.reportEvent(exports.ErrorTelemetryEventName, properties);
        };
        process.addListener('uncaughtException', (err) => {
            reportErrorTelemetry(err, 'uncaughtException');
            vscode_debugadapter_1.logger.error(`******** Unhandled error in debug adapter: ${safeGetErrDetails(err)}`);
        });
        process.addListener('unhandledRejection', (err) => {
            reportErrorTelemetry(err, 'unhandledRejection');
            // Node tests are watching for the ********, so fix the tests if it's changed
            vscode_debugadapter_1.logger.error(`******** Unhandled error in debug adapter - Unhandled promise rejection: ${safeGetErrDetails(err)}`);
        });
    }
    /**
     * This needs a bit of explanation -
     * The Session is reinstantiated for each session, but consumers need to configure their instance of
     * ChromeDebugSession. Consumers should call getSession with their config options, then call
     * DebugSession.run with the result. Alternatively they could subclass ChromeDebugSession and pass
     * their options to the super constructor, but I think this is easier to follow.
     */
    static getSession(opts) {
        // class expression!
        return class extends ChromeDebugSession {
            constructor(debuggerLinesAndColumnsStartAt1, isServer) {
                super(debuggerLinesAndColumnsStartAt1, isServer, opts);
            }
        };
    }
    /**
     * Overload dispatchRequest to the debug adapters' Promise-based methods instead of DebugSession's callback-based methods
     */
    dispatchRequest(request) {
        // We want the request to be non-blocking, so we won't await for reportTelemetry
        this.reportTelemetry(`ClientRequest/${request.command}`, (reportFailure, telemetryPropertyCollector) => __awaiter(this, void 0, void 0, function* () {
            const response = new vscode_debugadapter_1.Response(request);
            try {
                vscode_debugadapter_1.logger.verbose(`From client: ${request.command}(${JSON.stringify(request.arguments)})`);
                if (!(request.command in this._debugAdapter)) {
                    reportFailure('The debug adapter doesn\'t recognize this command');
                    this.sendUnknownCommandResponse(response, request.command);
                }
                else {
                    telemetryPropertyCollector.addTelemetryProperty('requestType', request.type);
                    response.body = yield this._debugAdapter[request.command](request.arguments, telemetryPropertyCollector, request.seq);
                    this.sendResponse(response);
                }
            }
            catch (e) {
                if (!this.isEvaluateRequest(request.command, e)) {
                    reportFailure(e);
                }
                this.failedRequest(request.command, response, e);
            }
        }));
    }
    // { command: request.command, type: request.type };
    reportTelemetry(eventName, action) {
        return __awaiter(this, void 0, void 0, function* () {
            const startProcessingTime = process.hrtime();
            const startTime = Date.now();
            const isSequentialRequest = eventName === 'ClientRequest/initialize' || eventName === 'ClientRequest/launch' || eventName === 'ClientRequest/attach';
            const properties = {};
            const telemetryPropertyCollector = new telemetry_1.TelemetryPropertyCollector();
            if (isSequentialRequest) {
                this.events.emitStepStarted(eventName);
            }
            let failed = false;
            const sendTelemetry = () => {
                const timeTakenInMilliseconds = utils.calculateElapsedTime(startProcessingTime);
                properties.timeTakenInMilliseconds = timeTakenInMilliseconds.toString();
                if (isSequentialRequest) {
                    this.events.emitStepCompleted(eventName);
                }
                else {
                    this.events.emitRequestCompleted(eventName, startTime, timeTakenInMilliseconds);
                }
                Object.assign(properties, telemetryPropertyCollector.getProperties());
                // GDPR annotations go with each individual request type
                telemetry_1.telemetry.reportEvent(eventName, properties);
            };
            const reportFailure = e => {
                failed = true;
                properties.successful = 'false';
                properties.exceptionType = 'firstChance';
                utils.fillErrorDetails(properties, e);
                sendTelemetry();
            };
            // We use the reportFailure callback because the client might exit immediately after the first failed request, so we need to send the telemetry before that, if not it might get dropped
            yield action(reportFailure, telemetryPropertyCollector);
            if (!failed) {
                properties.successful = 'true';
                sendTelemetry();
            }
        });
    }
    isEvaluateRequest(requestType, error) {
        return !isMessage(error) && (requestType === 'evaluate');
    }
    failedRequest(requestType, response, error) {
        if (isMessage(error)) {
            this.sendErrorResponse(response, error);
            return;
        }
        if (this.isEvaluateRequest(requestType, error)) {
            // Errors from evaluate show up in the console or watches pane. Doesn't seem right
            // as it's not really a failed request. So it doesn't need the [extensionName] tag and worth special casing.
            response.message = error ? error.message : 'Unknown error';
            response.success = false;
            this.sendResponse(response);
            return;
        }
        const errUserMsg = isChromeError(error) ?
            error.message + ': ' + error.data :
            (error.message || error.stack);
        const errDiagnosticMsg = isChromeError(error) ?
            errUserMsg : (error.stack || error.message);
        vscode_debugadapter_1.logger.error(`Error processing "${requestType}": ${errDiagnosticMsg}`);
        // These errors show up in the message bar at the top (or nowhere), sometimes not obvious that they
        // come from the adapter, so add extensionName
        this.sendErrorResponse(response, 1104, '[{_extensionName}] Error processing "{_requestType}": {_stack}', { _extensionName: this._extensionName, _requestType: requestType, _stack: errUserMsg }, vscode_debugadapter_1.ErrorDestination.Telemetry);
    }
    sendUnknownCommandResponse(response, command) {
        this.sendErrorResponse(response, 1014, `[${this._extensionName}] Unrecognized request: ${command}`, null, vscode_debugadapter_1.ErrorDestination.Telemetry);
    }
    reportTimingsWhileStartingUpIfNeeded(requestedContentWasDetected, reasonForNotDetected) {
        if (!this.haveTimingsWhileStartingUpBeenReported) {
            const report = this.reporter.generateReport();
            const telemetryData = { RequestedContentWasDetected: requestedContentWasDetected.toString() };
            for (const reportProperty in report) {
                telemetryData[reportProperty] = JSON.stringify(report[reportProperty]);
            }
            if (!requestedContentWasDetected && typeof reasonForNotDetected !== 'undefined') {
                telemetryData.RequestedContentWasNotDetectedReason = reasonForNotDetected;
            }
            /* __GDPR__
               "report-start-up-timings" : {
                  "RequestedContentWasNotDetectedReason" : { "classification": "SystemMetaData", "purpose": "FeatureInsight" },
                  "${include}": [
                      "${ReportProps}",
                      "${DebugCommonProperties}"
                    ]
               }
             */
            telemetry_1.telemetry.reportEvent('report-start-up-timings', telemetryData);
            this.haveTimingsWhileStartingUpBeenReported = true;
        }
    }
    configureExecutionTimingsReporting() {
        this.reporter.subscribeTo(this.events);
        this._debugAdapter.events.once(ChromeDebugSession.FinishedStartingUpEventName, args => {
            this.reportTimingsWhileStartingUpIfNeeded(args ? args.requestedContentWasDetected : true, args && args.reasonForNotDetected);
        });
        setTimeout(() => this.reportTimingsWhileStartingUpIfNeeded(/*requestedContentWasDetected*/ false, /*reasonForNotDetected*/ 'timeout'), this._readyForUserTimeoutInMilliseconds);
    }
    shutdown() {
        process.removeAllListeners('uncaughtException');
        process.removeAllListeners('unhandledRejection');
        this.reportTimingsWhileStartingUpIfNeeded(/*requestedContentWasDetected*/ false, /*reasonForNotDetected*/ 'shutdown');
        super.shutdown();
    }
    sendResponse(response) {
        const originalLogVerbose = vscode_debugadapter_1.logger.verbose;
        try {
            vscode_debugadapter_1.logger.verbose = textToLog => {
                if (response && response.command === 'source' && response.body && response.body.content) {
                    const clonedResponse = Object.assign({}, response);
                    clonedResponse.body = Object.assign({}, response.body);
                    clonedResponse.body.content = '<removed script source for logs>';
                    return originalLogVerbose.call(vscode_debugadapter_1.logger, `To client: ${JSON.stringify(clonedResponse)}`);
                }
                else {
                    return originalLogVerbose.call(vscode_debugadapter_1.logger, textToLog);
                }
            };
            super.sendResponse(response);
        }
        finally {
            vscode_debugadapter_1.logger.verbose = originalLogVerbose;
        }
    }
}
ChromeDebugSession.FinishedStartingUpEventName = 'finishedStartingUp';
exports.ChromeDebugSession = ChromeDebugSession;
function logVersionInfo() {
    vscode_debugadapter_1.logger.log(`OS: ${os.platform()} ${os.arch()}`);
    vscode_debugadapter_1.logger.log(`Adapter node: ${process.version} ${process.arch}`);
    const coreVersion = require('../../../package.json').version;
    vscode_debugadapter_1.logger.log('vscode-chrome-debug-core: ' + coreVersion);
    /* __GDPR__FRAGMENT__
       "DebugCommonProperties" : {
          "Versions.DebugAdapterCore" : { "classification": "SystemMetaData", "purpose": "FeatureInsight" }
       }
     */
    telemetry_1.telemetry.addCustomGlobalProperty({ 'Versions.DebugAdapterCore': coreVersion });
}

//# sourceMappingURL=chromeDebugSession.js.map
