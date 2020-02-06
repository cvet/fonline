"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const vscode_1 = require("vscode");
class DirectoryTreeItem extends vscode_1.TreeItem {
    constructor(data) {
        super(data.label, vscode_1.TreeItemCollapsibleState.Collapsed);
        this.data = data;
        this.contextValue = 'folder';
    }
}
exports.DirectoryTreeItem = DirectoryTreeItem;
class FileTreeItem extends vscode_1.TreeItem {
    constructor(data) {
        super(data.label, vscode_1.TreeItemCollapsibleState.None);
        this.data = data;
    }
}
exports.FileTreeItem = FileTreeItem;
//# sourceMappingURL=treeNodes.js.map