import * as vscode from 'vscode';
import * as path from 'path';

export class HtmlGenerator {
    private _content: string;

    constructor(title: string) {
        this._content = '';
    }

    get empty(): boolean {
        return this._content.length == 0;
    }

    addSpinner() {
        this._content += `<div uk-spinner="ratio: 3"></div>`;
    }

    addLeadText(text: string, centered: boolean = true) {
        this._content += `<div class="uk-text-lead ${centered ? "uk-text-center" : ""}">${text}</div>`;
    }

    addText(text: string, centered: boolean = true) {
        this._content += `<div class="uk-text-normal ${centered ? "uk-text-center" : ""}">${text}</div>`;
    }

    addLink(text: string, url: string, centered: boolean = true) {
        this._content += `<div class="${centered ? "uk-text-center" : ""}"><a class="uk-link-muted" href="${url}">${text}</a></div>`;
    }

    addList(inner: () => void) {
        this._content += `<dl class="uk-description-list">`;
        inner();
        this._content += `</dl>`;
    }

    addListEntry(title: string, message: string) {
        this._content += `<dt>${title}</dt><dd>${message}</dd>`;
    }

    finalize(context: vscode.ExtensionContext, panel: vscode.WebviewPanel): string {
        function makeUiKitPath(...args: string[]): vscode.Uri {
            const diskPath = path.join(context.extensionPath, 'node_modules', 'uikit', 'dist', ...args);
            return panel.webview.asWebviewUri(vscode.Uri.file(diskPath));
        }

        return `<!DOCTYPE html>
<html lang="en">
    <head>
        <title>Dashboard</title>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <link rel="stylesheet" href="${makeUiKitPath('css', 'uikit.min.css')}" />
        <link href="css/styles.css" rel="stylesheet">
    </head>
    <body>
        <script src="${makeUiKitPath('js', 'uikit.min.js')}"></script>
        <script src="${makeUiKitPath('js', 'uikit-icons.min.js')}"></script>
        <div class="uk-position-center">
            ${this._content}
        </div>
    </body>
</html>`;
    }
}
