"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const commandsNames_1 = require("../model/commandsNames");
class TerminalStatusBarItem {
    constructor(terminal, reg) {
        this._statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, -1);
        this.setupStatusBarItem(terminal.name, terminal.__id__);
        this.setupTerminalItem(terminal);
        this._registerCommand = reg;
    }
    show() {
        this._terminal.show();
    }
    setupStatusBarItem(name, id) {
        this._statusBarItem.text = `$(terminal) ${id}`;
        this._statusBarItem.tooltip = "Open the terminal";
        this._statusBarItem.command = commandsNames_1.commandsNames.showTerminal + id;
        this._statusBarItem.show();
    }
    setupTerminalItem(terminal) {
        this._terminal = terminal;
    }
    dispose() {
        //TODO disposal reguister commands
        this._statusBarItem.dispose();
        this._terminal.dispose();
    }
}
exports.TerminalStatusBarItem = TerminalStatusBarItem;
//# sourceMappingURL=terminalStatusBarItem.js.map