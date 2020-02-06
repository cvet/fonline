"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
let outInfoChannel;
let outLogChannel;
// tslint:disable-next-line:no-backbone-get-set-outside-model
const logLevel = vscode.workspace.getConfiguration('gitHistory').get('logLevel');
function getInfoChannel() {
    if (outInfoChannel === undefined) {
        outInfoChannel = vscode.window.createOutputChannel('Git History Info');
    }
    return outInfoChannel;
}
function getLogChannel() {
    if (outLogChannel === undefined) {
        outLogChannel = vscode.window.createOutputChannel('Git History');
    }
    return outLogChannel;
}
exports.getLogChannel = getLogChannel;
// tslint:disable-next-line:no-any
function logError(error) {
    getLogChannel().appendLine(`[Error-${getTimeAndms()}] ${error.toString()}`.replace(/(\r\n|\n|\r)/gm, ''));
    getLogChannel().show();
    vscode.window.showErrorMessage('There was an error, please view details in the \'Git History Log\' output window');
}
exports.logError = logError;
function logInfo(message) {
    if (logLevel === 'Info' || logLevel === 'Debug') {
        getLogChannel().appendLine(`[Info -${getTimeAndms()}] ${message}`);
    }
}
exports.logInfo = logInfo;
function logDebug(message) {
    if (logLevel === 'Debug') {
        getLogChannel().appendLine(`[Debug-${getTimeAndms()}] ${message}`);
    }
}
exports.logDebug = logDebug;
function getTimeAndms() {
    const time = new Date();
    const hours = `0${time.getHours()}`.slice(-2);
    const minutes = `0${time.getMinutes()}`.slice(-2);
    const seconds = `0${time.getSeconds()}`.slice(-2);
    const milliSeconds = `00${time.getMilliseconds()}`.slice(-3);
    return `${hours}:${minutes}:${seconds}.${milliSeconds}`;
}
function showInfo(message) {
    getInfoChannel().clear();
    getInfoChannel().appendLine(message);
    getInfoChannel().show();
}
exports.showInfo = showInfo;
//# sourceMappingURL=logger.js.map