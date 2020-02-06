import * as vscode from 'vscode';
import * as fs from 'fs';
import * as path from 'path';
import * as build from './build';

var busy = false;

function execute() {
  if (busy)
    return;

  if (vscode.window.activeTextEditor == undefined) {
    vscode.window.showInformationMessage('No input source file');
    return;
  }

  var doc = vscode.window.activeTextEditor.document;
  if (path.extname(doc.fileName).toLowerCase() != '.cs') {
    vscode.window.showInformationMessage('Invalid input source file');
    return;
  }

  var files = fs.readdirSync(path.dirname(doc.fileName));
  var csprojFile =
    files.find((f: string) => path.extname(f).toLowerCase() === '.csproj');
  if (csprojFile == undefined) {
    vscode.window.showInformationMessage('Project file not found (.csproj)');
    return;
  }

  var csprojPath = path.join(path.dirname(doc.fileName), csprojFile);

  busy = true;

  vscode.commands.executeCommand('workbench.action.files.saveAll')
    .then(
      () => {
        setTimeout(() => {
          if (build.foBuild(csprojPath, 'Server') && build.foBuild(csprojPath, 'Client') && build.foBuild(csprojPath, 'Mapper'))
            vscode.window.showInformationMessage('FOnline build successful');

          busy = false;
        }, 1);
      },
      () => {
        busy = false;
      });
}
