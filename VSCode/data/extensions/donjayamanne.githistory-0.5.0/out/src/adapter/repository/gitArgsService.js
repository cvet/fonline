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
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const types_1 = require("../../types");
const helpers_1 = require("../helpers");
const constants_1 = require("./constants");
let GitArgsService = class GitArgsService {
    constructor(isWindows = /^win/.test(process.platform)) {
        this.isWindows = isWindows;
    }
    getCommitArgs(hash) {
        return ['show', constants_1.LOG_FORMAT, '--decorate=full', '--numstat', hash];
    }
    getCommitParentHashesArgs(hash) {
        return ['log', '--format=%p', '-n1', hash];
    }
    getCommitWithNumStatArgs(hash) {
        return ['show', '--numstat', '--format=', '-M', hash];
    }
    getCommitNameStatusArgs(hash) {
        return ['show', '--name-status', '--format=', '-M', hash];
    }
    getCommitWithNumStatArgsForMerge(hash) {
        return ['show', '--numstat', '--format=', '-M', '--first-parent', hash];
    }
    getCommitNameStatusArgsForMerge(hash) {
        return ['show', '--name-status', '--format=', '-M', '--first-parent', hash];
    }
    getObjectHashArgs(object) {
        return ['show', `--format=${helpers_1.Helpers.GetCommitInfoFormatCode(types_1.CommitInfo.FullHash)}`, '--shortstat', object];
    }
    getRefsContainingCommitArgs(hash) {
        return ['branch', '--all', '--contains', hash];
    }
    getAuthorsArgs() {
        return ['shortlog', '-e', '-s', '-n', 'HEAD'];
    }
    getDiffCommitWithNumStatArgs(hash1, hash2) {
        return ['diff', '--numstat', hash1, hash2];
    }
    getDiffCommitNameStatusArgs(hash1, hash2) {
        return ['diff', '--name-status', hash1, hash2];
    }
    getPreviousCommitHashForFileArgs(hash, file) {
        return ['log', '--format=%H-%h', `${hash}^1`, '-n', '1', '--', file];
    }
    getLogArgs(pageIndex = 0, pageSize = 100, branch = '', searchText = '', relativeFilePath, lineNumber, author) {
        const allBranches = branch.trim().length === 0;
        const currentBranch = branch.trim() === '*';
        const specificBranch = !allBranches && !currentBranch;
        const authorArgs = [];
        if (author && author.length > 0) {
            authorArgs.push(`--author=${author}`);
        }
        const lineArgs = typeof lineNumber === 'number' && relativeFilePath
            ? [`-L${lineNumber},${lineNumber}:${relativeFilePath.replace(/\\/g, '/')}`]
            : [];
        const logArgs = ['log', ...authorArgs, ...lineArgs, '--full-history', constants_1.LOG_FORMAT];
        const fileStatArgs = [
            'log',
            ...authorArgs,
            ...lineArgs,
            '--full-history',
            `--format=${constants_1.LOG_ENTRY_SEPARATOR}${constants_1.newLineFormatCode}`
        ];
        const counterArgs = ['log', ...authorArgs, ...lineArgs, '--full-history', `--format=${constants_1.LOG_ENTRY_SEPARATOR}%h`];
        if (searchText && searchText.length > 0) {
            searchText
                .split(' ')
                .map(text => text.trim())
                .filter(text => text.length > 0)
                .forEach(text => {
                logArgs.push(`--grep=${text}`, '-i');
                fileStatArgs.push(`--grep=${text}`, '-i');
                counterArgs.push(`--grep=${text}`, '-i');
            });
        }
        logArgs.push('--date-order', '--decorate=full', `--skip=${pageIndex * pageSize}`, `--max-count=${pageSize}`);
        fileStatArgs.push('--date-order', '--decorate=full', `--skip=${pageIndex * pageSize}`, `--max-count=${pageSize}`);
        counterArgs.push('--date-order', '--decorate=full');
        // Don't use `--all`, cuz that will result in stashes `ref/stash` being included included in the logs.
        if (allBranches && lineArgs.length === 0) {
            logArgs.push('--branches', '--tags', '--remotes');
            fileStatArgs.push('--branches', '--tags', '--remotes');
            counterArgs.push('--branches', '--tags', '--remotes');
        }
        if (specificBranch && lineArgs.length === 0) {
            logArgs.push(branch);
            fileStatArgs.push(branch);
            counterArgs.push(branch);
        }
        // Check if we need a specific file
        if (relativeFilePath && lineArgs.length === 0) {
            const formattedPath = relativeFilePath.indexOf(' ') > 0 ? `"${relativeFilePath}"` : relativeFilePath;
            logArgs.push('--follow', '--', formattedPath);
            fileStatArgs.push('--follow', '--', formattedPath);
            counterArgs.push('--follow', '--', formattedPath);
        }
        else {
            if (specificBranch && lineArgs.length === 0) {
                logArgs.push('--');
                fileStatArgs.push('--');
                counterArgs.push('--');
            }
        }
        // Count only the number of lines in the log
        if (this.isWindows) {
            counterArgs.push('|', 'find', '/c', '/v', '""');
        }
        else {
            counterArgs.push('|', 'wc', '-l');
        }
        return { logArgs, fileStatArgs, counterArgs };
    }
};
GitArgsService = __decorate([
    inversify_1.injectable(),
    __metadata("design:paramtypes", [Boolean])
], GitArgsService);
exports.GitArgsService = GitArgsService;
//# sourceMappingURL=gitArgsService.js.map