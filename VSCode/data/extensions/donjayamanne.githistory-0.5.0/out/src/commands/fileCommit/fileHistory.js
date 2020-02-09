"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const baseFileCommitCommand_1 = require("../baseFileCommitCommand");
class ViewFileHistoryCommand extends baseFileCommitCommand_1.BaseFileCommitCommand {
    constructor(fileCommit, commandManager) {
        super(fileCommit);
        this.commandManager = commandManager;
        this.setTitle('$(history) View file history');
        this.setCommand('git.viewFileHistory');
        this.setCommandArguments([fileCommit]);
    }
    execute() {
        this.commandManager.executeCommand(this.command, ...this.arguments);
    }
}
exports.ViewFileHistoryCommand = ViewFileHistoryCommand;
//# sourceMappingURL=fileHistory.js.map