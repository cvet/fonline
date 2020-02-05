"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const terminal_1 = require("./terminal");
function activate(context) {
    vscode.window.onDidOpenTerminal(terminal => {
        terminal_1.terminals.concatTerminals(context);
    });
    vscode.window.onDidOpenTerminal((terminal) => {
        terminal_1.terminals.concatTerminals(context);
    });
    vscode.window.onDidChangeActiveTerminal(e => {
        terminal_1.terminals.concatTerminals(context);
    });
    context.subscriptions.push(vscode.window.onDidCloseTerminal(terminal_1.terminals.onDidCloseTerminal));
    terminal_1.terminals.concatTerminals(context);
    terminal_1.terminals.openInTerminal(context);
}
exports.default = activate;
//# sourceMappingURL=terminalCommands.js.map