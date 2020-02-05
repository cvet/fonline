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
const commandManager_1 = require("../../application/types/commandManager");
const types_1 = require("../../viewers/types");
const registration_1 = require("../registration");
const types_2 = require("../types");
let GitCompareCommitViewExplorerCommandHandler = class GitCompareCommitViewExplorerCommandHandler {
    constructor(commandManager, commitViewerFactory) {
        this.commandManager = commandManager;
        this.commitViewerFactory = commitViewerFactory;
    }
    hide() {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.commandManager.executeCommand('setContext', 'git.commit.compare.view.show', false);
        });
    }
    show() {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.commandManager.executeCommand('setContext', 'git.commit.compare.view.show', true);
        });
    }
    showFilesView() {
        return __awaiter(this, void 0, void 0, function* () {
            this.commitViewerFactory.getCommitViewer().showFilesView();
        });
    }
    showFolderView() {
        return __awaiter(this, void 0, void 0, function* () {
            this.commitViewerFactory.getCommitViewer().showFolderView();
        });
    }
};
__decorate([
    registration_1.command('git.commit.compare.view.hide', types_2.IGitCompareCommitViewExplorerCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], GitCompareCommitViewExplorerCommandHandler.prototype, "hide", null);
__decorate([
    registration_1.command('git.commit.compare.view.show', types_2.IGitCompareCommitViewExplorerCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], GitCompareCommitViewExplorerCommandHandler.prototype, "show", null);
__decorate([
    registration_1.command('git.commit.compare.view.showFilesOnly', types_2.IGitCompareCommitViewExplorerCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], GitCompareCommitViewExplorerCommandHandler.prototype, "showFilesView", null);
__decorate([
    registration_1.command('git.commit.compare.view.showFolderView', types_2.IGitCompareCommitViewExplorerCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], GitCompareCommitViewExplorerCommandHandler.prototype, "showFolderView", null);
GitCompareCommitViewExplorerCommandHandler = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(commandManager_1.ICommandManager)),
    __param(1, inversify_1.inject(types_1.ICommitViewerFactory)),
    __metadata("design:paramtypes", [Object, Object])
], GitCompareCommitViewExplorerCommandHandler);
exports.GitCompareCommitViewExplorerCommandHandler = GitCompareCommitViewExplorerCommandHandler;
//# sourceMappingURL=compareViewExplorer.js.map