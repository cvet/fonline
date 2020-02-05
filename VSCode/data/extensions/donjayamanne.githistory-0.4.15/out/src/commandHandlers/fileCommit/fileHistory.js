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
const types_1 = require("../../application/types");
const commandManager_1 = require("../../application/types/commandManager");
const types_2 = require("../../common/types");
const types_3 = require("../../ioc/types");
const types_4 = require("../../platform/types");
const types_5 = require("../../types");
const registration_1 = require("../registration");
const types_6 = require("../types");
let GitFileHistoryCommandHandler = class GitFileHistoryCommandHandler {
    constructor(serviceContainer, commandManager, applicationShell, fileSystem) {
        this.serviceContainer = serviceContainer;
        this.commandManager = commandManager;
        this.applicationShell = applicationShell;
        this.fileSystem = fileSystem;
    }
    doSomethingWithFile(fileCommit) {
        return __awaiter(this, void 0, void 0, function* () {
            const cmd = yield this.serviceContainer.get(types_2.IUiService).selectFileCommitCommandAction(fileCommit);
            if (!cmd) {
                return;
            }
            return cmd.execute();
        });
    }
    viewFile(nodeOrFileCommit) {
        return __awaiter(this, void 0, void 0, function* () {
            const fileCommit = nodeOrFileCommit instanceof types_2.FileCommitDetails ? nodeOrFileCommit : nodeOrFileCommit.data;
            const gitService = yield this.serviceContainer.get(types_5.IGitServiceFactory).createGitService(fileCommit.workspaceFolder, fileCommit.logEntry.gitRoot);
            if (fileCommit.committedFile.status === types_5.Status.Deleted) {
                return this.applicationShell.showErrorMessage('File cannot be viewed as it was deleted').then(() => void 0);
            }
            const tmpFile = yield gitService.getCommitFile(fileCommit.logEntry.hash.full, fileCommit.committedFile.uri);
            yield this.commandManager.executeCommand('git.openFileInViewer', tmpFile);
        });
    }
    compareFileWithWorkspace(nodeOrFileCommit) {
        return __awaiter(this, void 0, void 0, function* () {
            const fileCommit = nodeOrFileCommit instanceof types_2.FileCommitDetails ? nodeOrFileCommit : nodeOrFileCommit.data;
            const gitService = yield this.serviceContainer.get(types_5.IGitServiceFactory).createGitService(fileCommit.workspaceFolder, fileCommit.logEntry.gitRoot);
            if (fileCommit.committedFile.status === types_5.Status.Deleted) {
                return this.applicationShell.showErrorMessage('File cannot be compared with, as it was deleted').then(() => void 0);
            }
            if (!(yield this.fileSystem.fileExistsAsync(fileCommit.committedFile.uri.fsPath))) {
                return this.applicationShell.showErrorMessage('Corresponding workspace file does not exist').then(() => void 0);
            }
            const tmpFile = yield gitService.getCommitFile(fileCommit.logEntry.hash.full, fileCommit.committedFile.uri);
            const fileName = path.basename(fileCommit.committedFile.uri.fsPath);
            const title = `${fileName} (Working Tree)`;
            yield this.commandManager.executeCommand('vscode.diff', vscode_1.Uri.file(fileCommit.committedFile.uri.fsPath), vscode_1.Uri.file(tmpFile.fsPath), title, { preview: true });
        });
    }
    compareFileWithPrevious(nodeOrFileCommit) {
        return __awaiter(this, void 0, void 0, function* () {
            const fileCommit = nodeOrFileCommit instanceof types_2.FileCommitDetails ? nodeOrFileCommit : nodeOrFileCommit.data;
            const gitService = yield this.serviceContainer.get(types_5.IGitServiceFactory).createGitService(fileCommit.workspaceFolder, fileCommit.logEntry.gitRoot);
            if (fileCommit.committedFile.status === types_5.Status.Deleted) {
                return this.applicationShell.showErrorMessage('File cannot be compared with, as it was deleted').then(() => void 0);
            }
            if (fileCommit.committedFile.status === types_5.Status.Added) {
                return this.applicationShell.showErrorMessage('File cannot be compared with previous, as this is a new file').then(() => void 0);
            }
            const tmpFilePromise = gitService.getCommitFile(fileCommit.logEntry.hash.full, fileCommit.committedFile.uri);
            const previousCommitHashPromise = gitService.getPreviousCommitHashForFile(fileCommit.logEntry.hash.full, fileCommit.committedFile.uri);
            const values = yield Promise.all([tmpFilePromise, previousCommitHashPromise]);
            const tmpFile = values[0];
            const previousCommitHash = values[1];
            const previousFile = fileCommit.committedFile.oldUri ? fileCommit.committedFile.oldUri : fileCommit.committedFile.uri;
            const previousTmpFile = yield gitService.getCommitFile(previousCommitHash.full, previousFile);
            const title = this.getComparisonTitle({ file: vscode_1.Uri.file(fileCommit.committedFile.uri.fsPath), hash: fileCommit.logEntry.hash }, { file: vscode_1.Uri.file(previousFile.fsPath), hash: previousCommitHash });
            yield this.commandManager.executeCommand('vscode.diff', previousTmpFile, tmpFile, title, { preview: true });
        });
    }
    viewPreviousFile(nodeOrFileCommit) {
        return __awaiter(this, void 0, void 0, function* () {
            const fileCommit = nodeOrFileCommit instanceof types_2.FileCommitDetails ? nodeOrFileCommit : nodeOrFileCommit.data;
            const gitService = yield this.serviceContainer.get(types_5.IGitServiceFactory).createGitService(fileCommit.workspaceFolder, fileCommit.logEntry.gitRoot);
            if (fileCommit.committedFile.status === types_5.Status.Added) {
                return this.applicationShell.showErrorMessage('Previous version of the file cannot be opened, as this is a new file').then(() => void 0);
            }
            const previousCommitHash = yield gitService.getPreviousCommitHashForFile(fileCommit.logEntry.hash.full, fileCommit.committedFile.uri);
            const previousFile = fileCommit.committedFile.oldUri ? fileCommit.committedFile.oldUri : fileCommit.committedFile.uri;
            const previousTmpFile = yield gitService.getCommitFile(previousCommitHash.full, previousFile);
            yield this.commandManager.executeCommand('git.openFileInViewer', vscode_1.Uri.file(previousTmpFile.fsPath));
        });
    }
    compareFileAcrossCommits(fileCommit) {
        return __awaiter(this, void 0, void 0, function* () {
            const gitService = yield this.serviceContainer.get(types_5.IGitServiceFactory).createGitService(fileCommit.workspaceFolder, fileCommit.logEntry.gitRoot);
            if (fileCommit.committedFile.status === types_5.Status.Deleted) {
                return this.applicationShell.showErrorMessage('File cannot be compared with, as it was deleted').then(() => void 0);
            }
            if (fileCommit.committedFile.status === types_5.Status.Added) {
                return this.applicationShell.showErrorMessage('File cannot be compared, as this is a new file').then(() => void 0);
            }
            const leftFilePromise = gitService.getCommitFile(fileCommit.logEntry.hash.full, fileCommit.committedFile.uri);
            const rightFilePromise = gitService.getCommitFile(fileCommit.rightCommit.logEntry.hash.full, fileCommit.committedFile.uri);
            const [leftFile, rightFile] = yield Promise.all([leftFilePromise, rightFilePromise]);
            const title = this.getComparisonTitle({ file: vscode_1.Uri.file(fileCommit.committedFile.uri.fsPath), hash: fileCommit.logEntry.hash }, { file: vscode_1.Uri.file(fileCommit.committedFile.uri.fsPath), hash: fileCommit.rightCommit.logEntry.hash });
            yield this.commandManager.executeCommand('vscode.diff', leftFile, rightFile, title, { preview: true });
        });
    }
    getComparisonTitle(left, right) {
        const leftFileName = path.basename(left.file.fsPath);
        const rightFileName = path.basename(right.file.fsPath);
        if (leftFileName === rightFileName) {
            return `${leftFileName} (${left.hash.short} ↔ ${right.hash.short})`;
        }
        else {
            return `${leftFileName} (${left.hash.short} ↔ ${rightFileName} ${right.hash.short})`;
        }
    }
};
__decorate([
    registration_1.command('git.commit.file.select', types_6.IGitFileHistoryCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [types_2.FileCommitDetails]),
    __metadata("design:returntype", Promise)
], GitFileHistoryCommandHandler.prototype, "doSomethingWithFile", null);
__decorate([
    registration_1.command('git.commit.FileEntry.ViewFileContents', types_6.IGitFileHistoryCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [Object]),
    __metadata("design:returntype", Promise)
], GitFileHistoryCommandHandler.prototype, "viewFile", null);
__decorate([
    registration_1.command('git.commit.FileEntry.CompareAgainstWorkspace', types_6.IGitFileHistoryCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [Object]),
    __metadata("design:returntype", Promise)
], GitFileHistoryCommandHandler.prototype, "compareFileWithWorkspace", null);
__decorate([
    registration_1.command('git.commit.FileEntry.CompareAgainstPrevious', types_6.IGitFileHistoryCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [Object]),
    __metadata("design:returntype", Promise)
], GitFileHistoryCommandHandler.prototype, "compareFileWithPrevious", null);
__decorate([
    registration_1.command('git.commit.FileEntry.ViewPreviousFileContents', types_6.IGitFileHistoryCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [Object]),
    __metadata("design:returntype", Promise)
], GitFileHistoryCommandHandler.prototype, "viewPreviousFile", null);
__decorate([
    registration_1.command('git.commit.compare.file.compare', types_6.IGitFileHistoryCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [types_2.CompareFileCommitDetails]),
    __metadata("design:returntype", Promise)
], GitFileHistoryCommandHandler.prototype, "compareFileAcrossCommits", null);
GitFileHistoryCommandHandler = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_3.IServiceContainer)),
    __param(1, inversify_1.inject(commandManager_1.ICommandManager)),
    __param(2, inversify_1.inject(types_1.IApplicationShell)),
    __param(3, inversify_1.inject(types_4.IFileSystem)),
    __metadata("design:paramtypes", [Object, Object, Object, Object])
], GitFileHistoryCommandHandler);
exports.GitFileHistoryCommandHandler = GitFileHistoryCommandHandler;
//# sourceMappingURL=fileHistory.js.map