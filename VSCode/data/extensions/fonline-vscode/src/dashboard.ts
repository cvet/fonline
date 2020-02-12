import * as vscode from 'vscode';
import * as commands from './commands';
import { BaseView } from './baseView';
import { HtmlGenerator } from './htmlGenerator';

export function show(context: vscode.ExtensionContext) {
    context.subscriptions.push(
        vscode.commands.registerCommand('dashboard.show', () => {
            if (!DashboardView.instance) {
                new DashboardView(context);
            }
            else {
                DashboardView.instance.toFront();
            }
        }));
    vscode.commands.executeCommand('dashboard.show');
}

class DashboardView extends BaseView {
    static instance: DashboardView | undefined = undefined;

    constructor(protected _context: vscode.ExtensionContext) {
        super(_context, 'Dashboard');

        DashboardView.instance = this;
        this._panel.onDidDispose(() => { DashboardView.instance = undefined; }, null, this._context.subscriptions);

        setInterval(() => { this.refresh(); }, 3 * 60000);
    }

    async evaluate(html: HtmlGenerator): Promise<void> {
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
        const exitCode = await commands.execute('BuildScripts/check-workspace.sh');
        if (exitCode == 0)
            html.addLeadText('Workspace ready');
        else if (exitCode == 10)
            html.addLeadText('Workspace not created');
        else if (exitCode == 11)
            html.addLeadText('Workspace outdated');
        else
            html.addLeadText('Can\'t determine workspace state');
    }
}
