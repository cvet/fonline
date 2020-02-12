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
var dashboard = undefined;
function show(context) {
    context.subscriptions.push(vscode.commands.registerCommand('dashboard.show', () => {
        if (!dashboard) {
            dashboard = new DashboardView();
            dashboard.panel.onDidDispose(() => { dashboard = undefined; }, null, context.subscriptions);
        }
        else {
            dashboard.toFront();
        }
    }));
    vscode.commands.executeCommand('dashboard.show');
}
exports.show = show;
class DashboardView extends baseView_1.BaseView {
    constructor() {
        super('Dashboard');
        setInterval(() => { this.refresh(); }, 3 * 60000);
    }
    evaluate(html) {
        return __awaiter(this, void 0, void 0, function* () {
            const exitCode = yield commands.execute('BuildScripts/check-workspace.sh');
            if (exitCode == 0)
                html.addLeadText('Workspace is ready');
            else if (exitCode == 10)
                html.addLeadText('Workspace is not created');
            else if (exitCode == 11)
                html.addLeadText('Workspace is outdated');
            else
                html.addLeadText('Can\'t determine workspace state');
        });
    }
}
//# sourceMappingURL=dashboard.js.map