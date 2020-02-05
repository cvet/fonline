"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const nls = require("vscode-nls");
const localize = nls.loadMessageBundle(__filename);
exports.evalNotAvailableMsg = localize(0, null);
exports.runtimeNotConnectedMsg = localize(1, null);
exports.noRestartFrame = localize(2, null);
class ErrorWithMessage extends Error {
    constructor(message) {
        super(message.format);
        this.id = message.id;
        this.format = message.format;
        this.variables = message.variables;
        this.sendTelemetry = message.sendTelemetry;
        this.showUser = message.showUser;
        this.url = message.url;
        this.urlLabel = message.urlLabel;
    }
}
exports.ErrorWithMessage = ErrorWithMessage;
function attributePathNotExist(attribute, path) {
    return new ErrorWithMessage({
        id: 2007,
        format: localize(3, null, attribute, '{path}'),
        variables: { path }
    });
}
exports.attributePathNotExist = attributePathNotExist;
/**
 * Error stating that a relative path should be absolute
 */
function attributePathRelative(attribute, path) {
    return new ErrorWithMessage(withInfoLink(2008, localize(4, null, attribute, '{path}', '${workspaceFolder}/'), { path }, 20003));
}
exports.attributePathRelative = attributePathRelative;
/**
 * Get error with 'More Information' link.
 */
function withInfoLink(id, format, variables, infoId) {
    return new ErrorWithMessage({
        id,
        format,
        variables,
        showUser: true,
        url: 'http://go.microsoft.com/fwlink/?linkID=534832#_' + infoId.toString(),
        urlLabel: localize(5, null)
    });
}
exports.withInfoLink = withInfoLink;
function setValueNotSupported() {
    return new ErrorWithMessage({
        id: 2004,
        format: localize(6, null)
    });
}
exports.setValueNotSupported = setValueNotSupported;
function errorFromEvaluate(errMsg) {
    return new ErrorWithMessage({
        id: 2025,
        format: errMsg
    });
}
exports.errorFromEvaluate = errorFromEvaluate;
function sourceRequestIllegalHandle() {
    return new ErrorWithMessage({
        id: 2027,
        format: 'sourceRequest error: illegal handle',
        sendTelemetry: true
    });
}
exports.sourceRequestIllegalHandle = sourceRequestIllegalHandle;
function sourceRequestCouldNotRetrieveContent() {
    return new ErrorWithMessage({
        id: 2026,
        format: localize(7, null)
    });
}
exports.sourceRequestCouldNotRetrieveContent = sourceRequestCouldNotRetrieveContent;
function pathFormat() {
    return new ErrorWithMessage({
        id: 2018,
        format: 'debug adapter only supports native paths',
        sendTelemetry: true
    });
}
exports.pathFormat = pathFormat;
function runtimeConnectionTimeout(timeoutMs, errMsg) {
    return new ErrorWithMessage({
        id: 2010,
        format: localize(8, null, '{_timeout}', '{_error}'),
        variables: { _error: errMsg, _timeout: timeoutMs + '' }
    });
}
exports.runtimeConnectionTimeout = runtimeConnectionTimeout;
function stackFrameNotValid() {
    return new ErrorWithMessage({
        id: 2020,
        format: 'stack frame not valid',
        sendTelemetry: true
    });
}
exports.stackFrameNotValid = stackFrameNotValid;
function noCallStackAvailable() {
    return new ErrorWithMessage({
        id: 2023,
        format: localize(9, null)
    });
}
exports.noCallStackAvailable = noCallStackAvailable;
function invalidThread(threadId) {
    return new ErrorWithMessage({
        id: 2030,
        format: 'Invalid thread {_thread}',
        variables: { _thread: threadId + '' },
        sendTelemetry: true
    });
}
exports.invalidThread = invalidThread;
function exceptionInfoRequestError() {
    return new ErrorWithMessage({
        id: 2031,
        format: 'exceptionInfoRequest error',
        sendTelemetry: true
    });
}
exports.exceptionInfoRequestError = exceptionInfoRequestError;
function noStoredException() {
    return new ErrorWithMessage({
        id: 2032,
        format: 'exceptionInfoRequest error: no stored exception',
        sendTelemetry: true
    });
}
exports.noStoredException = noStoredException;
function failedToReadPortFromUserDataDir(dataDirPath, err) {
    return new ErrorWithMessage({
        id: 2033,
        format: localize(10, null),
        variables: { dataDirPath, error: err.message },
        sendTelemetry: true
    });
}
exports.failedToReadPortFromUserDataDir = failedToReadPortFromUserDataDir;
function activePortFileContentsInvalid(dataDirPath, dataDirContents) {
    return new ErrorWithMessage({
        id: 2034,
        format: localize(11, null),
        variables: { dataDirPath, dataDirContents },
        sendTelemetry: true
    });
}
exports.activePortFileContentsInvalid = activePortFileContentsInvalid;

//# sourceMappingURL=errors.js.map
