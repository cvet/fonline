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
const registration_1 = require("../registration");
const types_5 = require("../types");
let GitCompareFileCommitCommandHandler = class GitCompareFileCommitCommandHandler {
    constructor(serviceContainer, commandManager, application) {
        this.serviceContainer = serviceContainer;
        this.commandManager = commandManager;
        this.application = application;
    }
    get selectedCommit() {
        return this._previouslySelectedCommit;
    }
    select(nodeOrFileCommit) {
        return __awaiter(this, void 0, void 0, function* () {
            const fileCommit = nodeOrFileCommit instanceof types_2.FileCommitDetails ? nodeOrFileCommit : nodeOrFileCommit.data;
            yield this.commandManager.executeCommand('setContext', 'git.commit.FileEntry.selectForComparison', true);
            this._previouslySelectedCommit = fileCommit;
        });
    }
    compare(nodeOrFileCommit) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!this.selectedCommit) {
                yield this.application.showErrorMessage('Please select another file to compare with');
                return;
            }
            const fileCommit = nodeOrFileCommit instanceof types_2.FileCommitDetails ? nodeOrFileCommit : nodeOrFileCommit.data;
            const gitService = yield this.serviceContainer.get(types_4.IGitServiceFactory).createGitService(fileCommit.workspaceFolder, fileCommit.logEntry.gitRoot);
            const fileDiffs = yield gitService.getDifferences(this.selectedCommit.logEntry.hash.full, fileCommit.logEntry.hash.full);
            yield this.commandManager.executeCommand('git.commit.diff.view', this.selectedCommit, fileCommit, fileDiffs);
        });
    }
};
__decorate([
    registration_1.command('git.commit.FileEntry.selectForComparison', types_5.IGitCompareFileCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [Object]),
    __metadata("design:returntype", Promise)
], GitCompareFileCommitCommandHandler.prototype, "select", null);
__decorate([
    registration_1.command('git.commit.FileEntry.compare', types_5.IGitCompareFileCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [Object]),
    __metadata("design:returntype", Promise)
], GitCompareFileCommitCommandHandler.prototype, "compare", null);
GitCompareFileCommitCommandHandler = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_3.IServiceContainer)),
    __param(1, inversify_1.inject(types_1.ICommandManager)),
    __param(2, inversify_1.inject(types_1.IApplicationShell)),
    __metadata("design:paramtypes", [Object, Object, Object])
], GitCompareFileCommitCommandHandler);
exports.GitCompareFileCommitCommandHandler = GitCompareFileCommitCommandHandler;
//# sourceMappingURL=fileCompare.js.map