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
const vscode_1 = require("vscode");
const types_1 = require("../../application/types");
const types_2 = require("../../ioc/types");
const exec_1 = require("../exec");
const types_3 = require("../parsers/types");
const git_1 = require("./git");
const types_4 = require("./types");
const path = require("path");
let GitServiceFactory = class GitServiceFactory {
    constructor(gitCmdExecutor, logParser, gitArgsService, serviceContainer) {
        this.gitCmdExecutor = gitCmdExecutor;
        this.logParser = logParser;
        this.gitArgsService = gitArgsService;
        this.serviceContainer = serviceContainer;
        this.gitServices = new Map();
        this.gitApi = this.gitCmdExecutor.gitExtension.getAPI(1);
        this.repoIndex = -1;
    }
    getIndex() {
        return this.repoIndex;
    }
    getService(index) {
        return this.gitServices.get(index);
    }
    repositoryPicker() {
        return __awaiter(this, void 0, void 0, function* () {
            if (this.repoIndex > -1)
                return;
            const app = this.serviceContainer.get(types_1.IApplicationShell);
            const pickList = [];
            this.gitApi.repositories.forEach(x => {
                pickList.push({ label: path.basename(x.rootUri.path), detail: x.rootUri.path, description: x.state.HEAD.name });
            });
            const options = {
                canPickMany: false, matchOnDescription: false,
                matchOnDetail: true, placeHolder: 'Select a Git Repository'
            };
            const selectedItem = yield app.showQuickPick(pickList, options);
            if (selectedItem) {
                this.repoIndex = this.gitApi.repositories.findIndex(x => x.rootUri.path == selectedItem.detail);
            }
        });
    }
    createGitService(resource) {
        return __awaiter(this, void 0, void 0, function* () {
            const resourceUri = typeof resource === 'string' ? vscode_1.Uri.file(resource) : resource;
            if (!resourceUri) {
                const currentIndex = this.gitApi.repositories.findIndex(x => x.ui.selected);
                // show repository picker
                if (this.repoIndex === -1) {
                    yield this.repositoryPicker();
                }
                else if (currentIndex > -1 && currentIndex !== this.repoIndex) {
                    this.repoIndex = currentIndex;
                }
            }
            if (resourceUri) {
                // find the correct repository from the given resource uri
                let i = 0;
                for (let x of this.gitApi.repositories) {
                    if (resourceUri.fsPath.startsWith(x.rootUri.fsPath) && x.rootUri.fsPath === this.gitApi.repositories[i].rootUri.fsPath) {
                        this.repoIndex = i;
                        break;
                    }
                    i++;
                }
            }
            if (!this.gitServices.has(this.repoIndex)) {
                this.gitServices.set(this.repoIndex, new git_1.Git(this.gitApi.repositories[this.repoIndex], this.serviceContainer, this.gitCmdExecutor, this.logParser, this.gitArgsService));
            }
            return this.gitServices.get(this.repoIndex);
        });
    }
};
GitServiceFactory = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(exec_1.IGitCommandExecutor)),
    __param(1, inversify_1.inject(types_3.ILogParser)),
    __param(2, inversify_1.inject(types_4.IGitArgsService)),
    __param(3, inversify_1.inject(types_2.IServiceContainer)),
    __metadata("design:paramtypes", [Object, Object, Object, Object])
], GitServiceFactory);
exports.GitServiceFactory = GitServiceFactory;
//# sourceMappingURL=factory.js.map