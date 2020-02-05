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
const baseFileCommitCommand_1 = require("../baseFileCommitCommand");
class CompareFileCommand extends baseFileCommitCommand_1.BaseFileCommitCommand {
    constructor(fileCommit, handler) {
        super(fileCommit);
        this.handler = handler;
        if (handler.selectedCommit) {
            this.setTitle(`$(git-compare) Compare with ${handler.selectedCommit.logEntry.hash.short}`);
        }
        this.setCommand('git.commit.FileEntry.compare');
        this.setCommandArguments([fileCommit]);
    }
    preExecute() {
        return __awaiter(this, void 0, void 0, function* () {
            // tslint:disable-next-line:no-suspicious-comment
            // TODO: Not completed
            return false;
            // return !!this.handler.selectedCommit;
        });
    }
    execute() {
        this.handler.compare(this.data);
    }
}
exports.CompareFileCommand = CompareFileCommand;
//# sourceMappingURL=compareFile.js.map