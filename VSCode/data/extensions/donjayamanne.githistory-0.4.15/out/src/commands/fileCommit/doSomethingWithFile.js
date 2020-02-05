"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const baseFileCommitCommand_1 = require("../baseFileCommitCommand");
class DoSomethingWithFileCommitCommand extends baseFileCommitCommand_1.BaseFileCommitCommand {
    constructor(fileCommit, handler) {
        super(fileCommit);
        this.handler = handler;
        this.setTitle('');
        this.setCommand('git.commit.file.select');
    }
    execute() {
        this.handler.doSomethingWithFile(this.data);
    }
}
exports.DoSomethingWithFileCommitCommand = DoSomethingWithFileCommitCommand;
//# sourceMappingURL=doSomethingWithFile.js.map