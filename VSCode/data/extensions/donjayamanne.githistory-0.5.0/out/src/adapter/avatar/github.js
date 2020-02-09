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
const axios_1 = require("axios");
const inversify_1 = require("inversify");
const stateStore_1 = require("../../application/types/stateStore");
const types_1 = require("../../ioc/types");
const types_2 = require("../repository/types");
const base_1 = require("./base");
// tslint:disable-next-line:no-require-imports no-var-requires
const { URL } = require('url');
let GithubAvatarProvider = class GithubAvatarProvider extends base_1.BaseAvatarProvider {
    constructor(serviceContainer) {
        super(serviceContainer, types_2.GitOriginType.github);
        this.httpProxy = '';
        const stateStoreFactory = this.serviceContainer.get(stateStore_1.IStateStoreFactory);
        this.stateStore = stateStoreFactory.createStore();
    }
    get proxy() {
        let proxy;
        if (this.httpProxy.length > 0) {
            const proxyUri = new URL(this.httpProxy);
            proxy = { host: proxyUri.hostname, port: proxyUri.port };
        }
        return proxy;
    }
    getAvatarsImplementation(repository) {
        return __awaiter(this, void 0, void 0, function* () {
            const remoteUrl = yield repository.getOriginUrl();
            const remoteRepoPath = remoteUrl.replace(/.*?github.com\//, '');
            const remoteRepoWithNoGitSuffix = remoteRepoPath.replace(/\.git\/?$/, '');
            const contributors = yield this.getContributors(remoteRepoWithNoGitSuffix);
            const githubUsers = yield Promise.all(contributors.map((user) => __awaiter(this, void 0, void 0, function* () {
                return yield this.getUserByLogin(user.login);
            })));
            let avatars = [];
            githubUsers.forEach(user => {
                if (!user) {
                    return;
                }
                avatars.push({
                    login: user.login,
                    name: user.name,
                    email: user.email,
                    url: user.url,
                    avatarUrl: user.avatar_url
                });
            });
            return avatars;
        });
    }
    /**
     * Fetch the user details through Github API
     * @param loginName the user login name from github
     */
    getUserByLogin(loginName) {
        return __awaiter(this, void 0, void 0, function* () {
            const key = `GitHub:User:${loginName}`;
            const cachedUser = yield this.stateStore.get(key);
            let headers = {};
            if (cachedUser) {
                // Use GitHub API with conditional check on last modified
                // to avoid API request rate limitation
                headers = { 'If-Modified-Since': cachedUser.last_modified };
            }
            const proxy = this.proxy;
            const info = yield axios_1.default.get(`https://api.github.com/users/${encodeURIComponent(loginName)}`, { proxy, headers })
                .then((result) => {
                if (!result.data || (!result.data.name && !result.data.login)) {
                    return;
                }
                else {
                    result.data.last_modified = result.headers['last-modified'];
                    return result.data;
                }
            }).catch(() => {
                // can either be '302 Not Modified' or any other error
                // in case of '302 Not Modified' this API request is not counted and returns nothing
            });
            if (info) {
                yield this.stateStore.set(key, info);
                return info;
            }
            else {
                return cachedUser;
            }
        });
    }
    /**
     * Fetch all constributors from the remote repository through Github API
     * @param repoPath relative repository path
     */
    getContributors(repoPath) {
        return __awaiter(this, void 0, void 0, function* () {
            const proxy = this.proxy;
            return axios_1.default.get(`https://api.github.com/repos/${repoPath}/contributors`, { proxy, timeout: 8000 })
                .then((result) => {
                return result.data;
            });
        });
    }
};
GithubAvatarProvider = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_1.IServiceContainer)),
    __metadata("design:paramtypes", [Object])
], GithubAvatarProvider);
exports.GithubAvatarProvider = GithubAvatarProvider;
//# sourceMappingURL=github.js.map