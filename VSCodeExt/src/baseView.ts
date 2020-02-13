import * as vscode from 'vscode';
import { HtmlGenerator } from './htmlGenerator';

export abstract class BaseView {
    protected _panel: vscode.WebviewPanel;

    abstract async evaluate(html: HtmlGenerator): Promise<void>;

    constructor(protected _context: vscode.ExtensionContext, protected _title: string) {
        this._panel = vscode.window.createWebviewPanel(this._title, this._title,
            vscode.ViewColumn.One, { enableScripts: true });
        this.refresh();
    }

    toFront() {
        this.refresh();
        this._panel.reveal();
    }

    protected refresh() {
        const wait = new HtmlGenerator(this._title);
        wait.addSpinner();
        this._panel.webview.html = wait.finalize(this._context, this._panel);

        const page = new HtmlGenerator(this._title);
        this.evaluate(page).then(() => {
            this._panel.webview.html = page.finalize(this._context, this._panel);
        });
    }
}
