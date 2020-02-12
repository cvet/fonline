import * as vscode from 'vscode';
import * as fs from 'fs';
import * as path from 'path';

const wslShellPath: string = 'C:\\Windows\\System32\\wsl.exe';

interface TerminalEntry {
    label: string;
    tooltip: string;
    command: string;
}

export function init(context: vscode.ExtensionContext) {
    const fileContent = fs.readFileSync(path.join(context.extensionPath, 'resources', 'commands.json'));
    let jsonTerminals = JSON.parse(fileContent.toString());

    let terminals = jsonTerminals.map((terminal: { label: string, command: string, shellArgs: string }) => {
        if (terminal.command) {
            terminal.shellArgs = `export FO_INSTALL_PACKAGES=0; ${terminal.command}; read -p "Press enter to close terminal..."`;
        }
        terminal.command = 'extension.' + terminal.label.toLowerCase().replace(/ /g, '');
        return terminal;
    });

    let tree = vscode.window.createTreeView('terminalManager', { treeDataProvider: new TerminalTree(terminals) });
    context.subscriptions.push(tree);

    for (let terminal of terminals) {
        let cmd = vscode.commands.registerCommand(terminal.command, function () {
            let terminalInstance = vscode.window.createTerminal(terminal.label, wslShellPath, terminal.shellArgs);
            terminalInstance.show();
        });
        context.subscriptions.push(cmd);
    }
}

export async function execute(label: string, ...shellArgs: string[]): Promise<number> {
    shellArgs.push('exit');
    let terminal = vscode.window.createTerminal({ "hideFromUser": true, "name": label, "shellPath": wslShellPath, "shellArgs": shellArgs });

    function sleep(ms: number) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
    while (terminal.exitStatus === undefined)
        await sleep(5);

    return terminal.exitStatus.code ?? -1;
}

class TerminalTree implements vscode.TreeDataProvider<TerminalEntry> {
    constructor(private terminals: TerminalEntry[]) {
    }

    getChildren(element?: TerminalEntry): TerminalEntry[] {
        return element ? [] : this.terminals;
    }

    getTreeItem(element: TerminalEntry): vscode.TreeItem {
        return {
            label: element.label,
            command: { command: element.command, title: element.label }
        }
    }
}
