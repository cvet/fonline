import * as vscode from 'vscode';

// import * as build from './build';
import * as components from './components';
import * as fileExplorer from './fileExplorer';

export function activate(context: vscode.ExtensionContext) {
  try {
    var outputChannel: vscode.OutputChannel;
    outputChannel = vscode.window.createOutputChannel('FOnline');
    outputChannel.show(true);

    new components.ComponentsView(context);
    new fileExplorer.FileExplorer(context);

    context.subscriptions.push(
      vscode.commands.registerCommand('extension.run', function () {
        outputChannel.appendLine('run');
      }));

    context.subscriptions.push(
      vscode.commands.registerCommand('extension.compile', function () {
        outputChannel.appendLine('compile');
      }));

    context.subscriptions.push(
      vscode.commands.registerCommand('extension.build', function () {
        outputChannel.appendLine('build');
      }));

    outputChannel.appendLine('Welcome to the FOnline Editor!');
  } catch (error) {
    vscode.window.showErrorMessage(error);
  }
}

export function deactivate() {
  // ...
}
