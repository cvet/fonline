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
const vscode_1 = require("vscode");
const commandManager_1 = require("../application/types/commandManager");
const types_1 = require("../formatters/types");
const treeNodes_1 = require("../nodes/treeNodes");
const types_2 = require("../nodes/types");
const types_3 = require("../types");
let CommitViewer = class CommitViewer {
    constructor(outputChannel, commitFormatter, commandManager, nodeBuilder, treeId, visibilityContextVariable) {
        this.outputChannel = outputChannel;
        this.commitFormatter = commitFormatter;
        this.commandManager = commandManager;
        this.nodeBuilder = nodeBuilder;
        this.treeId = treeId;
        this.visibilityContextVariable = visibilityContextVariable;
        this.registered = false;
        this._onDidChangeTreeData = new vscode_1.EventEmitter();
        this.fileView = false;
    }
    get onDidChangeTreeData() {
        return this._onDidChangeTreeData.event;
    }
    get selectedCommit() {
        return this.commit;
    }
    showCommitTree(commit) {
        this.commit = commit;
        if (!this.registered) {
            this.registered = true;
            vscode_1.window.registerTreeDataProvider(this.treeId, this);
        }
        this._onDidChangeTreeData.fire();
        this.commandManager.executeCommand('setContext', this.visibilityContextVariable, true);
    }
    showCommit(commit) {
        const output = this.commitFormatter.format(commit.logEntry);
        this.outputChannel.appendLine(output);
        this.outputChannel.show();
    }
    showFilesView() {
        this.fileView = true;
        this._onDidChangeTreeData.fire();
    }
    showFolderView() {
        this.fileView = false;
        this._onDidChangeTreeData.fire();
    }
    getTreeItem(element) {
        return __awaiter(this, void 0, void 0, function* () {
            const treeItem = yield this.nodeBuilder.getTreeItem(element);
            if (treeItem instanceof treeNodes_1.DirectoryTreeItem) {
                treeItem.collapsibleState = vscode_1.TreeItemCollapsibleState.Expanded;
            }
            if (this.fileView) {
                const fileDirectory = path.dirname(element.resource.fsPath);
                const isEmptyPath = fileDirectory === path.sep;
                treeItem.label = `${element.label} ${isEmptyPath ? '' : '•'} ${isEmptyPath ? '' : fileDirectory}`.trim();
            }
            return treeItem;
        });
    }
    getChildren(element) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!element) {
                // tslint:disable-next-line:no-suspicious-comment
                // TODO: HACK
                const committedFiles = this.treeId === 'commitViewProvider' ? this.commit.logEntry.committedFiles : this.commit.committedFiles;
                return this.fileView ? this.nodeBuilder.buildList(this.commit, committedFiles) : this.nodeBuilder.buildTree(this.commit, committedFiles);
            }
            if (element instanceof types_2.DirectoryNode) {
                return element.children;
            }
            return [];
        });
    }
};
CommitViewer = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_3.IOutputChannel)),
    __param(1, inversify_1.inject(types_1.ICommitViewFormatter)),
    __param(2, inversify_1.inject(commandManager_1.ICommandManager)),
    __metadata("design:paramtypes", [Object, Object, Object, Object, String, String])
], CommitViewer);
exports.CommitViewer = CommitViewer;
//# sourceMappingURL=commitViewer.js.map