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
const stateStore_1 = require("../../application/types/stateStore");
const workspace_1 = require("../../application/types/workspace");
const types_1 = require("../../ioc/types");
const types_2 = require("../repository/types");
let BaseAvatarProvider = class BaseAvatarProvider {
    constructor(serviceContainer, remoteRepoType) {
        this.serviceContainer = serviceContainer;
        this.remoteRepoType = remoteRepoType;
        const workspace = this.serviceContainer.get(workspace_1.IWorkspaceService);
        this.httpProxy = workspace.getConfiguration('http').get('proxy', '');
        const stateStoreFactory = this.serviceContainer.get(stateStore_1.IStateStoreFactory);
        this.avatarStateStore = stateStoreFactory.createStore();
    }
    getAvatars(repository) {
        return __awaiter(this, void 0, void 0, function* () {
            const workspace = this.serviceContainer.get(workspace_1.IWorkspaceService);
            const cacheExpiration = workspace.getConfiguration('gitHistory').get('avatarCacheExpiration', 60); // in minutes (zero to disable cache)
            const remoteUrl = yield repository.getOriginUrl();
            const key = `Git:Avatars:${remoteUrl}`;
            const cachedAvatars = yield this.avatarStateStore.get(key);
            const retry = cacheExpiration === 0 || !cachedAvatars || (cachedAvatars && cachedAvatars.timestamp && (cachedAvatars.timestamp + (cacheExpiration * 60 * 1000)) < new Date().getTime());
            if (retry) {
                const avatars = yield this.getAvatarsImplementation(repository);
                yield this.avatarStateStore.set(key, { timestamp: new Date().getTime(), items: avatars });
                return avatars;
            }
            else if (cachedAvatars) {
                return cachedAvatars.items;
            }
            return [];
        });
    }
    supported(remoteRepo) {
        return remoteRepo === this.remoteRepoType;
    }
};
BaseAvatarProvider = __decorate([
    inversify_1.injectable(),
    __param(1, inversify_1.unmanaged()),
    __metadata("design:paramtypes", [Object, Number])
], BaseAvatarProvider);
exports.BaseAvatarProvider = BaseAvatarProvider;
//# sourceMappingURL=base.js.map