"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const baseCommitCommand_1 = require("../baseCommitCommand");
class CheckoutCommand extends baseCommitCommand_1.BaseCommitCommand {
    constructor(commit, handler) {
        super(commit);
        this.handler = handler;
        // const committer = `${commit.logEntry.author!.name} <${commit.logEntry.author!.email}> on ${commit.logEntry.author!.date!.toLocaleString()}`;
        this.setTitle(`$(git-pull-request) Checkout (${commit.logEntry.hash.short}) commit`);
        // this.setDescription(committer);
        // this.setDetail(commit.logEntry.subject);
        this.setCommand('git.commit.checkout');
        this.setCommandArguments([commit]);
    }
    execute() {
        this.handler.checkoutCommit(this.data);
    }
}
exports.CheckoutCommand = CheckoutCommand;
//# sourceMappingURL=checkout.js.map