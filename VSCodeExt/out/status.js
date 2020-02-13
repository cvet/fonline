"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const commands = require("./commands");
function check(context) {
    checkReadiness();
}
exports.check = check;
function checkReadiness() {
    return __awaiter(this, void 0, void 0, function* () {
        if ((yield commands.execute('Check WSL')) != 0) {
            const message = 'Windows Subsystem for Linux is not installed (required WSL2 and Ubuntu-18.04 as distro)';
            vscode.window.showErrorMessage(message, 'WSL Installation Guide').then((value) => {
                if (value === 'WSL Installation Guide')
                    vscode.env.openExternal(vscode.Uri.parse('https://docs.microsoft.com/en-us/windows/wsl/wsl2-install'));
            });
        }
        else {
            const workspaceStatus = yield commands.execute('BuildScripts/check-workspace.sh');
            if (workspaceStatus != 0) {
                let message;
                if (workspaceStatus == 10)
                    message = 'Workspace is not created';
                else if (workspaceStatus == 11)
                    message = 'Workspace is outdated';
                else
                    message = 'Can\'t determine workspace state';
                vscode.window.showErrorMessage(message, 'Prepare Workspace').then((value) => {
                    if (value === 'Prepare Workspace')
                        vscode.commands.executeCommand('extension.prepareWorkspace');
                });
            }
        }
    });
}
//# sourceMappingURL=status.js.map