const vscode = require('vscode');
var fs = require('fs');
var path = require('path');
var childProcess = require("child_process");
var busy;
var outputChannel;

function foBuild(csprojPath, target) {
    var buildToolPath = path.resolve('../../Mono/bin/xbuild');
    var buildToolParams = '/property:Configuration=' + target + ' /nologo /verbosity:quiet';
    var command = buildToolPath + ' ' + buildToolParams + ' "' + csprojPath + '"';

    outputChannel.append('Build ' + target + '...');

    try {
        var output = childProcess.execSync(command).toString();
        var code = 0;
    }
    catch (error) {
        output = error.stdout;
        code = error.status;
    }

    if (code == 0) {
        outputChannel.appendLine(' successful');
        if (output.length > 0)
            outputChannel.appendLine(output);
        return true;
    } else {
        outputChannel.appendLine(' failed');
        outputChannel.append(output);
        vscode.window.showInformationMessage('FOnline build "' + target + '" failed');
        return false;
    }
}

function activate(context) {
    busy = false;
    outputChannel = vscode.window.createOutputChannel("FOnline");

    let disposable = vscode.commands.registerCommand('extension.fonlineBuild', function () {
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
        var csprojFile = files.find(f => path.extname(f).toLowerCase() === '.csproj');
        if (csprojFile == undefined) {
            vscode.window.showInformationMessage('Project file not found (.csproj)');
            return;
        }

        var csprojPath = path.join(path.dirname(doc.fileName), csprojFile);
        outputChannel.clear();
        outputChannel.show(true);

        busy = true;

        vscode.commands.executeCommand('workbench.action.files.saveAll').then(r => {
            setTimeout(() => {
                if (foBuild(csprojPath, 'Server') && foBuild(csprojPath, 'Client') && foBuild(csprojPath, 'Mapper'))
                    vscode.window.showInformationMessage('FOnline build successful');

                busy = false;
            }, 1);
        }, r => {
            busy = false;
        });
    });

    context.subscriptions.push(disposable);
}
exports.activate = activate;

function deactivate() {
}
exports.deactivate = deactivate;
