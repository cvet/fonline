import * as vscode from 'vscode';
import * as fs from 'fs';
import * as path from 'path';
import * as process from 'process';

const wslShellPath: string = 'C:\\Windows\\System32\\wsl.exe';

interface TerminalEntry {
    label: string;
    tooltip: string;
    command: string;
}

export class TerminalManager {
    constructor(private context: vscode.ExtensionContext) {
        const fileContent = fs.readFileSync(path.join(this.context.extensionPath, 'resources', 'commands.json'));
        let jsonTerminals = JSON.parse(fileContent.toString());

        let terminals = jsonTerminals.map((terminal: any) => {
            terminal.shellArgs = undefined;
            if (terminal.command) {
                terminal.shellArgs = `export FO_INSTALL_PACKAGES=0; ${terminal.command}; read -p "Press enter to continue"`;
            }
            terminal.command = 'extension.' + terminal.label.toLowerCase().replace(/ /g, '');
            return terminal;
        })

        let tree = vscode.window.createTreeView('terminalManager', { treeDataProvider: new TerminalTree(terminals) });
        this.context.subscriptions.push(tree);

        for (let terminal of terminals) {
            let cmd = vscode.commands.registerCommand(terminal.command, function () {
                let terminalInstance = vscode.window.createTerminal(terminal.label, wslShellPath, terminal.shellArgs);
                terminalInstance.show();
            });
            this.context.subscriptions.push(cmd);
        }
    }
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
