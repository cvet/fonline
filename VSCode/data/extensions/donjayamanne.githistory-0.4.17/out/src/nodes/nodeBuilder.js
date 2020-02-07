"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
var __metadata = (this && this.__metadata) || function (k, v) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(k, v);
};
var __param = (this && this.__param) || function (paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
};
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const path = require("path");
const types_1 = require("../commandFactories/types");
const types_2 = require("../platform/types");
const types_3 = require("../types");
const nodeIcons_1 = require("./nodeIcons");
const treeNodes_1 = require("./treeNodes");
const types_4 = require("./types");
let NodeBuilder = class NodeBuilder {
    constructor(fileCommandFactory, nodeFactory, platform) {
        this.fileCommandFactory = fileCommandFactory;
        this.nodeFactory = nodeFactory;
        this.platform = platform;
    }
    buildTree(commit, committedFiles) {
        const sortedFiles = committedFiles.sort((a, b) => a.uri.fsPath.toUpperCase() > b.uri.fsPath.toUpperCase() ? 1 : -1);
        const commitFileDetails = new Map();
        sortedFiles.forEach(item => commitFileDetails.set(item.uri.fsPath, item));
        const nodes = new Map();
        const roots = [];
        sortedFiles.forEach(file => {
            const dirName = path.dirname(file.relativePath);
            const dirNode = dirName.split(path.sep).reduce((parent, folderName, index) => {
                if (folderName === '.' && index === 0) {
                    return undefined;
                }
                const currentPath = parent ? path.join(parent.resource.fsPath, folderName) : folderName;
                const nodeExists = nodes.has(currentPath);
                if (nodes.has(currentPath)) {
                    return nodes.get(currentPath);
                }
                const folderNode = this.nodeFactory.createDirectoryNode(commit, currentPath);
                nodes.set(currentPath, folderNode);
                if (parent) {
                    parent.children.push(folderNode);
                }
                if (index === 0 && !nodeExists) {
                    roots.push(folderNode);
                }
                return folderNode;
                // tslint:disable-next-line:no-any
            }, undefined);
            const fileNode = this.nodeFactory.createFileNode(commit, file);
            if (dirNode) {
                dirNode.children.push(fileNode);
            }
            else {
                roots.push(fileNode);
            }
        });
        nodes.forEach(node => node.children = this.sortNodes(node.children));
        return this.sortNodes(roots);
    }
    buildList(commit, committedFiles) {
        const nodes = committedFiles.map(file => this.nodeFactory.createFileNode(commit, file));
        return this.sortFileNodes(nodes);
    }
    getTreeItem(element) {
        return __awaiter(this, void 0, void 0, function* () {
            if (element instanceof types_4.DirectoryNode) {
                return this.buildDirectoryTreeItem(element);
            }
            else {
                return this.buildFileTreeItem(element);
            }
        });
    }
    buildDirectoryTreeItem(element) {
        const treeItem = new treeNodes_1.DirectoryTreeItem(element);
        treeItem.iconPath = nodeIcons_1.FolderIcon;
        treeItem.contextValue = 'folder';
        if (treeItem.command) {
            treeItem.command.tooltip = 'sdafasfef.';
        }
        return treeItem;
    }
    buildFileTreeItem(element) {
        return __awaiter(this, void 0, void 0, function* () {
            const treeItem = new treeNodes_1.FileTreeItem(element);
            switch (element.data.committedFile.status) {
                case types_3.Status.Added: {
                    treeItem.iconPath = nodeIcons_1.AddedIcon;
                    break;
                }
                case types_3.Status.Modified: {
                    treeItem.iconPath = nodeIcons_1.ModifiedIcon;
                    break;
                }
                case types_3.Status.Deleted: {
                    treeItem.iconPath = nodeIcons_1.RemovedIcon;
                    break;
                }
                case types_3.Status.Renamed: {
                    treeItem.iconPath = nodeIcons_1.RemovedIcon;
                    break;
                }
                default: {
                    treeItem.iconPath = nodeIcons_1.FileIcon;
                }
            }
            treeItem.contextValue = 'file';
            treeItem.command = yield this.fileCommandFactory.getDefaultFileCommand(element.data);
            return treeItem;
        });
    }
    sortNodes(nodes) {
        let directoryNodes = nodes
            .filter(node => node instanceof types_4.DirectoryNode)
            .map(node => node);
        let fileNodes = nodes
            .filter(node => node instanceof types_4.FileNode)
            .map(node => node);
        if (this.platform.isWindows) {
            directoryNodes = directoryNodes.sort((a, b) => a.label.toUpperCase() > b.label.toUpperCase() ? 1 : -1);
            fileNodes = fileNodes.sort((a, b) => a.label.toUpperCase() > b.label.toUpperCase() ? 1 : -1);
        }
        else {
            directoryNodes = directoryNodes.sort((a, b) => a.label > b.label ? 1 : -1);
            fileNodes = fileNodes.sort((a, b) => a.label > b.label ? 1 : -1);
        }
        return this.sortDirectoryNodes(directoryNodes).concat(this.sortFileNodes(fileNodes));
    }
    sortDirectoryNodes(nodes) {
        const directoryNodes = nodes
            .filter(node => node instanceof types_4.DirectoryNode)
            .map(node => node);
        if (this.platform.isWindows) {
            return directoryNodes.sort((a, b) => a.label.toUpperCase() > b.label.toUpperCase() ? 1 : -1);
        }
        else {
            return directoryNodes.sort((a, b) => a.label > b.label ? 1 : -1);
        }
    }
    sortFileNodes(nodes) {
        const fileNodes = nodes
            .filter(node => node instanceof types_4.FileNode)
            .map(node => node);
        if (this.platform.isWindows) {
            return fileNodes.sort((a, b) => a.label.toUpperCase() > b.label.toUpperCase() ? 1 : -1);
        }
        else {
            return fileNodes.sort((a, b) => a.label > b.label ? 1 : -1);
        }
    }
};
NodeBuilder = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_1.IFileCommitCommandFactory)),
    __param(2, inversify_1.inject(types_2.IPlatformService)),
    __metadata("design:paramtypes", [Object, Object, Object])
], NodeBuilder);
exports.NodeBuilder = NodeBuilder;
//# sourceMappingURL=nodeBuilder.js.map