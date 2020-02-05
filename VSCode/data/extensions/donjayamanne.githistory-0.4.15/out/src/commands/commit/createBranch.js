"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const baseCommitCommand_1 = require("../baseCommitCommand");
class CreateBranchCommand extends baseCommitCommand_1.BaseCommitCommand {
    constructor(commit, handler) {
        super(commit);
        this.handler = handler;
        // const committer = `${commit.logEntry.author!.name} <${commit.logEntry.author!.email}> on ${commit.logEntry.author!.date!.toLocaleString()}`;
        // this.setTitle(`$(git-branch) Branch from here ${commit.logEntry.hash.short} (${committer})`);
        this.setTitle('$(git-branch) Branch from here');
        // this.setDetail(commit.logEntry.subject);
        this.setCommand('git.commit.createBranch');
        this.setCommandArguments([commit]);
    }
    execute() {
        this.handler.createBranchFromCommit(this.data);
    }
}
exports.CreateBranchCommand = CreateBranchCommand;
//# sourceMappingURL=createBranch.js.map