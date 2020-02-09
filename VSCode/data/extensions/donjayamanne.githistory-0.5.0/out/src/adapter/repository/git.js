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
    constructor(repo, serviceContainer, gitCmdExecutor, logParser, gitArgsService) {
        this.repo = repo;
        this.serviceContainer = serviceContainer;
        this.gitCmdExecutor = gitCmdExecutor;
        this.logParser = logParser;
        this.gitArgsService = gitArgsService;
    }
    getGitRoot() {
        return __awaiter(this, void 0, void 0, function* () {
            return this.repo.rootUri.fsPath;
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
            return this.repo.state.refs.filter(x => x.type <= 1).map(x => { return { ref: x.name, hash: x.commit }; });
        });
    }
    getBranches() {
        return __awaiter(this, void 0, void 0, function* () {
            const currentBranchName = yield this.getCurrentBranch();
            const gitRootPath = this.repo.rootUri.fsPath;
            const localBranches = this.repo.state.refs.filter(x => x.type === 0);
            return yield Promise.all(localBranches.map((x) => __awaiter(this, void 0, void 0, function* () {
                // tslint:disable-next-line:no-object-literal-type-assertion
                let originUrl = yield this.getOriginUrl(x.name);
                let originType = yield this.getOriginType(originUrl);
                return {
                    gitRoot: gitRootPath,
                    name: x.name,
                    remote: originUrl,
                    remoteType: originType,
                    current: currentBranchName === x.name
                };
            })));
        });
    }
    getCurrentBranch() {
        return __awaiter(this, void 0, void 0, function* () {
            return this.repo.state.HEAD.name || '';
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
    getOriginType(url) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!url) {
                url = yield this.getOriginUrl();
            }
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
    getOriginUrl(branchName) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!branchName) {
                branchName = yield this.getCurrentBranch();
            }
            const branch = yield this.repo.getBranch(branchName);
            if (branch.upstream) {
                const remoteIndex = this.repo.state.remotes.findIndex(x => x.name === branch.upstream.remote);
                return this.repo.state.remotes[remoteIndex].fetchUrl || '';
            }
            return '';
        });
    }
    getRefsContainingCommit(hash) {
        return __awaiter(this, void 0, void 0, function* () {
            // tslint:disable-next-line:possible-timing-attack
            return this.repo.state.refs.filter(x => x.commit === hash).map(x => x.name || '');
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
            const args = this.gitArgsService.getLogArgs(pageIndex, pageSize, branch, searchText, relativePath, lineNumber, author);
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
            //const headHashMap = new Map<string, string>(headHashes.map(item => [item.ref, item.hash] as [string, string]));
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
                /*const refsContainingThisCommit = await this.getRefsContainingCommit(item.hash.full);
                const hashesOfRefs = refsContainingThisCommit
                    .filter(ref => headHashMap.has(ref))
                    .map(ref => headHashMap.get(ref)!)
                    // tslint:disable-next-line:possible-timing-attack
                    .filter(hash => hash !== item.hash.full);
                // If we have hashes other than current, then yes it has been merged
                item.isThisLastCommitMerged = hashesOfRefs.length > 0;*/
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
    getCommit(hash) {
        return __awaiter(this, void 0, void 0, function* () {
            //const parentHashesArgs = this.gitArgsService.getCommitParentHashesArgs(hash);
            //const parentHashes = await this.exec(...parentHashesArgs);
            //const singleParent = parentHashes.trim().split(' ').filter(item => item.trim().length > 0).length === 1;
            const commitArgs = this.gitArgsService.getCommitArgs(hash);
            const nameStatusArgs = this.gitArgsService.getCommitNameStatusArgsForMerge(hash);
            const gitRootPath = yield this.getGitRoot();
            const commitOutput = yield this.exec(...commitArgs);
            const filesWithNumStat = commitOutput.split("\n\n", 2)[1];
            const filesWithNameStatus = yield this.exec(...nameStatusArgs);
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
            //const gitRootPath = await this.getGitRoot();
            const filePath = typeof file === 'string' ? file : file.fsPath.toString();
            const content = yield this.repo.show(hash, filePath);
            return new Promise((resolve, reject) => {
                tmp.file({ postfix: path.extname(filePath) }, (err, tmpPath) => __awaiter(this, void 0, void 0, function* () {
                    if (err) {
                        return reject(err);
                    }
                    try {
                        const tmpFilePath = path.join(path.dirname(tmpPath), `${hash}${new Date().getTime()}${path.basename(tmpPath)}`).replace(/\\/g, '/');
                        const tmpFile = path.join(tmpFilePath, path.basename(filePath));
                        yield fs.ensureDir(tmpFilePath);
                        yield fs.writeFile(tmpFile, content);
                        resolve(vscode_1.Uri.file(tmpFile));
                    }
                    catch (ex) {
                        // tslint:disable-next-line:no-console
                        console.error('Git History: failed to get file contents (again)');
                        // tslint:disable-next-line:no-console
                        console.error(ex);
                        reject(ex);
                    }
                }));
            });
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
    reset(hash, hard = false) {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.exec('reset', hard ? '--hard' : '--soft', hash);
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
};
__decorate([
    cache_1.cache('IGitService', 60 * 1000),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", []),
    __metadata("design:returntype", Promise)
], Git.prototype, "getAuthors", null);
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
    __param(1, inversify_1.inject(types_1.IServiceContainer)),
    __param(2, inversify_1.inject(exec_1.IGitCommandExecutor)),
    __param(3, inversify_1.inject(types_2.ILogParser)),
    __param(4, inversify_1.inject(types_3.IGitArgsService)),
    __metadata("design:paramtypes", [Object, Object, Object, Object, Object])
], Git);
exports.Git = Git;
//# sourceMappingURL=git.js.map