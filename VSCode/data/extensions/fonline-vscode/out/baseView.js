"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const htmlGenerator_1 = require("./htmlGenerator");
class BaseView {
    constructor(_title) {
        this._title = _title;
        this.panel = vscode.window.createWebviewPanel(this._title, this._title, vscode.ViewColumn.One, { enableScripts: true });
        this.refresh();
    }
    toFront() {
        this.refresh();
        this.panel.reveal();
    }
    refresh() {
        const wait = new htmlGenerator_1.HtmlGenerator(this._title);
        wait.addSpinner();
        this.panel.webview.html = wait.finalize();
        const page = new htmlGenerator_1.HtmlGenerator(this._title);
        this.evaluate(page).then(() => {
            this.panel.webview.html = page.finalize();
        });
    }
}
exports.BaseView = BaseView;
//# sourceMappingURL=baseView.js.map