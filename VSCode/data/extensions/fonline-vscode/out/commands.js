"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const fs = require("fs");
const path = require("path");
class TerminalManager {
    constructor(context) {
        this.context = context;
        const fileContent = fs.readFileSync(path.join(this.context.extensionPath, 'resources', 'commands.json'));
        let jsonTerminals = JSON.parse(fileContent.toString());
        let terminals = jsonTerminals.map((terminal) => {
            terminal.command = 'extension.run_' + terminal.label.toLowerCase().replace(/ /g, '');
            return terminal;
        });
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
exports.TerminalManager = TerminalManager;
class TerminalTree {
    constructor(terminals) {
        this.terminals = terminals;
        this.i = -1;
    }
    getChildren(element) {
        return this.terminals;
    }
    getTreeItem(element) {
        this.i++;
        let terminal = this.terminals[this.i];
        return {
            label: terminal.label,
            command: { command: terminal.command, title: terminal.label }
        };
    }
}
//# sourceMappingURL=commands.js.map