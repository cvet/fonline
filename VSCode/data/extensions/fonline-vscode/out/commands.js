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
            terminal.command = 'extension.' + terminal.label.toLowerCase().replace(/ /g, '');
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
    }
    getChildren(element) {
        return element ? [] : this.terminals;
    }
    getTreeItem(element) {
        return {
            label: element.label,
            command: { command: element.command, title: element.label }
        };
    }
}
//# sourceMappingURL=commands.js.map