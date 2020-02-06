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
const md5 = require("md5");
const vscode_1 = require("vscode");
const types_1 = require("../../ioc/types");
const exec_1 = require("../exec");
const types_2 = require("../parsers/types");
const git_1 = require("./git");
const types_3 = require("./types");
let GitServiceFactory = class GitServiceFactory {
    constructor(gitCmdExecutor, logParser, gitArgsService, serviceContainer) {
        this.gitCmdExecutor = gitCmdExecutor;
        this.logParser = logParser;
        this.gitArgsService = gitArgsService;
        this.serviceContainer = serviceContainer;
        this.gitServices = new Map();
    }
    createGitService(workspaceRoot, resource) {
        return __awaiter(this, void 0, void 0, function* () {
            const resourceUri = typeof resource === 'string' ? vscode_1.Uri.file(resource) : resource;
            const id = md5(workspaceRoot + resourceUri.fsPath);
            if (!this.gitServices.has(id)) {
                this.gitServices.set(id, new git_1.Git(this.serviceContainer, workspaceRoot, resourceUri, this.gitCmdExecutor, this.logParser, this.gitArgsService));
            }
            return this.gitServices.get(id);
        });
    }
};
GitServiceFactory = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(exec_1.IGitCommandExecutor)),
    __param(1, inversify_1.inject(types_2.ILogParser)),
    __param(2, inversify_1.inject(types_3.IGitArgsService)),
    __param(3, inversify_1.inject(types_1.IServiceContainer)),
    __metadata("design:paramtypes", [Object, Object, Object, Object])
], GitServiceFactory);
exports.GitServiceFactory = GitServiceFactory;
//# sourceMappingURL=factory.js.map