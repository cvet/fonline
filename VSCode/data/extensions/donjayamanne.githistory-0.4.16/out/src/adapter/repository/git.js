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
const fs = require("fs-extra");
const inversify_1 = require("inversify");
const path = require("path");
const tmp = require("tmp");
const vscode_1 = require("vscode");
const workspace_1 = require("../../application/types/workspace");
const cache_1 = require("../../common/cache");
const types_1 = require("../../ioc/types");
const exec_1 = require("../exec");
const types_2 = require("../parsers/types");
const constants_1 = require("./constants");
const index_1 = require("./index");
const types_3 = require("./types");
let Git = class Git {
    constructor(serviceContainer, workspaceRoot, resource, gitCmdExecutor, logParser, gitArgsService) {
        this.serviceContainer = serviceContainer;
        this.workspaceRoot = workspaceRoot;
        this.resource = resource;
        this.gitCmdExecutor = gitCmdExecutor;
        this.logParser = logParser;
        this.gitArgsService = gitArgsService;
        this.knownGitRoots = new Set();
    }
    getHashCode() {
        return this.workspaceRoot;
    }
    getGitRoot() {
        return __awaiter(this, void 0, void 0, function* () {
            if (this.gitRootPath) {
                return this.gitRootPath;
            }
            const gitRootPath = yield this.gitCmdExecutor.exec(this.resource.fsPath, ...this.gitArgsService.getGitRootArgs());
            return this.gitRootPath = gitRootPath.split(/\r?\n/g)[0].trim();
        });
    }
    getGitRoots(rootDirectory) {
        return __awaiter(this, void 0, void 0, function* () {
            // Lets not enable support for sub modules for now.
            if (rootDirectory && (this.knownGitRoots.has(rootDirectory) || this.knownGitRoots.has(vscode_1.Uri.file(rootDirectory).fsPath))) {
                return [rootDirectory];
            }
            const rootDirectories = [];
            if (rootDirectory) {
                rootDirectories.push(rootDirectory);
            }
            else {
                const workspace = this.serviceContainer.get(workspace_1.IWorkspaceService);
                const workspaceFolders = Array.isArray(workspace.workspaceFolders) ? workspace.workspaceFolders.map(item => item.uri.fsPath) : [];
                rootDirectories.push(...workspaceFolders);
            }
            if (rootDirectories.length === 0) {
                return [];
            }
            // Instead of traversing the directory structure for the entire workspace, use the Git extension API to get all repo paths
            const git = this.gitCmdExecutor.gitExtension.getAPI(1);
            const sourceControlFolders = git.repositories.map(repo => repo.rootUri.path);
            sourceControlFolders.sort();
            // gitFoldersList should be an Array of Arrays
            const gitFoldersList = [sourceControlFolders];
            const gitRoots = new Set();
            gitFoldersList
                .reduce((aggregate, items) => { aggregate.push(...items); return aggregate; }, [])
                .forEach(item => {
                gitRoots.add(item);
                this.knownGitRoots.add(item);
            });
            return Array.from(gitRoots.values());
        });
    }
    getGitRelativePath(file) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!path.isAbsolute(file.fsPath)) {
                return file.fsPath;
            }
            const gitRoot = yield this.getGitRoot();
            return path.relative(gitRoot, file.fsPath).replace(/\\/g, '/');
        });
    }
    getHeadHashes() {
        return __awaiter(this, void 0, void 0, function* () {
            const fullHashArgs = ['show-ref'];
            const fullHashRefsOutput = yield this.exec(...fullHashArgs);
            return fullHashRefsOutput.split(/\r?\n/g)
                .filter(line => line.length > 0)
                .filter(line => line.indexOf('refs/heads/') > 0 || line.indexOf('refs/remotes/') > 0)
                .map(line => line.trim().split(' '))
                .filter(lineParts => lineParts.length > 1)
                .map(hashAndRef => { return { ref: hashAndRef[1], hash: hashAndRef[0] }; });
        });
    }
    getAuthors() {
        return __awaiter(this, void 0, void 0, function* () {
            const authorArgs = this.gitArgsService.getAuthorsArgs();
            const authors = yield this.exec(...authorArgs);
            const dict = new Set();
            return authors.split(/\r?\n/g)
                .map(line => line.trim())
                .filter(line => line.trim().length > 0)
                .map(line => line.substring(line.indexOf('\t') + 1))
                .map(line => {
                const indexOfEmailSeparator = line.indexOf('<');
                if (indexOfEmailSeparator === -1) {
                    return {
                        name: line.trim(),
                        email: ''
                    };
                }
                else {
                    const nameParts = line.split('<');
                    const name = nameParts.shift().trim();
                    const email = nameParts[0].substring(0, nameParts[0].length - 1).trim();
                    return {
                        name,
                        email
                    };
                }
            })
                .filter(item => {
                if (dict.has(item.name)) {
                    return false;
                }
                dict.add(item.name);
                return true;
            })
                .sort((a, b) => a.name > b.name ? 1 : -1);
        });
    }
    // tslint:disable-next-line:no-suspicious-comment
    // TODO: We need a way of clearing this cache, if a new branch is created.
    getBranches() {
        return __awaiter(this, void 0, void 0, function* () {
            const output = yield this.exec('branch');
            const gitRootPath = yield this.getGitRoot();
            return output.split(/\r?\n/g)
                .filter(line => line.trim())
                .filter(line => line.length > 0)
                .map(line => {
                const isCurrent = line.startsWith('*');
                const name = isCurrent ? line.substring(1).trim() : line.trim();
                return {
                    gitRoot: gitRootPath,
                    name,
                    current: isCurrent
                };
            });
        });
    }
    // tslint:disable-next-line:no-suspicious-comment
    // TODO: We need a way of clearing this cache, if a new branch is created.
    getCurrentBranch() {
        return __awaiter(this, void 0, void 0, function* () {
            const args = this.gitArgsService.getCurrentBranchArgs();
            const branch = yield this.exec(...args);
            return branch.split(/\r?\n/g)[0].trim();
        });
    }
    getObjectHash(object) {
        return __awaiter(this, void 0, void 0, function* () {
            // Get the hash of the given ref
            // E.g. git show --format=%H --shortstat remotes/origin/tyriar/xterm-v3
            const args = this.gitArgsService.getObjectHashArgs(object);
            const output = yield this.exec(...args);
            return output.split(/\r?\n/g)[0].trim();
        });
    }
    getOriginType() {
        return __awaiter(this, void 0, void 0, function* () {
            const url = yield this.getOriginUrl();
            if (url.indexOf('github.com') > 0) {
                return index_1.GitOriginType.github;
            }
            else if (url.indexOf('bitbucket') > 0) {
                return index_1.GitOriginType.bitbucket;
            }
            else if (url.indexOf('visualstudio') > 0) {
                return index_1.GitOriginType.vsts;
            }
            return undefined;
        });
    }
    getOriginUrl() {
        return __awaiter(this, void 0, void 0, function* () {
            try {
                const remoteName = yield this.exec('status', '--porcelain=v1', '-b', '--untracked-files=no').then((branchDetails) => {
                    const matchResult = branchDetails.match(/.*\.\.\.(.*?)\//);
                    return matchResult && matchResult[1] ? matchResult[1] : 'origin';
                });
                const url = yield this.exec('config', `remote.${remoteName}.url`);
                return url.slice(0, url.length - 1);
            }
            catch (_a) {
                return '';
            }
        });
    }
    getRefsContainingCommit(hash) {
        return __awaiter(this, void 0, void 0, function* () {
            const args = this.gitArgsService.getRefsContainingCommitArgs(hash);
            const entries = yield this.exec(...args);
            return entries.split(/\r?\n/g)
                .map(line => line.trim())
                .filter(line => line.length > 0)
                // Remove the '*' prefix from current branch
                .map(line => line.startsWith('*') ? line.substring(1) : line)
                // Remove the '->' from ref pointers (take first portion)
                .map(ref => ref.indexOf(' ') ? ref.split(' ')[0].trim() : ref);
        });
    }
    getLogEntries(pageIndex = 0, pageSize = 0, branch = '', searchText = '', file, lineNumber, author) {
        return __awaiter(this, void 0, void 0, function* () {
            if (pageSize <= 0) {
                // tslint:disable-next-line:no-parameter-reassignment
                const workspace = this.serviceContainer.get(workspace_1.IWorkspaceService);
                pageSize = workspace.getConfiguration('gitHistory').get('pageSize', 100);
            }
            const relativePath = file ? yield this.getGitRelativePath(file) : undefined;
            const args = yield this.gitArgsService.getLogArgs(pageIndex, pageSize, branch, searchText, relativePath, lineNumber, author);
            const gitRootPathPromise = this.getGitRoot();
            const outputPromise = this.exec(...args.logArgs);
            // tslint:disable-next-line:no-suspicious-comment
            // TODO: Disabled due to performance issues https://github.com/DonJayamanne/gitHistoryVSCode/issues/195
            // // Since we're using find and wc (shell commands, we need to execute the command in a shell)
            // const countOutputPromise = this.execInShell(...args.counterArgs)
            //     .then(countValue => parseInt(countValue.trim(), 10))
            //     .catch(ex => {
            //         console.error('Git History: Failed to get commit count');
            //         console.error(ex);
            //         return -1;
            //     });
            const count = -1;
            const [gitRepoPath, output] = yield Promise.all([gitRootPathPromise, outputPromise]);
            // Run another git history, but get file stats instead of the changes
            // const outputWithFileModeChanges = await this.exec(args.fileStatArgs);
            // const entriesWithFileModeChanges = outputWithFileModeChanges.split(LOG_ENTRY_SEPARATOR);
            const items = output
                .split(constants_1.LOG_ENTRY_SEPARATOR)
                .map(entry => {
                if (entry.length === 0) {
                    return;
                }
                return this.logParser.parse(gitRepoPath, entry, constants_1.ITEM_ENTRY_SEPARATOR, constants_1.LOG_FORMAT_ARGS);
            })
                .filter(logEntry => logEntry !== undefined)
                .map(logEntry => logEntry);
            const headHashes = yield this.getHeadHashes();
            const headHashesOnly = headHashes.map(item => item.hash);
            // tslint:disable-next-line:prefer-type-cast
            const headHashMap = new Map(headHashes.map(item => [item.ref, item.hash]));
            items.forEach((item) => __awaiter(this, void 0, void 0, function* () {
                item.gitRoot = gitRepoPath;
                // Check if this the very last commit of a branch
                // Just check if this is a head commit (if shows up in 'git show-ref')
                item.isLastCommit = headHashesOnly.indexOf(item.hash.full) >= 0;
                // Check if this commit has been merged into another branch
                // Do this only if this is a head commit (we don't care otherwise, only the graph needs it)
                if (!item.isLastCommit) {
                    return;
                }
                const refsContainingThisCommit = yield this.getRefsContainingCommit(item.hash.full);
                const hashesOfRefs = refsContainingThisCommit
                    .filter(ref => headHashMap.has(ref))
                    .map(ref => headHashMap.get(ref))
                    // tslint:disable-next-line:possible-timing-attack
                    .filter(hash => hash !== item.hash.full);
                // If we have hashes other than current, then yes it has been merged
                item.isThisLastCommitMerged = hashesOfRefs.length > 0;
            }));
            // tslint:disable-next-line:no-suspicious-comment
            // @ts-ignore
            // tslint:disable-next-line:no-object-literal-type-assertion
            return {
                items,
                count,
                branch,
                file,
                pageIndex,
                pageSize,
                searchText
            };
        });
    }
    getHash(hash) {
        return __awaiter(this, void 0, void 0, function* () {
            const hashes = yield this.exec('show', '--format=%H-%h', '--no-patch', hash);
            const parts = hashes.split(/\r?\n/g).filter(item => item.length > 0)[0].split('-');
            return {
                full: parts[0],
                short: parts[1]
            };
        });
    }
    getCommitDate(hash) {
        return __awaiter(this, void 0, void 0, function* () {
            const args = this.gitArgsService.getCommitDateArgs(hash);
            const output = yield this.exec(...args);
            const lines = output.split(/\r?\n/g).map(line => line.trim()).filter(line => line.length > 0);
            if (lines.length === 0) {
                return;
            }
            const unixTime = parseInt(lines[0], 10);
            if (isNaN(unixTime) || unixTime <= 0) {
                return;
            }
            return new Date(unixTime * 1000);
        });
    }
    getCommit(hash) {
        return __awaiter(this, void 0, void 0, function* () {
            const parentHashesArgs = this.gitArgsService.getCommitParentHashesArgs(hash);
            const parentHashes = yield this.exec(...parentHashesArgs);
            const singleParent = parentHashes.trim().split(' ').filter(item => item.trim().length > 0).length === 1;
            const commitArgs = this.gitArgsService.getCommitArgs(hash);
            const numStartArgs = singleParent ? this.gitArgsService.getCommitWithNumStatArgs(hash) : this.gitArgsService.getCommitWithNumStatArgsForMerge(hash);
            const nameStatusArgs = singleParent ? this.gitArgsService.getCommitNameStatusArgs(hash) : this.gitArgsService.getCommitNameStatusArgsForMerge(hash);
            const gitRootPathPromise = yield this.getGitRoot();
            const commitOutputPromise = yield this.exec(...commitArgs);
            const filesWithNumStatPromise = yield this.exec(...numStartArgs);
            const filesWithNameStatusPromise = yield this.exec(...nameStatusArgs);
            const values = yield Promise.all([gitRootPathPromise, commitOutputPromise, filesWithNumStatPromise, filesWithNameStatusPromise]);
            const gitRootPath = values[0];
            const commitOutput = values[1];
            const filesWithNumStat = values[2];
            const filesWithNameStatus = values[3];
            const entries = commitOutput
                .split(constants_1.LOG_ENTRY_SEPARATOR)
                .map(entry => {
                if (entry.trim().length === 0) {
                    return undefined;
                }
                return this.logParser.parse(gitRootPath, entry, constants_1.ITEM_ENTRY_SEPARATOR, constants_1.LOG_FORMAT_ARGS, filesWithNumStat, filesWithNameStatus);
            })
                .filter(entry => entry !== undefined)
                .map(entry => entry);
            return entries.length > 0 ? entries[0] : undefined;
        });
    }
    getCommitFile(hash, file) {
        return __awaiter(this, void 0, void 0, function* () {
            const gitRootPath = yield this.getGitRoot();
            const filePath = typeof file === 'string' ? file : file.fsPath.toString();
            return new Promise((resolve, reject) => {
                tmp.file({ postfix: path.extname(filePath) }, (err, tmpPath) => __awaiter(this, void 0, void 0, function* () {
                    if (err) {
                        return reject(err);
                    }
                    try {
                        // Sometimes the damn file is in use, lets create a new one everytime.
                        const tmpFilePath = path.join(path.dirname(tmpPath), `${hash}${new Date().getTime()}${path.basename(tmpPath)}`).replace(/\\/g, '/');
                        const tmpFile = path.join(tmpFilePath, path.basename(filePath));
                        yield fs.ensureDir(tmpFilePath);
                        const relativeFilePath = path.relative(gitRootPath, filePath);
                        const fsStream = fs.createWriteStream(tmpFile);
                        yield this.execBinary(fsStream, 'show', `${hash}:${relativeFilePath.replace(/\\/g, '/')}`);
                        fsStream.end();
                        resolve(vscode_1.Uri.file(tmpFile));
                    }
                    catch (ex) {
                        console.error('Git History: failed to get file contents (again)');
                        console.error(ex);
                        reject(ex);
                    }
                }));
            });
        });
    }
    getCommitFileContent(hash, file) {
        return __awaiter(this, void 0, void 0, function* () {
            const gitRootPath = yield this.getGitRoot();
            const filePath = typeof file === 'string' ? file : file.fsPath.toString();
            const relativeFilePath = path.relative(gitRootPath, filePath);
            return this.exec('show', `${hash}:${relativeFilePath.replace(/\\/g, '/')}`);
        });
    }
    getDifferences(hash1, hash2) {
        return __awaiter(this, void 0, void 0, function* () {
            const numStartArgs = this.gitArgsService.getDiffCommitWithNumStatArgs(hash1, hash2);
            const nameStatusArgs = this.gitArgsService.getDiffCommitNameStatusArgs(hash1, hash2);
            const gitRootPathPromise = this.getGitRoot();
            const filesWithNumStatPromise = this.exec(...numStartArgs);
            const filesWithNameStatusPromise = this.exec(...nameStatusArgs);
            const values = yield Promise.all([gitRootPathPromise, filesWithNumStatPromise, filesWithNameStatusPromise]);
            const gitRootPath = values[0];
            const filesWithNumStat = values[1];
            const filesWithNameStatus = values[2];
            const fileStatParser = this.serviceContainer.get(types_2.IFileStatParser);
            return fileStatParser.parse(gitRootPath, filesWithNumStat.split(/\r?\n/g), filesWithNameStatus.split(/\r?\n/g));
        });
    }
    getPreviousCommitHashForFile(hash, file) {
        return __awaiter(this, void 0, void 0, function* () {
            const gitRootPath = yield this.getGitRoot();
            const relativeFilePath = path.relative(gitRootPath, file.fsPath);
            const args = this.gitArgsService.getPreviousCommitHashForFileArgs(hash, relativeFilePath);
            const output = yield this.exec(...args);
            const hashes = output.split(/\r?\n/g).filter(item => item.length > 0)[0].split('-');
            return {
                short: hashes[1],
                full: hashes[0]
            };
        });
    }
    cherryPick(hash) {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.exec('cherry-pick', hash);
        });
    }
    checkout(hash) {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.exec('checkout', hash);
        });
    }
    revertCommit(hash) {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.exec('revert', '--no-edit', hash);
        });
    }
    createBranch(branchName, hash) {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.exec('checkout', '-b', branchName, hash);
        });
    }
    createTag(tagName, hash) {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.exec('tag', '-a', tagName, '-m', tagName, hash);
        });
    }
    removeTag(tagName) {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.exec('tag', '-d', tagName);
        });
    }
    merge(hash) {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.exec('merge', hash);
        });
    }
    rebase(hash) {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.exec('rebase', hash);
        });
    }
    exec(...args) {
        return __awaiter(this, void 0, void 0, function* () {
            const gitRootPath = yield this.getGitRoot();
            return this.gitCmdExecutor.exec(gitRootPath, ...args);
        });
    }
    execBinary(destination, ...args) {
        return __awaiter(this, void 0, void 0, function* () {
            const gitRootPath = yield this.getGitRoot();
            return this.gitCmdExecutor.exec({ cwd: gitRootPath, encoding: 'binary' }, destination, ...args);
        });
    }
};
__decorate([
    cache_1.cache('IGitService'),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], Git.prototype, "getGitRoot", null);
__decorate([
    cache_1.cache('IGitService', 5 * 60 * 1000),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [String]),
    __metadata("design:returntype", Promise)
], Git.prototype, "getGitRoots", null);
__decorate([
    cache_1.cache('IGitService', 10 * 1000),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], Git.prototype, "getHeadHashes", null);
__decorate([
    cache_1.cache('IGitService', 60 * 1000),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], Git.prototype, "getAuthors", null);
__decorate([
    cache_1.cache('IGitService', 30 * 1000),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], Git.prototype, "getBranches", null);
__decorate([
    cache_1.cache('IGitService', 30 * 1000),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], Git.prototype, "getCurrentBranch", null);
__decorate([
    cache_1.cache('IGitService'),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [String]),
    __metadata("design:returntype", Promise)
], Git.prototype, "getObjectHash", null);
__decorate([
    cache_1.cache('IGitService'),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], Git.prototype, "getOriginType", null);
__decorate([
    cache_1.cache('IGitService'),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], Git.prototype, "getOriginUrl", null);
__decorate([
    cache_1.cache('IGitService'),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [String]),
    __metadata("design:returntype", Promise)
], Git.prototype, "getHash", null);
__decorate([
    cache_1.cache('IGitService'),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [String]),
    __metadata("design:returntype", Promise)
], Git.prototype, "getCommitDate", null);
__decorate([
    cache_1.cache('IGitService'),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [String]),
    __metadata("design:returntype", Promise)
], Git.prototype, "getCommit", null);
__decorate([
    cache_1.cache('IGitService'),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [String, Object]),
    __metadata("design:returntype", Promise)
], Git.prototype, "getCommitFile", null);
__decorate([
    cache_1.cache('IGitService'),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [String, String]),
    __metadata("design:returntype", Promise)
], Git.prototype, "getDifferences", null);
__decorate([
    cache_1.cache('IGitService'),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [String, vscode_1.Uri]),
    __metadata("design:returntype", Promise)
], Git.prototype, "getPreviousCommitHashForFile", null);
Git = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_1.IServiceContainer)),
    __param(3, inversify_1.inject(exec_1.IGitCommandExecutor)),
    __param(4, inversify_1.inject(types_2.ILogParser)),
    __param(5, inversify_1.inject(types_3.IGitArgsService)),
    __metadata("design:paramtypes", [Object, String, vscode_1.Uri, Object, Object, Object])
], Git);
exports.Git = Git;
//# sourceMappingURL=git.js.map