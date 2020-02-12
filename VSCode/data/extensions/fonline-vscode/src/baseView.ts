import * as vscode from 'vscode';
import { HtmlGenerator } from './htmlGenerator';

export abstract class BaseView {
    public panel: vscode.WebviewPanel;

    abstract async evaluate(html: HtmlGenerator): Promise<void>;

    constructor(protected _title: string) {
        this.panel = vscode.window.createWebviewPanel(this._title, this._title,
            vscode.ViewColumn.One, { enableScripts: true });
        this.refresh();
    }

    toFront() {
        this.refresh();
        this.panel.reveal();
    }

    protected refresh() {
        const wait = new HtmlGenerator(this._title);
        wait.addSpinner();
        this.panel.webview.html = wait.finalize();

        const page = new HtmlGenerator(this._title);
        this.evaluate(page).then(() => {
            this.panel.webview.html = page.finalize();
        });
    }
}
