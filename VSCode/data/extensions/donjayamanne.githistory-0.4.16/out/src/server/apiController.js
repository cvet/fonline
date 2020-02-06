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
const types_1 = require("../adapter/avatar/types");
const index_1 = require("../adapter/repository/index");
const commandManager_1 = require("../application/types/commandManager");
const types_2 = require("../commandHandlers/types");
const types_3 = require("../common/types");
const types_4 = require("../ioc/types");
const types_5 = require("../types");
const types_6 = require("./types");
// tslint:disable-next-line:no-require-imports no-var-requires
let ApiController = class ApiController {
    constructor(app, gitServiceFactory, serviceContainer, stateStore, commandManager) {
        this.app = app;
        this.gitServiceFactory = gitServiceFactory;
        this.serviceContainer = serviceContainer;
        this.stateStore = stateStore;
        this.commandManager = commandManager;
        // tslint:disable-next-line:cyclomatic-complexity member-ordering max-func-body-length
        this.getLogEntries = (request, response) => __awaiter(this, void 0, void 0, function* () {
            const id = decodeURIComponent(request.query.id);
            const currentState = this.stateStore.getState(id);
            const refresh = request.query.refresh === 'true';
            let searchText = request.query.searchText;
            if (currentState && currentState.searchText && typeof searchText !== 'string') {
                searchText = currentState.searchText;
            }
            searchText = typeof searchText === 'string' && searchText.length === 0 ? undefined : searchText;
            let pageIndex = request.query.pageIndex ? parseInt(request.query.pageIndex, 10) : undefined;
            if (currentState && currentState.pageIndex && typeof pageIndex !== 'number') {
                pageIndex = currentState.pageIndex;
            }
            let author = typeof request.query.author === 'string' ? request.query.author : undefined;
            if (currentState && currentState.author && typeof author !== 'string') {
                author = currentState.author;
            }
            let lineNumber = request.query.lineNumber ? parseInt(request.query.lineNumber, 10) : undefined;
            if (currentState && currentState.lineNumber && typeof lineNumber !== 'number') {
                lineNumber = currentState.lineNumber;
            }
            let branch = request.query.branch;
            if (currentState && currentState.branch && typeof branch !== 'string') {
                branch = currentState.branch;
            }
            let pageSize = request.query.pageSize ? parseInt(request.query.pageSize, 10) : undefined;
            if (currentState && currentState.pageSize && (typeof pageSize !== 'number' || pageSize === 0)) {
                pageSize = currentState.pageSize;
            }
            // When getting history for a line, then always get 10 pages, cuz `git log -L` also spits out the diff, hence slow
            if (typeof lineNumber === 'number') {
                pageSize = 10;
            }
            const filePath = request.query.file;
            let file = filePath ? vscode_1.Uri.file(filePath) : undefined;
            if (currentState && currentState.file && !file) {
                file = currentState.file;
            }
            let branchSelection = request.query.pageSize ? parseInt(request.query.branchSelection, 10) : undefined;
            if (currentState && currentState.branchSelection && typeof branchSelection !== 'number') {
                branchSelection = currentState.branchSelection;
            }
            let promise;
            const branchesMatch = currentState && (currentState.branch === branch);
            const noBranchDefinedByClient = !currentState;
            if (!refresh && searchText === undefined && pageIndex === undefined && pageSize === undefined &&
                (file === undefined || (currentState && currentState.file && currentState.file.fsPath === file.fsPath)) &&
                (author === undefined || (currentState && currentState.author === author)) &&
                currentState && currentState.entries && (branchesMatch || noBranchDefinedByClient)) {
                let selected;
                if (currentState.lastFetchedCommit) {
                    selected = yield currentState.lastFetchedCommit;
                }
                // @ts-ignore
                promise = currentState.entries.then(data => {
                    // tslint:disable-next-line:no-unnecessary-local-variable
                    // @ts-ignore
                    const entriesResponse = Object.assign({}, data, { branch: currentState.branch, author: currentState.author, lineNumber: currentState.lineNumber, branchSelection: currentState.branchSelection, file: currentState.file, pageIndex: currentState.pageIndex, pageSize: currentState.pageSize, searchText: currentState.searchText, selected: selected });
                    return entriesResponse;
                });
            }
            else if (!refresh && currentState &&
                (currentState.searchText === (searchText || '')) &&
                currentState.pageIndex === pageIndex &&
                (typeof branch === 'string' && currentState.branch === branch) &&
                currentState.pageSize === pageSize &&
                currentState.file === file &&
                currentState.author === author &&
                currentState.lineNumber === lineNumber &&
                currentState.entries) {
                promise = currentState.entries;
            }
            else {
                // @ts-ignore
                promise = (yield this.getRepository(decodeURIComponent(request.query.id)))
                    .getLogEntries(pageIndex, pageSize, branch, searchText, file, lineNumber, author)
                    .then(data => {
                    // tslint:disable-next-line:no-unnecessary-local-variable
                    // @ts-ignore
                    const entriesResponse = Object.assign({}, data, { branch,
                        author,
                        branchSelection,
                        file,
                        pageIndex,
                        pageSize,
                        searchText, selected: undefined });
                    return entriesResponse;
                });
                this.stateStore.updateEntries(id, promise, pageIndex, pageSize, branch, searchText, file, branchSelection, lineNumber, author);
            }
            try {
                const data = yield promise;
                response.send(data);
            }
            catch (err) {
                response.status(500).send(err);
            }
        });
        // tslint:disable-next-line:cyclomatic-complexity
        this.getBranches = (request, response) => __awaiter(this, void 0, void 0, function* () {
            const id = decodeURIComponent(request.query.id);
            (yield this.getRepository(id))
                .getBranches()
                .then(data => response.send(data))
                .catch(err => response.status(500).send(err));
        });
        this.getAuthors = (request, response) => __awaiter(this, void 0, void 0, function* () {
            const id = decodeURIComponent(request.query.id);
            (yield this.getRepository(id))
                .getAuthors()
                .then(data => response.send(data))
                .catch(err => response.status(500).send(err));
        });
        this.getCommit = (request, response) => __awaiter(this, void 0, void 0, function* () {
            const id = decodeURIComponent(request.query.id);
            const hash = request.params.hash;
            const currentState = this.stateStore.getState(id);
            let commitPromise;
            // tslint:disable-next-line:possible-timing-attack
            if (currentState && currentState.lastFetchedHash === hash && currentState.lastFetchedCommit) {
                commitPromise = currentState.lastFetchedCommit;
            }
            else {
                commitPromise = (yield this.getRepository(id)).getCommit(hash);
                this.stateStore.updateLastHashCommit(id, hash, commitPromise);
            }
            commitPromise
                .then(data => {
                response.send(data);
                if (data && currentState) {
                    this.commitViewer.viewCommitTree(new types_3.CommitDetails(currentState.workspaceFolder, currentState.branch, data));
                }
            })
                .catch(err => {
                response.status(500).send(err);
            });
        });
        // tslint:disable-next-line:no-any
        this.getAvatars = (request, response) => __awaiter(this, void 0, void 0, function* () {
            const id = decodeURIComponent(request.query.id);
            //const users = request.body as ActionedUser[];
            try {
                const repo = yield this.getRepository(id);
                const originType = yield repo.getOriginType();
                if (!originType) {
                    return response.send();
                }
                const providers = this.serviceContainer.getAll(types_1.IAvatarProvider);
                const provider = providers.find(item => item.supported(originType));
                const genericProvider = providers.find(item => item.supported(index_1.GitOriginType.any));
                let avatars;
                if (provider) {
                    avatars = yield provider.getAvatars(repo);
                }
                else {
                    avatars = yield genericProvider.getAvatars(repo);
                }
                response.send(avatars);
            }
            catch (err) {
                response.status(500).send(err);
            }
        });
        this.clearSelectedCommit = (request, response) => __awaiter(this, void 0, void 0, function* () {
            const id = decodeURIComponent(request.query.id);
            yield this.stateStore.clearLastHashCommit(id);
            response.send('');
        });
        this.doActionRef = (request, response) => __awaiter(this, void 0, void 0, function* () {
            response.status(200).send('');
            const id = decodeURIComponent(request.query.id);
            const workspaceFolder = this.getWorkspace(id);
            const currentState = this.stateStore.getState(id);
            const actionName = request.param('name');
            //const value = decodeURIComponent(request.query.value);
            const refEntry = request.body;
            switch (actionName) {
                case 'removeTag':
                    this.commandManager.executeCommand('git.commit.removeTag', new types_3.BranchDetails(workspaceFolder, currentState.branch), refEntry.name);
                    break;
            }
        });
        this.doAction = (request, response) => __awaiter(this, void 0, void 0, function* () {
            response.status(200).send('');
            const id = decodeURIComponent(request.query.id);
            const workspaceFolder = this.getWorkspace(id);
            const currentState = this.stateStore.getState(id);
            const actionName = request.param('name');
            const value = decodeURIComponent(request.query.value);
            const logEntry = request.body;
            switch (actionName) {
                default:
                    this.commandManager.executeCommand('git.commit.doSomething', new types_3.CommitDetails(workspaceFolder, currentState.branch, logEntry));
                    break;
                case 'new':
                    this.commandManager.executeCommand('git.commit.doNewRef', new types_3.CommitDetails(workspaceFolder, currentState.branch, logEntry));
                    break;
                case 'newtag':
                    this.commandManager.executeCommand('git.commit.createTag', new types_3.CommitDetails(workspaceFolder, currentState.branch, logEntry), value);
                    break;
                case 'newbranch':
                    this.commandManager.executeCommand('git.commit.createBranch', new types_3.CommitDetails(workspaceFolder, currentState.branch, logEntry), value);
                    break;
            }
        });
        this.selectCommittedFile = (request, response) => __awaiter(this, void 0, void 0, function* () {
            response.status(200).send('');
            const id = decodeURIComponent(request.query.id);
            const body = request.body;
            const workspaceFolder = this.getWorkspace(id);
            const currentState = this.stateStore.getState(id);
            this.commandManager.executeCommand('git.commit.file.select', new types_3.FileCommitDetails(workspaceFolder, currentState.branch, body.logEntry, body.committedFile));
        });
        // tslint:disable-next-line:no-any
        this.handleRequest = (handler) => {
            return (request, response) => __awaiter(this, void 0, void 0, function* () {
                try {
                    yield handler(request, response);
                }
                catch (err) {
                    response.status(500).send(err);
                }
            });
        };
        this.commitViewer = this.serviceContainer.get(types_2.IGitCommitViewDetailsCommandHandler);
        this.app.get('/log', this.handleRequest(this.getLogEntries.bind(this)));
        this.app.get('/branches', this.handleRequest(this.getBranches.bind(this)));
        this.app.post('/action/:name?', this.handleRequest(this.doAction.bind(this)));
        this.app.post('/actionref/:name?', this.handleRequest(this.doActionRef.bind(this)));
        this.app.get('/log/:hash', this.handleRequest(this.getCommit.bind(this)));
        this.app.post('/log/clearSelection', this.handleRequest(this.clearSelectedCommit.bind(this)));
        this.app.post('/log/:hash/committedFile', this.handleRequest(this.selectCommittedFile.bind(this)));
        this.app.post('/avatars', this.handleRequest(this.getAvatars.bind(this)));
        this.app.get('/authors', this.handleRequest(this.getAuthors.bind(this)));
    }
    // tslint:disable-next-line:no-empty member-ordering
    dispose() { }
    getWorkspace(id) {
        return this.stateStore.getState(id).workspaceFolder;
    }
    getGitRoot(id) {
        return this.stateStore.getState(id).gitRoot;
    }
    getRepository(id) {
        return __awaiter(this, void 0, void 0, function* () {
            const workspaceFolder = this.getWorkspace(id);
            return this.gitServiceFactory.createGitService(workspaceFolder, vscode_1.Uri.file(this.getGitRoot(id)));
        });
    }
};
ApiController = __decorate([
    inversify_1.injectable(),
    __metadata("design:paramtypes", [Function, Object, Object, Object, Object])
], ApiController);
exports.ApiController = ApiController;
//# sourceMappingURL=apiController.js.map