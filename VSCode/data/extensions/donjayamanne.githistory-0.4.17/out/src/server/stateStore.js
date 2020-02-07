"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
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
let StateStore = class StateStore {
    constructor() {
        this.storesPerWorkspace = new Map();
    }
    initialize(id, workspaceFolder, gitRoot, branch, branchSelection, searchText, file, lineNumber, author) {
        return __awaiter(this, void 0, void 0, function* () {
            this.storesPerWorkspace.set(id, { gitRoot: gitRoot, branch, branchSelection, workspaceFolder, searchText, file, lineNumber, author });
        });
    }
    updateEntries(id, entries, pageIndex, pageSize, branch, searchText, file, branchSelection, lineNumber, author) {
        return __awaiter(this, void 0, void 0, function* () {
            const state = this.storesPerWorkspace.get(id);
            state.branch = branch;
            state.entries = entries;
            state.author = author;
            state.pageIndex = pageIndex;
            state.lineNumber = lineNumber;
            state.pageSize = pageSize;
            state.searchText = searchText;
            state.file = file;
            state.branchSelection = branchSelection;
            this.storesPerWorkspace.set(id, state);
        });
    }
    updateLastHashCommit(id, hash, commit) {
        return __awaiter(this, void 0, void 0, function* () {
            const state = this.storesPerWorkspace.get(id);
            state.lastFetchedHash = hash;
            state.lastFetchedCommit = commit;
            this.storesPerWorkspace.set(id, state);
        });
    }
    clearLastHashCommit(id) {
        return __awaiter(this, void 0, void 0, function* () {
            const state = this.storesPerWorkspace.get(id);
            state.lastFetchedHash = undefined;
            state.lastFetchedCommit = undefined;
            this.storesPerWorkspace.set(id, state);
        });
    }
    getState(id) {
        return this.storesPerWorkspace.get(id);
    }
    dispose() {
        this.storesPerWorkspace.clear();
    }
};
StateStore = __decorate([
    inversify_1.injectable()
], StateStore);
exports.StateStore = StateStore;
//# sourceMappingURL=stateStore.js.map