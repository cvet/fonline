import * as vscode from 'vscode';
import { HtmlGenerator } from './htmlGenerator';

export abstract class BaseView {
    public panel: vscode.WebviewPanel;

    constructor(protected _title: string) {
        this.panel = vscode.window.createWebviewPanel(this._title, this._title,
            vscode.ViewColumn.One, { enableScripts: true });
    }

    toFront() {
        this.panel.reveal();
    }

    render() {
        const html = this.getHtml();
        if (html === undefined) {
            const wait = new HtmlGenerator(this._title);
            wait.addSpinner();
            this.panel.webview.html = wait.finalize();
        }
        else {
            this.panel.webview.html = html;
        }
    }

    abstract getHtml(): string | undefined;
}
