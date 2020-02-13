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
const htmlGenerator_1 = require("./htmlGenerator");
function check(context) {
    checkReadiness().then((html) => {
        if (!html.empty) {
            const panel = vscode.window.createWebviewPanel('Status', 'Status', vscode.ViewColumn.One, { enableScripts: true });
            panel.webview.html = html.finalize();
        }
    });
}
exports.check = check;
function checkReadiness() {
    return __awaiter(this, void 0, void 0, function* () {
        const html = new htmlGenerator_1.HtmlGenerator('Status');
        if ((yield commands.execute('Check WSL')) !== 0) {
            html.addLeadText('Windows Subsystem for Linux not installed');
            html.addText('WSL version 2 required');
            html.addText('Ubuntu-18.04 distro required');
            html.addLink('Follow this instruction to install all this stuff', 'https://docs.microsoft.com/en-us/windows/wsl/wsl2-install');
        }
        else {
            const workspaceStatus = yield commands.execute('BuildTools/check-workspace.sh');
            if (workspaceStatus != 0) {
                if (workspaceStatus == 10)
                    html.addLeadText('Workspace not created');
                else if (workspaceStatus == 11)
                    html.addLeadText('Workspace outdated');
                else
                    html.addLeadText('Can\'t determine workspace state');
                html.addText('Run command "Prepare Workspace"');
            }
        }
        return html;
    });
}
//# sourceMappingURL=dashboard.js.map