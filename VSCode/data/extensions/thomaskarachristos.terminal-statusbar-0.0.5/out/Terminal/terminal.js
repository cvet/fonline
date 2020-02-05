"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const terminalStatusBarItem_1 = require("./terminalStatusBarItem");
const commandsNames_1 = require("../model/commandsNames");
const paths_utils_1 = require("../utils/paths.utils");
class Terminal {
    constructor() {
        this._terminals = {};
        this._counter_id = 1;
        this.registerStatusBarCommand = (context, id) => {
            let tripleClickCount = 0;
            let timoutDisposal = null;
            const command = vscode.commands.registerCommand(commandsNames_1.commandsNames.showTerminal + id, () => {
                if (tripleClickCount === -1) {
                    return;
                }
                if (tripleClickCount < 2) {
                    ++tripleClickCount;
                    if (timoutDisposal === null) {
                        this.singleClick(id);
                        timoutDisposal = setTimeout(() => {
                            tripleClickCount = 0;
                            timoutDisposal = null;
                        }, 650);
                    }
                }
                else {
                    clearTimeout(timoutDisposal);
                    tripleClickCount = -1;
                    timoutDisposal = null;
                    this.tripleClick(id);
                }
            });
            context.subscriptions.push(command);
            return command;
        };
        this.onDidCloseTerminal = (terminal) => {
            this._terminals[terminal.__id__].dispose();
            delete this._terminals[terminal.__id__];
        };
        this.concatTerminals = (context) => {
            const terminals = vscode.window.terminals;
            terminals.forEach((element) => {
                let id = element.__id__;
                if (!this._terminals[id]) {
                    id = this._counter_id++;
                    element.__id__ = id;
                    this._terminals[id] = new terminalStatusBarItem_1.TerminalStatusBarItem(element, this.registerStatusBarCommand(context, id));
                }
            });
        };
        this.openInTerminal = (context) => {
            vscode.commands.registerCommand(commandsNames_1.commandsNames.openInTerminal, this.openCurrentTerminal);
        };
    }
    singleClick(id) {
        this._terminals[id].show();
    }
    tripleClick(id) {
        vscode.commands.executeCommand(commandsNames_1.commandsNames.buildInTerminalKill);
    }
    openCurrentTerminal() {
        try {
            const terminal = vscode.window.createTerminal({
                cwd: paths_utils_1.getRealPath("current")
            });
            terminal.show();
        }
        catch (_a) {
            vscode.window.showInformationMessage('No file with focus found to open');
        }
    }
}
exports.terminals = new Terminal();
//# sourceMappingURL=terminal.js.map