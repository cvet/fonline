"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode_1 = require("vscode");
const vscode = require("vscode");
const commandsNames_1 = require("../model/commandsNames");
const paths_utils_1 = require("../utils/paths.utils");
class CustomActions {
    constructor() {
        this.customStatusBars = [];
        this.commands = {};
        this.registerStatusBarCommand = (name, path, commandToRun) => {
            if (!this.commands[this.calculateTheCommandName(name)]) {
                vscode.commands.registerCommand(this.calculateTheCommandName(name), () => {
                    this.commands[this.calculateTheCommandName(name)]();
                });
            }
            this.makecommand(name, path, commandToRun);
        };
    }
    calculateTheCommandName(name) {
        return commandsNames_1.commandsNames.runInTerminal + name;
    }
    removeAll() {
        this.customStatusBars.forEach((actionsInfo) => {
            actionsInfo.dispose();
        });
    }
    add(customActionInfo) {
        const newAction = vscode_1.window.createStatusBarItem(vscode_1.StatusBarAlignment.Right, 99999999);
        newAction.text = `$(terminal) ${customActionInfo.name}`;
        newAction.tooltip = `path: ${customActionInfo.path} command: ${customActionInfo.command}`;
        newAction.command = this.calculateTheCommandName(customActionInfo.name);
        this.registerStatusBarCommand(customActionInfo.name, customActionInfo.path, customActionInfo.command);
        newAction.show();
        this.customStatusBars.push(newAction);
    }
    makecommand(name, path, commandToRun) {
        this.commands[this.calculateTheCommandName(name)] = this.callbackCommand.bind(this, name, path, commandToRun);
    }
    callbackCommand(name, path, commandToRun) {
        path = paths_utils_1.getRealPath(path);
        const terminal = vscode.window.createTerminal({
            name: name,
            cwd: path
        });
        terminal.sendText(commandToRun);
        terminal.show();
    }
}
exports.customActions = new CustomActions();
//# sourceMappingURL=customActions.js.map