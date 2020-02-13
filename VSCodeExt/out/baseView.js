"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const htmlGenerator_1 = require("./htmlGenerator");
class BaseView {
    constructor(_context, _title) {
        this._context = _context;
        this._title = _title;
        this._panel = vscode.window.createWebviewPanel(this._title, this._title, vscode.ViewColumn.One, { enableScripts: true });
        this.refresh();
    }
    toFront() {
        this.refresh();
        this._panel.reveal();
    }
    refresh() {
        const wait = new htmlGenerator_1.HtmlGenerator(this._title);
        wait.addSpinner();
        this._panel.webview.html = wait.finalize(this._context, this._panel);
        const page = new htmlGenerator_1.HtmlGenerator(this._title);
        this.evaluate(page).then(() => {
            this._panel.webview.html = page.finalize(this._context, this._panel);
        });
    }
}
exports.BaseView = BaseView;
//# sourceMappingURL=baseView.js.map