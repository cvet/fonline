"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const commands = require("./commands");
const status = require("./status");
function activate(context) {
    const outputChannel = vscode.window.createOutputChannel('FOnline');
    try {
        commands.init(context);
        status.check(context);
        context.subscriptions.push(vscode.commands.registerCommand('extension.run', function () {
            outputChannel.appendLine('run');
        }));
        context.subscriptions.push(vscode.commands.registerCommand('extension.compile', function () {
            outputChannel.appendLine('compile');
        }));
        context.subscriptions.push(vscode.commands.registerCommand('extension.build', function () {
            outputChannel.appendLine('build');
        }));
        outputChannel.show(true);
        outputChannel.appendLine('Welcome to the FOnline Editor!');
    }
    catch (error) {
        vscode.window.showErrorMessage(error);
        outputChannel.show(true);
        outputChannel.appendLine('Something going wrong... Try restart editor');
        outputChannel.appendLine(error);
    }
}
exports.activate = activate;
function deactivate() {
    // ...
}
exports.deactivate = deactivate;
//# sourceMappingURL=extension.js.map