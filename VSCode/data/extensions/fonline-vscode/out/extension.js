"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
// import * as build from './build';
const components = require("./components");
const fileExplorer = require("./fileExplorer");
function activate(context) {
    try {
        var outputChannel;
        outputChannel = vscode.window.createOutputChannel('FOnline');
        outputChannel.show(true);
        new components.ComponentsView(context);
        new fileExplorer.FileExplorer(context);
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