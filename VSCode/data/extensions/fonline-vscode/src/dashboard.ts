import * as vscode from 'vscode';
import { BaseView } from './view';

var dashboard: DashboardView | undefined = undefined;

export function show(context: vscode.ExtensionContext) {
    context.subscriptions.push(
        vscode.commands.registerCommand('dashboard.show', () => {
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

class DashboardView extends BaseView {
    constructor() {
        super('Dashboard');
    }

    getHtml(): string | undefined {
        return undefined;
    }
}
