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
const types_1 = require("../ioc/types");
class WorkspaceMementoStore {
    constructor(store) {
        this.store = store;
    }
    has(key) {
        return this.store.get(key) !== undefined;
    }
    set(key, data) {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.store.update(key, data);
        });
    }
    get(key) {
        return __awaiter(this, void 0, void 0, function* () {
            return this.store.get(key);
        });
    }
}
exports.WorkspaceMementoStore = WorkspaceMementoStore;
let WorkspaceStateStoreFactory = class WorkspaceStateStoreFactory {
    constructor(serviceContainer) {
        this.serviceContainer = serviceContainer;
    }
    createStore() {
        return new WorkspaceMementoStore(this.serviceContainer.get('workspaceMementoStore'));
    }
};
WorkspaceStateStoreFactory = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_1.IServiceContainer)),
    __metadata("design:paramtypes", [Object])
], WorkspaceStateStoreFactory);
exports.WorkspaceStateStoreFactory = WorkspaceStateStoreFactory;
let GlobalStateStoreFactory = class GlobalStateStoreFactory {
    constructor(serviceContainer) {
        this.serviceContainer = serviceContainer;
    }
    createStore() {
        return new WorkspaceMementoStore(this.serviceContainer.get('globalMementoStore'));
    }
};
GlobalStateStoreFactory = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_1.IServiceContainer)),
    __metadata("design:paramtypes", [Object])
], GlobalStateStoreFactory);
exports.GlobalStateStoreFactory = GlobalStateStoreFactory;
//# sourceMappingURL=stateStore.js.map