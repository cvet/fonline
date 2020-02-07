import * as vscode from 'vscode';
import * as fs from 'fs';
import * as path from 'path';
import * as process from 'process';

interface TerminalEntry {
    label: string;
    tooltip: string;
    command: string;
}

export class TerminalManager {
    constructor(private context: vscode.ExtensionContext) {
        const fileContent = fs.readFileSync(path.join(this.context.extensionPath, 'resources', 'commands.json'));
        let jsonTerminals = JSON.parse(fileContent.toString());

        let terminals = jsonTerminals.map((terminal: { command: string; label: string; }) => {
            terminal.command = 'extension.run_' + terminal.label.toLowerCase().replace(/ /g, '');
            return terminal;
        })

        let tree = vscode.window.createTreeView('terminalManager', { treeDataProvider: new TerminalTree(terminals) });
        this.context.subscriptions.push(tree);

        for (let terminal of terminals) {
            let cmd = vscode.commands.registerCommand(terminal.command, function () {
                let terminalInstance = vscode.window.createTerminal(terminal.label, terminal.shellPath, terminal.shellArgs);
                terminalInstance.show();
            });
            this.context.subscriptions.push(cmd);
        }
    }
}

class TerminalTree implements vscode.TreeDataProvider<TerminalEntry> {
    private i: number = -1;

    constructor(private terminals: TerminalEntry[]) {
    }

    getChildren(element: TerminalEntry): TerminalEntry[] {
        return this.terminals;
    }

    getTreeItem(element: TerminalEntry): vscode.TreeItem {
        this.i++;
        let terminal = this.terminals[this.i];
        return {
            label: terminal.label,
            command: { command: terminal.command, title: terminal.label }
        }
    }
}
