import * as vscode from 'vscode';
import * as commands from './commands';
import { BaseView } from './baseView';
import { HtmlGenerator } from './htmlGenerator';

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
        }));
    vscode.commands.executeCommand('dashboard.show');
}

class DashboardView extends BaseView {
    constructor() {
        super('Dashboard');

        setInterval(() => { this.refresh(); }, 3 * 60000);
    }

    async evaluate(html: HtmlGenerator): Promise<void> {
        const exitCode = await commands.execute('BuildScripts/check-workspace.sh');

        if (exitCode == 0)
            html.addLeadText('Workspace is ready');
        else if (exitCode == 10)
            html.addLeadText('Workspace is not created');
        else if (exitCode == 11)
            html.addLeadText('Workspace is outdated');
        else
            html.addLeadText('Can\'t determine workspace state');
    }
}
