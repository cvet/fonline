import * as vscode from 'vscode';
import * as commands from './commands';
import { HtmlGenerator } from './htmlGenerator';

export function check(context: vscode.ExtensionContext) {
    checkReadiness().then((html: HtmlGenerator) => {
        if (!html.empty) {
            const panel = vscode.window.createWebviewPanel('Status', 'Status', vscode.ViewColumn.One, { enableScripts: true });
            panel.webview.html = html.finalize(context, panel);
        }
    });
}

async function checkReadiness(): Promise<HtmlGenerator> {
    const html = new HtmlGenerator('Status');
    if (await commands.execute('Check WSL') != 0) {
        html.addLeadText('Windows Subsystem for Linux not installed');
        html.addText('WSL version 2 required');
        html.addText('Ubuntu-18.04 distro required');
        html.addLink('Follow this instruction to install all this stuff', 'https://docs.microsoft.com/en-us/windows/wsl/wsl2-install');
    } else {
        const workspaceStatus = await commands.execute('BuildScripts/check-workspace.sh');
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
}
