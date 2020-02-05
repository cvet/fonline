"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
class ActionsStatusBar {
    constructor() {
        this.rootStatusBar = vscode.window.createStatusBarItem();
        this.currentStatusBar = vscode.window.createStatusBarItem();
        this.createRootStatusBar = (context) => {
            const command = vscode.commands.registerCommand(`createRootTerminal`, () => {
                vscode.window.createTerminal("root").show();
            });
            context.subscriptions.push(command);
            this.rootStatusBar.text = "$(terminal) root";
            this.rootStatusBar.tooltip = "create a new terimanl to the root";
            this.rootStatusBar.command = `createRootTerminal`;
            this.rootStatusBar.show();
        };
        this.createCurrentStatusBar = (context) => {
            this.currentStatusBar.text = "$(terminal) current";
            this.currentStatusBar.tooltip = "create a new terimanl to the currentFolder";
            this.currentStatusBar.command = `TerminalStatusBar.OpenInTerminal`;
            this.currentStatusBar.show();
        };
    }
}
exports.actionsStatusBar = new ActionsStatusBar();
//# sourceMappingURL=actionsStatusBar.js.map