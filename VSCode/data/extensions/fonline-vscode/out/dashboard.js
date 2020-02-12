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
const baseView_1 = require("./baseView");
function show(context) {
    context.subscriptions.push(vscode.commands.registerCommand('dashboard.show', () => {
        if (!DashboardView.instance) {
            new DashboardView(context);
        }
        else {
            DashboardView.instance.toFront();
        }
    }));
    vscode.commands.executeCommand('dashboard.show');
}
exports.show = show;
class DashboardView extends baseView_1.BaseView {
    constructor(_context) {
        super(_context, 'Dashboard');
        this._context = _context;
        DashboardView.instance = this;
        this._panel.onDidDispose(() => { DashboardView.instance = undefined; }, null, this._context.subscriptions);
        setInterval(() => { this.refresh(); }, 3 * 60000);
    }
    evaluate(html) {
        return __awaiter(this, void 0, void 0, function* () {
            // Check Windows host
            // ...
            // Check Windows version (at least 18917 when WSL2 was improved)
            // ...
            // Check WSL in general
            // ...
            // Check WSL version (target 2)
            // ...
            // Check WSL distro (target Ununtu-18.04)
            // ...
            // Check workspace
            const exitCode = yield commands.execute('BuildScripts/check-workspace.sh');
            if (exitCode == 0)
                html.addLeadText('Workspace ready');
            else if (exitCode == 10)
                html.addLeadText('Workspace not created');
            else if (exitCode == 11)
                html.addLeadText('Workspace outdated');
            else
                html.addLeadText('Can\'t determine workspace state');
        });
    }
}
DashboardView.instance = undefined;
//# sourceMappingURL=dashboard.js.map