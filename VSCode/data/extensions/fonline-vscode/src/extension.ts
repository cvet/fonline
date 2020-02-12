import * as vscode from 'vscode';

// import * as build from './build';
import * as fileExplorer from './fileExplorer';
import * as commands from './commands'
import * as dashboard from './dashboard'

export function activate(context: vscode.ExtensionContext) {
  const outputChannel: vscode.OutputChannel = vscode.window.createOutputChannel('FOnline');

  try {
    dashboard.show(context);

    new commands.TerminalManager(context);
    new fileExplorer.FileExplorer(context, /CMakeLists\.txt/);

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

    outputChannel.show(true);
    outputChannel.appendLine('Welcome to the FOnline Editor!');

  } catch (error) {
    vscode.window.showErrorMessage(error);

    outputChannel.show(true);
    outputChannel.appendLine('Something going wrong... Try restart editor');
    outputChannel.appendLine(error);
  }
}

export function deactivate() {
  // ...
}
