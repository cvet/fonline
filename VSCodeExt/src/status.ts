import * as vscode from 'vscode';
import * as commands from './commands';

export function check(context: vscode.ExtensionContext) {
    checkReadiness();
}

async function checkReadiness(): Promise<void> {
    if (await commands.execute('Check WSL') != 0) {
        const message = 'Windows Subsystem for Linux is not installed (required WSL2 and Ubuntu-18.04 as distro)';
        vscode.window.showErrorMessage(message, 'WSL Installation Guide').then((value?: string) => {
            if (value === 'WSL Installation Guide')
                vscode.env.openExternal(vscode.Uri.parse('https://docs.microsoft.com/en-us/windows/wsl/wsl2-install'));
        });
    } else {
        const workspaceStatus = await commands.execute('BuildTools/check-workspace.sh');
        if (workspaceStatus != 0) {
            let message: string;
            if (workspaceStatus == 10)
                message = 'Workspace is not created';
            else if (workspaceStatus == 11)
                message = 'Workspace is outdated';
            else
                message = 'Can\'t determine workspace state';

            vscode.window.showErrorMessage(message, 'Prepare Workspace').then((value?: string) => {
                if (value === 'Prepare Workspace')
                    vscode.commands.executeCommand('extension.prepareWorkspace');
            });
        }
    }
}
