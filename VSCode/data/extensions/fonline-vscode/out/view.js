"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const htmlGenerator_1 = require("./htmlGenerator");
class BaseView {
    constructor(_title) {
        this._title = _title;
        this.panel = vscode.window.createWebviewPanel(this._title, this._title, vscode.ViewColumn.One, { enableScripts: true });
    }
    toFront() {
        this.panel.reveal();
    }
    render() {
        const html = this.getHtml();
        if (html === undefined) {
            const wait = new htmlGenerator_1.HtmlGenerator(this._title);
            wait.addSpinner();
            this.panel.webview.html = wait.finalize();
        }
        else {
            this.panel.webview.html = html;
        }
    }
}
exports.BaseView = BaseView;
//# sourceMappingURL=view.js.map