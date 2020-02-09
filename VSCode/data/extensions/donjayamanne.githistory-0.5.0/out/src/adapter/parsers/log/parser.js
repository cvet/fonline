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
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const types_1 = require("../../../ioc/types");
const types_2 = require("../../../types");
const helpers_1 = require("../../helpers");
const types_3 = require("../types");
let LogParser = class LogParser {
    constructor(refsparser, serviceContainer, actionDetailsParser) {
        this.refsparser = refsparser;
        this.serviceContainer = serviceContainer;
        this.actionDetailsParser = actionDetailsParser;
    }
    parse(gitRepoPath, summaryEntry, itemEntrySeparator, logFormatArgs, filesWithNumStat, filesWithNameStatus) {
        const logItems = summaryEntry.split(itemEntrySeparator);
        const fullParentHash = this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.ParentFullHash).split(' ').filter(hash => hash.trim().length > 0);
        const shortParentHash = this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.ParentShortHash).split(' ').filter(hash => hash.trim().length > 0);
        const parents = fullParentHash.map((hash, index) => { return { full: hash, short: shortParentHash[index] }; });
        const committedFiles = this.parserCommittedFiles(gitRepoPath, filesWithNumStat, filesWithNameStatus);
        return {
            gitRoot: gitRepoPath,
            refs: this.refsparser.parse(this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.RefsNames)),
            author: this.getAuthorInfo(logItems, logFormatArgs),
            committer: this.getCommitterInfo(logItems, logFormatArgs),
            body: this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.Body),
            notes: this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.Notes),
            parents,
            committedFiles,
            hash: {
                full: this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.FullHash),
                short: this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.ShortHash)
            },
            tree: {
                full: this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.TreeFullHash),
                short: this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.TreeShortHash)
            },
            subject: this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.Subject)
        };
    }
    parserCommittedFiles(gitRepoPath, filesWithNumStat, filesWithNameStatus) {
        if (filesWithNumStat && filesWithNumStat.length > 0) {
            const numStatFiles = filesWithNumStat.split(/\r?\n/g).map(entry => entry.trim()).filter(entry => entry.length > 0);
            const nameStatusFiles = filesWithNameStatus.split(/\r?\n/g).map(entry => entry.trim()).filter(entry => entry.length > 0);
            const fileStatParserFactory = this.serviceContainer.get(types_3.IFileStatParser);
            return fileStatParserFactory.parse(gitRepoPath, numStatFiles, nameStatusFiles);
        }
        else {
            return [];
        }
    }
    getCommitInfo(logItems, logFormatArgs, info) {
        const commitInfoFormatCode = helpers_1.Helpers.GetCommitInfoFormatCode(info);
        const indexInArgs = logFormatArgs.indexOf(commitInfoFormatCode);
        if (indexInArgs === -1) {
            throw new Error(`Commit Arg '${commitInfoFormatCode}' not found in the args`);
        }
        return logItems[indexInArgs];
    }
    getAuthorInfo(logItems, logFormatArgs) {
        const name = this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.AuthorName);
        const email = this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.AuthorEmail);
        const dateTime = this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.AuthorDateUnixTime);
        return this.actionDetailsParser.parse(name, email, dateTime);
    }
    getCommitterInfo(logItems, logFormatArgs) {
        const name = this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.CommitterName);
        const email = this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.CommitterEmail);
        const dateTime = this.getCommitInfo(logItems, logFormatArgs, types_2.CommitInfo.CommitterDateUnixTime);
        return this.actionDetailsParser.parse(name, email, dateTime);
    }
};
LogParser = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_3.IRefsParser)),
    __param(1, inversify_1.inject(types_1.IServiceContainer)),
    __param(2, inversify_1.inject(types_3.IActionDetailsParser)),
    __metadata("design:paramtypes", [Object, Object, Object])
], LogParser);
exports.LogParser = LogParser;
//# sourceMappingURL=parser.js.map