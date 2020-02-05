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
class SelectFileForComparison extends baseFileCommitCommand_1.BaseFileCommitCommand {
    constructor(fileCommit, handler) {
        super(fileCommit);
        this.handler = handler;
        this.setTitle('$(git-compare) Select for comparison');
        this.setCommand('git.commit.FileEntry.selectForComparison');
        this.setCommandArguments([fileCommit]);
    }
    preExecute() {
        return __awaiter(this, void 0, void 0, function* () {
            // tslint:disable-next-line:no-suspicious-comment
            // TODO: Not completed
            return false;
        });
    }
    execute() {
        this.handler.select(this.data);
    }
}
exports.SelectFileForComparison = SelectFileForComparison;
//# sourceMappingURL=selectFileForComparison.js.map