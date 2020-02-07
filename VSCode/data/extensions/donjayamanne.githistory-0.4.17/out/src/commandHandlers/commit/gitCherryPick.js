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
let GitCherryPickCommandHandler = class GitCherryPickCommandHandler {
    constructor(serviceContainer, commitViewerFactory, applicationShell) {
        this.serviceContainer = serviceContainer;
        this.commitViewerFactory = commitViewerFactory;
        this.applicationShell = applicationShell;
    }
    cherryPickCommit(commit, showPrompt = true) {
        return __awaiter(this, void 0, void 0, function* () {
            commit = commit ? commit : this.commitViewerFactory.getCommitViewer().selectedCommit;
            const gitService = yield this.serviceContainer.get(types_4.IGitServiceFactory).createGitService(commit.workspaceFolder, commit.logEntry.gitRoot);
            const currentBranch = yield gitService.getCurrentBranch();
            const msg = `Cherry pick ${commit.logEntry.hash.short} into ${currentBranch}?`;
            const yesNo = showPrompt ? yield this.applicationShell.showQuickPick(['Yes', 'No'], { placeHolder: msg }) : 'Yes';
            if (yesNo === undefined || yesNo === 'No') {
                return;
            }
            gitService.cherryPick(commit.logEntry.hash.full)
                .catch(err => {
                if (typeof err === 'string') {
                    this.applicationShell.showErrorMessage(err);
                }
            });
        });
    }
};
__decorate([
    registration_1.command('git.commit.cherryPick', types_6.IGitCherryPickCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [types_2.CommitDetails, Boolean]),
    __metadata("design:returntype", Promise)
], GitCherryPickCommandHandler.prototype, "cherryPickCommit", null);
GitCherryPickCommandHandler = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_3.IServiceContainer)),
    __param(1, inversify_1.inject(types_5.ICommitViewerFactory)),
    __param(2, inversify_1.inject(types_1.IApplicationShell)),
    __metadata("design:paramtypes", [Object, Object, Object])
], GitCherryPickCommandHandler);
exports.GitCherryPickCommandHandler = GitCherryPickCommandHandler;
//# sourceMappingURL=gitCherryPick.js.map