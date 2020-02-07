"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const path = require("path");
const vscode_1 = require("vscode");
class AbstractCommitNode {
    constructor(data) {
        this.data = data;
        this.children = [];
    }
    get label() {
        return this._label;
    }
    get resource() {
        return this._resource;
    }
    setLabel(value) {
        this._label = value;
    }
    setResoruce(value) {
        this._resource = typeof value === 'string' ? vscode_1.Uri.file(value) : value;
    }
}
exports.AbstractCommitNode = AbstractCommitNode;
class DirectoryNode extends AbstractCommitNode {
    constructor(commit, relativePath) {
        super(commit);
        const folderName = path.basename(relativePath);
        this.setLabel(folderName);
        this.setResoruce(relativePath);
    }
}
exports.DirectoryNode = DirectoryNode;
class FileNode extends AbstractCommitNode {
    constructor(commit) {
        super(commit);
        const fileName = path.basename(commit.committedFile.relativePath);
        this.setLabel(fileName);
        this.setResoruce(commit.committedFile.relativePath);
    }
}
exports.FileNode = FileNode;
exports.INodeFactory = Symbol('INodeFactory');
exports.INodeBuilder = Symbol('INodeBuilder');
//# sourceMappingURL=types.js.map