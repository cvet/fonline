"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const baseCommitCommand_1 = require("../baseCommitCommand");
class RevertCommand extends baseCommitCommand_1.BaseCommitCommand {
    constructor(commit, handler) {
        super(commit);
        this.handler = handler;
        this.setTitle(`$(x) Revert this (${commit.logEntry.hash.short}) commit`);
        this.setCommand('git.commit.revert');
        this.setCommandArguments([commit]);
    }
    execute() {
        this.handler.revertCommit(this.data);
    }
}
exports.RevertCommand = RevertCommand;
//# sourceMappingURL=revert.js.map