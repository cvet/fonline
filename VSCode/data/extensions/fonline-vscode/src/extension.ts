import * as vscode from 'vscode';

// import * as build from './build';
import * as fileExplorer from './fileExplorer';
import * as commands from './commands'
import * as status from './status'

export function activate(context: vscode.ExtensionContext) {
  const outputChannel: vscode.OutputChannel = vscode.window.createOutputChannel('FOnline');

  try {
    // Close Welcome page
    if (vscode.window.activeTextEditor && vscode.window.activeTextEditor.document.fileName == 'tasks')
      vscode.commands.executeCommand('workbench.action.closeActiveEditor');

    commands.init(context);
    status.check(context);

    context.subscriptions.push(
      vscode.commands.registerCommand('extension.run', () => {
        outputChannel.appendLine('run');
      }));

    context.subscriptions.push(
      vscode.commands.registerCommand('extension.compile', () => {
        outputChannel.appendLine('compile');
      }));

    context.subscriptions.push(
      vscode.commands.registerCommand('extension.build', () => {
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
