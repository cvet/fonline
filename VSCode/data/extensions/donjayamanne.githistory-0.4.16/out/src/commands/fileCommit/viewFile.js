"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const types_1 = require("../../types");
const baseFileCommitCommand_1 = require("../baseFileCommitCommand");
class ViewFileCommand extends baseFileCommitCommand_1.BaseFileCommitCommand {
    constructor(fileCommit, handler) {
        super(fileCommit);
        this.handler = handler;
        this.setTitle('$(eye) View file contents');
        this.setCommand('git.commit.FileEntry.ViewFileContents');
        this.setCommandArguments([fileCommit]);
    }
    preExecute() {
        return __awaiter(this, void 0, void 0, function* () {
            return this.data.committedFile.status !== types_1.Status.Deleted;
        });
    }
    execute() {
        this.handler.viewFile(this.data);
    }
}
exports.ViewFileCommand = ViewFileCommand;
//# sourceMappingURL=viewFile.js.map