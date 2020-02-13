"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const path = require("path");
class HtmlGenerator {
    constructor(title) {
        this._content = '';
    }
    get empty() {
        return this._content.length == 0;
    }
    addSpinner() {
        this._content += `<div uk-spinner="ratio: 3"></div>`;
    }
    addLeadText(text, centered = true) {
        this._content += `<div class="uk-text-lead ${centered ? "uk-text-center" : ""}">${text}</div>`;
    }
    addText(text, centered = true) {
        this._content += `<div class="uk-text-normal ${centered ? "uk-text-center" : ""}">${text}</div>`;
    }
    addLink(text, url, centered = true) {
        this._content += `<div class="${centered ? "uk-text-center" : ""}"><a class="uk-link-muted" href="${url}">${text}</a></div>`;
    }
    addList(inner) {
        this._content += `<dl class="uk-description-list">`;
        inner();
        this._content += `</dl>`;
    }
    addListEntry(title, message) {
        this._content += `<dt>${title}</dt><dd>${message}</dd>`;
    }
    finalize(context, panel) {
        function makeUiKitPath(...args) {
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
exports.HtmlGenerator = HtmlGenerator;
//# sourceMappingURL=htmlGenerator.js.map