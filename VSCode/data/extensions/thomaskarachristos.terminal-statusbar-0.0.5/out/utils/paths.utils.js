"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
const modulePath = require("path");
exports.getRealPath = (path) => {
    const pathLowerCase = path.toLowerCase();
    if (pathLowerCase === "root") {
        return vscode.workspace.workspaceFolders[0].uri.fsPath;
    }
    if (pathLowerCase === "current") {
        return modulePath.dirname(getCurrentEditorPath().fsPath);
    }
    return path;
};
const getCurrentEditorPath = () => {
    const activeTextEditor = vscode.window.activeTextEditor;
    if (!activeTextEditor || activeTextEditor.document.isUntitled) {
        return;
    }
    return activeTextEditor.document.uri;
};
//# sourceMappingURL=paths.utils.js.map