"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
class ActionTerminal {
    constructor(name, context, priority, barAlignment = vscode.StatusBarAlignment.Left) {
        this.name = name;
        this.context = context;
        this.StatusBar = vscode.window.createStatusBarItem(barAlignment, priority);
        this.visibility = this.getGlobalStateOrTrue(name);
    }
    getGlobalStateOrTrue(name) {
        return this.context.globalState.get(name, true);
    }
    setVisibility(toogle = false) {
        if (toogle) {
            this.visibility = !this.visibility;
        }
        if (this.visibility) {
            this.StatusBar.show();
        }
        else {
            this.StatusBar.hide();
        }
        this.updateGlobalState(this.name, this.visibility);
    }
    updateGlobalState(name, value) {
        this.context.globalState.update(name, value);
    }
}
exports.ActionTerminal = ActionTerminal;
//# sourceMappingURL=actionTerminal.js.map