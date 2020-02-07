"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
function activate(context) {
    context.subscriptions.push(vscode.commands.registerCommand('extension.node-debug2.toggleSkippingFile', toggleSkippingFile));
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('extensionHost', new ExtensionHostDebugConfigurationProvider()));
}
exports.activate = activate;
function deactivate() {
}
exports.deactivate = deactivate;
function toggleSkippingFile(path) {
    if (!path) {
        const activeEditor = vscode.window.activeTextEditor;
        path = activeEditor && activeEditor.document.fileName;
    }
    if (path && vscode.debug.activeDebugSession) {
        const args = typeof path === 'string' ? { path } : { sourceReference: path };
        vscode.debug.activeDebugSession.customRequest('toggleSkipFileStatus', args);
    }
}
class ExtensionHostDebugConfigurationProvider {
    resolveDebugConfiguration(_folder, debugConfiguration) {
        const useV3 = vscode.workspace.getConfiguration().get('debug.extensionHost.useV3', false)
            || vscode.workspace.getConfiguration().get('debug.javascript.usePreview', false);
        if (useV3) {
            debugConfiguration['__workspaceFolder'] = '${workspaceFolder}';
            debugConfiguration.type = 'pwa-extensionHost';
        }
        return debugConfiguration;
    }
}

//# sourceMappingURL=extension.js.map
