"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const types_1 = require("../../common/types");
const baseCommitCommand_1 = require("../baseCommitCommand");
class SelectForComparison extends baseCommitCommand_1.BaseCommitCommand {
    constructor(commit, handler) {
        super(commit);
        this.handler = handler;
        const committer = `${commit.logEntry.author.name} <${commit.logEntry.author.email}> on ${commit.logEntry.author.date.toLocaleString()}`;
        this.setTitle(`$(git-compare) Select this commit (${committer}) for comparison`);
        // this.setDetail(commit.logEntry.subject);
        this.setCommand('git.commit.compare.selectForComparison');
        this.setCommandArguments([types_1.CommitDetails]);
    }
    execute() {
        this.handler.select(this.data);
    }
}
exports.SelectForComparison = SelectForComparison;
//# sourceMappingURL=selectForComparion.js.map