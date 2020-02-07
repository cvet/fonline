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
const baseCommitCommand_1 = require("../baseCommitCommand");
class CompareCommand extends baseCommitCommand_1.BaseCommitCommand {
    constructor(commit, handler) {
        super(commit);
        this.handler = handler;
        if (handler.selectedCommit) {
            const committer = `${commit.logEntry.author.name} <${commit.logEntry.author.email}> on ${commit.logEntry.author.date.toLocaleString()}`;
            this.setTitle(`$(git-compare) Compare with ${handler.selectedCommit.logEntry.hash.short}, ${committer}`);
            this.setDetail(commit.logEntry.subject);
        }
        this.setCommand('git.commit.compare');
        this.setCommandArguments([commit]);
    }
    preExecute() {
        return __awaiter(this, void 0, void 0, function* () {
            return !!this.handler.selectedCommit;
        });
    }
    execute() {
        this.handler.compare(this.data);
    }
}
exports.CompareCommand = CompareCommand;
//# sourceMappingURL=compare.js.map