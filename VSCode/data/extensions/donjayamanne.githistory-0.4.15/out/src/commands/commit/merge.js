"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const baseCommitCommand_1 = require("../baseCommitCommand");
class MergeCommand extends baseCommitCommand_1.BaseCommitCommand {
    constructor(commit, handler) {
        super(commit);
        this.handler = handler;
        this.setTitle(`$(git-merge) Merge this (${commit.logEntry.hash.short}) commit into current branch`);
        this.setCommand('git.commit.merge');
        this.setCommandArguments([commit]);
    }
    execute() {
        this.handler.merge(this.data);
    }
}
exports.MergeCommand = MergeCommand;
//# sourceMappingURL=merge.js.map