"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const baseCommitCommand_1 = require("../baseCommitCommand");
class CherryPickCommand extends baseCommitCommand_1.BaseCommitCommand {
    constructor(commit, handler) {
        super(commit);
        this.handler = handler;
        // const committer = `${commit.logEntry.author!.name} <${commit.logEntry.author!.email}> on ${commit.logEntry.author!.date!.toLocaleString()}`;
        this.setTitle(`$(git-pull-request) Cherry pick this (${commit.logEntry.hash.short}) commit into current branch`);
        // this.setDescription(committer);
        // this.setDetail(commit.logEntry.subject);
        this.setCommand('git.commit.cherryPick');
        this.setCommandArguments([commit]);
    }
    execute() {
        this.handler.cherryPickCommit(this.data);
    }
}
exports.CherryPickCommand = CherryPickCommand;
//# sourceMappingURL=cherryPick.js.map