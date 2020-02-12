"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const view_1 = require("./view");
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
        dashboard.render();
    }));
    vscode.commands.executeCommand('dashboard.show');
}
exports.show = show;
class DashboardView extends view_1.BaseView {
    constructor() {
        super('Dashboard');
    }
    getHtml() {
        return undefined;
    }
}
//# sourceMappingURL=dashboard.js.map