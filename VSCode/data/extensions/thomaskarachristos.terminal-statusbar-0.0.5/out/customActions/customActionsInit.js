"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const configurationFile_1 = require("./configurationFile");
function activate(context) {
    const configurationFile = new configurationFile_1.ConfigurationFile(context);
    try {
        configurationFile.checkIfFileExistAndCrete();
        configurationFile.parseFile();
        configurationFile.watchConfigurationChanges(context);
        configurationFile.editConfigurationFile(context);
    }
    catch (e) {
        console.log(e);
        vscode.window.showInformationMessage("Problem with custom actions, check if you have permissions to write and read files.");
    }
}
exports.default = activate;
//# sourceMappingURL=customActionsInit.js.map