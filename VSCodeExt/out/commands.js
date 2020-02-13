"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const fs = require("fs");
const path = require("path");
const wslShellPath = 'C:\\Windows\\System32\\wsl.exe';
function init(context) {
    const fileContent = fs.readFileSync(path.join(context.extensionPath, 'resources', 'commands.json'));
    const jsonTerminals = JSON.parse(fileContent.toString());
    const terminals = jsonTerminals.map((terminal) => {
        if (terminal.command) {
            terminal.shellArgs = `export FO_INSTALL_PACKAGES=0; ${terminal.command}; read -p "Press enter to close terminal..."`;
        }
        terminal.command = 'extension.' + terminal.label.substr(0, 1).toLowerCase() + terminal.label.replace(/ /g, '').substr(1);
        return terminal;
    });
    const tree = vscode.window.createTreeView('terminalManager', { treeDataProvider: new TerminalTree(terminals) });
    context.subscriptions.push(tree);
    for (const terminal of terminals) {
        const cmd = vscode.commands.registerCommand(terminal.command, function () {
            const terminalInstance = vscode.window.createTerminal(terminal.label, wslShellPath, terminal.shellArgs);
            terminalInstance.show();
        });
        context.subscriptions.push(cmd);
    }
}
exports.init = init;
function execute(label, ...shellArgs) {
    var _a;
    return __awaiter(this, void 0, void 0, function* () {
        const terminal = vscode.window.createTerminal({
            "hideFromUser": true,
            "name": label,
            "shellPath": wslShellPath,
            "shellArgs": shellArgs.concat('exit')
        });
        function sleep(ms) {
            return new Promise(resolve => setTimeout(resolve, ms));
        }
        while (terminal.exitStatus === undefined)
            yield sleep(5);
        return _a = terminal.exitStatus.code, (_a !== null && _a !== void 0 ? _a : -1);
    });
}
exports.execute = execute;
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