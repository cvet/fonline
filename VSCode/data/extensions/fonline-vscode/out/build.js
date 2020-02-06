"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const childProcess = require("child_process");
const path = require("path");
const vscode = require("vscode");
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
    }
    else {
        outputChannel.appendLine(' failed');
        outputChannel.append(output);
        vscode.window.showInformationMessage('FOnline build "' + target + '" failed');
        return false;
    }
}
exports.foBuild = foBuild;
//# sourceMappingURL=build.js.map