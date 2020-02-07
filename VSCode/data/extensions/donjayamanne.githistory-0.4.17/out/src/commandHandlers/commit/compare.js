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
const types_1 = require("../../application/types");
const types_2 = require("../../common/types");
const types_3 = require("../../ioc/types");
const types_4 = require("../../types");
const types_5 = require("../../viewers/types");
const registration_1 = require("../registration");
const types_6 = require("../types");
let GitCompareCommitCommandHandler = class GitCompareCommitCommandHandler {
    constructor(serviceContainer, commandManager, commitViewerFactory, application) {
        this.serviceContainer = serviceContainer;
        this.commandManager = commandManager;
        this.commitViewerFactory = commitViewerFactory;
        this.application = application;
    }
    get selectedCommit() {
        return this._previouslySelectedCommit;
    }
    select(commit) {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.commandManager.executeCommand('setContext', 'git.commit.compare.selectedForComparison', true);
            this._previouslySelectedCommit = commit;
        });
    }
    compare(commit) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!this.selectedCommit) {
                yield this.application.showErrorMessage('Please select another file to compare with');
                return;
            }
            yield this.commandManager.executeCommand('setContext', 'git.commit.compare.compared', true);
            yield this.commandManager.executeCommand('setContext', 'git.commit.compare.view.show', true);
            // display explorer view when running compare
            yield this.commandManager.executeCommand('workbench.view.explorer');
            const gitService = yield this.serviceContainer.get(types_4.IGitServiceFactory).createGitService(commit.workspaceFolder, commit.logEntry.gitRoot);
            const fileDiffs = yield gitService.getDifferences(this.selectedCommit.logEntry.hash.full, commit.logEntry.hash.full);
            const compareCommit = new types_2.CompareCommitDetails(this.selectedCommit, commit, fileDiffs);
            this.commitViewerFactory.getCompareCommitViewer().showCommitTree(compareCommit);
        });
    }
};
__decorate([
    registration_1.command('git.commit.compare.selectForComparison', types_6.IGitCompareCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [types_2.CommitDetails]),
    __metadata("design:returntype", Promise)
], GitCompareCommitCommandHandler.prototype, "select", null);
__decorate([
    registration_1.command('git.commit.compare', types_6.IGitCompareCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [types_2.CommitDetails]),
    __metadata("design:returntype", Promise)
], GitCompareCommitCommandHandler.prototype, "compare", null);
GitCompareCommitCommandHandler = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_3.IServiceContainer)),
    __param(1, inversify_1.inject(types_1.ICommandManager)),
    __param(2, inversify_1.inject(types_5.ICommitViewerFactory)),
    __param(3, inversify_1.inject(types_1.IApplicationShell)),
    __metadata("design:paramtypes", [Object, Object, Object, Object])
], GitCompareCommitCommandHandler);
exports.GitCompareCommitCommandHandler = GitCompareCommitCommandHandler;
//# sourceMappingURL=compare.js.map