"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
// import * as build from './build';
// import * as components from './components';
function activate(context) {
    console.log('Congratulations, your extension "fonline-vscode" is now active!');
    try {
        var outputChannel;
        outputChannel = vscode.window.createOutputChannel('FOnline');
        outputChannel.show(true);
        // new components.ComponentsView(context);
        context.subscriptions.push(vscode.commands.registerCommand('extension.run', function () {
            outputChannel.appendLine('run');
        }));
        context.subscriptions.push(vscode.commands.registerCommand('extension.compile', function () {
            outputChannel.appendLine('compile');
        }));
        context.subscriptions.push(vscode.commands.registerCommand('extension.build', function () {
            outputChannel.appendLine('build');
        }));
        outputChannel.appendLine('Welcome to the FOnline Editor!');
    }
    catch (error) {
        vscode.window.showErrorMessage(error);
    }
}
exports.activate = activate;
function deactivate() {
    // ...
}
exports.deactivate = deactivate;
//# sourceMappingURL=extension.js.map