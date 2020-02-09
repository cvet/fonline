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
const types_1 = require("../application/types");
const disposableRegistry_1 = require("../application/types/disposableRegistry");
const types_2 = require("../common/types");
const constants_1 = require("../constants");
const types_3 = require("../ioc/types");
const types_4 = require("../nodes/types");
const types_5 = require("../server/types");
const types_6 = require("../types");
const registration_1 = require("./registration");
const types_7 = require("./types");
let GitHistoryCommandHandler = class GitHistoryCommandHandler {
    constructor(serviceContainer, disposableRegistry, commandManager) {
        this.serviceContainer = serviceContainer;
        this.disposableRegistry = disposableRegistry;
        this.commandManager = commandManager;
    }
    get server() {
        if (!this._server) {
            this._server = this.serviceContainer.get(types_5.IServerHost);
            this.disposableRegistry.register(this._server);
        }
        return this._server;
    }
    viewFileHistory(info) {
        return __awaiter(this, void 0, void 0, function* () {
            let fileUri;
            if (info) {
                if (info instanceof types_2.FileCommitDetails) {
                    const committedFile = info.committedFile;
                    fileUri = committedFile.uri ? vscode_1.Uri.file(committedFile.uri.fsPath) : vscode_1.Uri.file(committedFile.oldUri.fsPath);
                }
                else if (info instanceof types_4.FileNode) {
                    const committedFile = info.data.committedFile;
                    fileUri = committedFile.uri ? vscode_1.Uri.file(committedFile.uri.fsPath) : vscode_1.Uri.file(committedFile.oldUri.fsPath);
                }
                else if (info instanceof vscode_1.Uri) {
                    fileUri = info;
                    // tslint:disable-next-line:no-any
                }
                else if (info.resourceUri) {
                    // tslint:disable-next-line:no-any
                    fileUri = info.resourceUri;
                }
            }
            else {
                const activeTextEditor = vscode_1.window.activeTextEditor;
                if (!activeTextEditor || activeTextEditor.document.isUntitled) {
                    return;
                }
                fileUri = activeTextEditor.document.uri;
            }
            return this.viewHistory(fileUri);
        });
    }
    viewLineHistory() {
        return __awaiter(this, void 0, void 0, function* () {
            let fileUri;
            const activeTextEditor = vscode_1.window.activeTextEditor;
            if (!activeTextEditor || activeTextEditor.document.isUntitled) {
                return;
            }
            fileUri = activeTextEditor.document.uri;
            const currentLineNumber = activeTextEditor.selection.start.line + 1;
            return this.viewHistory(fileUri, currentLineNumber);
        });
    }
    viewBranchHistory() {
        return __awaiter(this, void 0, void 0, function* () {
            return this.viewHistory();
        });
    }
    viewHistory(fileUri, lineNumber) {
        return __awaiter(this, void 0, void 0, function* () {
            const gitServiceFactory = this.serviceContainer.get(types_6.IGitServiceFactory);
            const gitService = yield gitServiceFactory.createGitService(fileUri);
            const branchName = yield gitService.getCurrentBranch();
            const gitRoot = yield gitService.getGitRoot();
            const startupInfo = yield this.server.start();
            const id = gitServiceFactory.getIndex();
            const queryArgs = [
                `id=${id}`,
                `port=${startupInfo.port}`,
                `internalPort=${startupInfo.port - 1}`,
                `file=${fileUri ? encodeURIComponent(fileUri.fsPath) : ''}`,
                `branchSelection=${types_6.BranchSelection.Current}`,
                `branchName=${encodeURIComponent(branchName)}`
            ];
            const uri = `${constants_1.previewUri}?${queryArgs.join('&')}`;
            let title = `Git History (${path.basename(gitRoot)})`;
            if (fileUri) {
                title = `File History (${path.basename(fileUri.fsPath)})`;
                if (typeof lineNumber === 'number') {
                    title = `Line History (${path.basename(fileUri.fsPath)}#${lineNumber})`;
                }
            }
            this.commandManager.executeCommand('previewHtml', uri, vscode_1.ViewColumn.One, title);
        });
    }
};
__decorate([
    registration_1.command('git.viewFileHistory', types_7.IGitHistoryCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [Object]),
    __metadata("design:returntype", Promise)
], GitHistoryCommandHandler.prototype, "viewFileHistory", null);
__decorate([
    registration_1.command('git.viewLineHistory', types_7.IGitHistoryCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], GitHistoryCommandHandler.prototype, "viewLineHistory", null);
__decorate([
    registration_1.command('git.viewHistory', types_7.IGitHistoryCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], GitHistoryCommandHandler.prototype, "viewBranchHistory", null);
GitHistoryCommandHandler = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_3.IServiceContainer)),
    __param(1, inversify_1.inject(disposableRegistry_1.IDisposableRegistry)),
    __param(2, inversify_1.inject(types_1.ICommandManager)),
    __metadata("design:paramtypes", [Object, Object, Object])
], GitHistoryCommandHandler);
exports.GitHistoryCommandHandler = GitHistoryCommandHandler;
//# sourceMappingURL=gitHistory.js.map