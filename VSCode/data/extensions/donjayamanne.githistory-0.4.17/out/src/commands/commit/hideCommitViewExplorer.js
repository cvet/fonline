"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const baseCommitCommand_1 = require("../baseCommitCommand");
class HideCommitViewExplorerCommand extends baseCommitCommand_1.BaseCommitCommand {
    constructor(commit, commandManager) {
        super(commit);
        this.commandManager = commandManager;
        this.setTitle('Hide Commit View Explorer');
        this.setCommand('git.commit.view.hide');
    }
    execute() {
        this.commandManager.executeCommand(this.command);
    }
}
exports.HideCommitViewExplorerCommand = HideCommitViewExplorerCommand;
//# sourceMappingURL=hideCommitViewExplorer.js.map