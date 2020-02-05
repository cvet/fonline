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
const workspace_1 = require("../application/types/workspace");
const types_2 = require("../commandFactories/types");
const types_3 = require("../ioc/types");
const types_4 = require("../types");
const allBranches = '$(git-branch) All branches';
const currentBranch = '$(git-branch) Current branch';
let UiService = class UiService {
    constructor(serviceContainer, application) {
        this.serviceContainer = serviceContainer;
        this.application = application;
    }
    getBranchSelection() {
        return __awaiter(this, void 0, void 0, function* () {
            const itemPickList = [];
            itemPickList.push({ label: currentBranch, description: '' });
            itemPickList.push({ label: allBranches, description: '' });
            const modeChoice = yield this.application.showQuickPick(itemPickList, { placeHolder: 'Show history for...', matchOnDescription: true });
            if (!modeChoice) {
                return;
            }
            return modeChoice.label === allBranches ? types_4.BranchSelection.All : types_4.BranchSelection.Current;
        });
    }
    getWorkspaceFolder(uri) {
        return __awaiter(this, void 0, void 0, function* () {
            let workspaceFolder;
            const workspaceService = this.serviceContainer.get(workspace_1.IWorkspaceService);
            if (uri) {
                const workspaceFolderUri = workspaceService.getWorkspaceFolder(uri);
                if (workspaceFolderUri) {
                    workspaceFolder = workspaceFolderUri.uri;
                }
            }
            if (!Array.isArray(workspaceService.workspaceFolders) || workspaceService.workspaceFolders.length === 0) {
                this.serviceContainer.get(types_1.IApplicationShell).showInformationMessage('Please open a workspace folder');
                return;
            }
            const firstWorkspaceFolder = workspaceService.workspaceFolders[0].uri.fsPath;
            const folders = workspaceFolder ? [workspaceFolder] : workspaceService.workspaceFolders.map(item => item.uri);
            const gitServices = yield Promise.all(folders.map((folder) => __awaiter(this, void 0, void 0, function* () {
                const gitService = yield this.serviceContainer.get(types_4.IGitServiceFactory).createGitService(folder.fsPath, folder);
                return gitService.getGitRoots(folder.fsPath);
            })));
            const flattendGitServices = gitServices.reduce((a, b) => a.concat(b), []);
            // Filter to get only those that belong to a workspace folder
            const filteredGitRoots = flattendGitServices
                .map(gitRoot => {
                const workspaceFolderUri = workspaceService.getWorkspaceFolder(vscode_1.Uri.file(gitRoot));
                if (workspaceFolderUri) {
                    return {
                        workspaceFolder: workspaceFolderUri.uri.fsPath,
                        gitRoot
                    };
                }
                return;
            })
                .filter(item => !!item)
                .map(item => item);
            switch (filteredGitRoots.length) {
                case 0: {
                    return { workspaceFolder: firstWorkspaceFolder, gitRoot: firstWorkspaceFolder };
                }
                case 1: {
                    return filteredGitRoots[0];
                }
                default: {
                    return this.selectGitRoot(filteredGitRoots);
                }
            }
        });
    }
    selectFileCommitCommandAction(fileCommit) {
        return __awaiter(this, void 0, void 0, function* () {
            if (this.selectionActionToken) {
                this.selectionActionToken.cancel();
            }
            this.selectionActionToken = new vscode_1.CancellationTokenSource();
            const commands = yield this.serviceContainer.get(types_2.IFileCommitCommandFactory).createCommands(fileCommit);
            const options = { matchOnDescription: true, matchOnDetail: true, token: this.selectionActionToken.token };
            return this.application.showQuickPick(commands, options);
        });
    }
    selectCommitCommandAction(commit) {
        return __awaiter(this, void 0, void 0, function* () {
            if (this.selectionActionToken) {
                this.selectionActionToken.cancel();
            }
            this.selectionActionToken = new vscode_1.CancellationTokenSource();
            const commands = yield this.serviceContainer.get(types_2.ICommitCommandFactory).createCommands(commit);
            const options = { matchOnDescription: true, matchOnDetail: true, token: this.selectionActionToken.token };
            return this.application.showQuickPick(commands, options);
        });
    }
    newRefCommitCommandAction(commit) {
        return __awaiter(this, void 0, void 0, function* () {
            if (this.selectionActionToken) {
                this.selectionActionToken.cancel();
            }
            this.selectionActionToken = new vscode_1.CancellationTokenSource();
            const commands = yield this.serviceContainer.get(types_2.ICommitCommandFactory).createNewCommands(commit);
            const options = { matchOnDescription: true, matchOnDetail: true, token: this.selectionActionToken.token };
            return this.application.showQuickPick(commands, options);
        });
    }
    selectGitRoot(workspaceGitRoots) {
        return __awaiter(this, void 0, void 0, function* () {
            const app = this.serviceContainer.get(types_1.IApplicationShell);
            const pickList = workspaceGitRoots.map(item => {
                return Object.assign({}, item, { label: path.basename(item.gitRoot), detail: item.gitRoot });
            });
            const options = {
                canPickMany: false, matchOnDescription: true,
                matchOnDetail: true, placeHolder: 'Select a Git Repository'
            };
            const selectedItem = yield app.showQuickPick(pickList, options);
            if (selectedItem) {
                return selectedItem;
            }
            return;
        });
    }
};
UiService = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_3.IServiceContainer)),
    __param(1, inversify_1.inject(types_1.IApplicationShell)),
    __metadata("design:paramtypes", [Object, Object])
], UiService);
exports.UiService = UiService;
//# sourceMappingURL=uiService.js.map