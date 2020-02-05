"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const baseCommitCommand_1 = require("../baseCommitCommand");
class CreateTagCommand extends baseCommitCommand_1.BaseCommitCommand {
    constructor(commit, handler) {
        super(commit);
        this.handler = handler;
        this.setTitle(`$(tag) Tag commit ${commit.logEntry.hash.short}`);
        this.setCommand('git.commit.createTag');
        this.setCommandArguments([commit]);
    }
    execute() {
        this.handler.createTagFromCommit(this.data);
    }
}
exports.CreateTagCommand = CreateTagCommand;
//# sourceMappingURL=createTag.js.map