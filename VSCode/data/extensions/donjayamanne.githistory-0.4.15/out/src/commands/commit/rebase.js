"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const baseCommitCommand_1 = require("../baseCommitCommand");
class RebaseCommand extends baseCommitCommand_1.BaseCommitCommand {
    constructor(commit, handler) {
        super(commit);
        this.handler = handler;
        this.setTitle(`$(git-merge) Rebase current branch onto this (${commit.logEntry.hash.short}) commit`);
        this.setCommand('git.commit.rebase');
        this.setCommandArguments([commit]);
    }
    execute() {
        this.handler.rebase(this.data);
    }
}
exports.RebaseCommand = RebaseCommand;
//# sourceMappingURL=rebase.js.map