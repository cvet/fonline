"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const commandsNames_1 = require("../model/commandsNames");
const actionTerminal_1 = require("./actionTerminal");
var actionsTerminalsNames;
(function (actionsTerminalsNames) {
    actionsTerminalsNames["root"] = "root";
    actionsTerminalsNames["current"] = "current";
    actionsTerminalsNames["edit"] = "edit";
})(actionsTerminalsNames || (actionsTerminalsNames = {}));
class ActionsStatusBar {
    constructor(context) {
        this.context = context;
        this.statusBars = {};
        this.createRootStatusBar = () => {
            const command = vscode.commands.registerCommand(commandsNames_1.commandsNames.createRootTerminal, () => {
                vscode.window.createTerminal("root").show();
            });
            this.context.subscriptions.push(command);
            this.statusBars[actionsTerminalsNames.root].StatusBar.text =
                "$(terminal) root";
            this.statusBars[actionsTerminalsNames.root].StatusBar.tooltip =
                "Create a new terimanl to the root.\nToggle visibility by pressing F1 and select 'Toggle visibility of root teriminal'";
            this.statusBars[actionsTerminalsNames.root].StatusBar.command =
                commandsNames_1.commandsNames.createRootTerminal;
            this.statusBars[actionsTerminalsNames.root].setVisibility(false);
        };
        this.createCurrentStatusBar = () => {
            this.statusBars[actionsTerminalsNames.current].StatusBar.text =
                "$(terminal) current";
            this.statusBars[actionsTerminalsNames.current].StatusBar.tooltip =
                "Create a new terimanl to the current folder.\nToggle visibility by pressing F1 and select 'Toggle visibility of current teriminal'";
            this.statusBars[actionsTerminalsNames.current].StatusBar.command =
                commandsNames_1.commandsNames.openInTerminal;
            this.statusBars[actionsTerminalsNames.current].setVisibility(false);
        };
        this.createEditStatusBar = () => {
            this.statusBars[actionsTerminalsNames.edit].StatusBar.text =
                "|$(settings)|";
            this.statusBars[actionsTerminalsNames.edit].StatusBar.tooltip =
                "Open json that is responsible about the custom terminal actions.\nToggle visibility by pressing F1 and select 'Toggle visibility of edit json button' ";
            this.statusBars[actionsTerminalsNames.edit].StatusBar.command =
                commandsNames_1.commandsNames.editConfigurationFile;
            this.statusBars[actionsTerminalsNames.edit].setVisibility(false);
        };
        this.init();
        this.registryTheCommands();
    }
    init() {
        this.statusBars[actionsTerminalsNames.root] = new actionTerminal_1.ActionTerminal(actionsTerminalsNames.root, this.context, 0);
        this.statusBars[actionsTerminalsNames.current] = new actionTerminal_1.ActionTerminal(actionsTerminalsNames.current, this.context, 0);
        this.statusBars[actionsTerminalsNames.edit] = new actionTerminal_1.ActionTerminal(actionsTerminalsNames.current, this.context, 99999, vscode.StatusBarAlignment.Right);
    }
    makeCommadForStatusBarBy(name) {
        this.statusBars[name].setVisibility(true);
    }
    registryTheCommands() {
        vscode.commands.registerCommand(commandsNames_1.commandsNames.ToggleRootTerminal, () => {
            this.makeCommadForStatusBarBy(actionsTerminalsNames.root);
        });
        vscode.commands.registerCommand(commandsNames_1.commandsNames.ToggleCurrentTerminal, () => {
            this.makeCommadForStatusBarBy(actionsTerminalsNames.current);
        });
        vscode.commands.registerCommand(commandsNames_1.commandsNames.ToogleEditStatusBar, () => {
            this.makeCommadForStatusBarBy(actionsTerminalsNames.edit);
        });
    }
}
exports.ActionsStatusBar = ActionsStatusBar;
//# sourceMappingURL=actionsStatusBar.js.map