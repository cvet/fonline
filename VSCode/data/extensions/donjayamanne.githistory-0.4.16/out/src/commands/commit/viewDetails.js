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
class ViewDetailsCommand extends baseCommitCommand_1.BaseCommitCommand {
    constructor(commit, handler) {
        super(commit);
        this.handler = handler;
        this.setTitle('$(eye) View Change log');
    }
    preExecute() {
        return __awaiter(this, void 0, void 0, function* () {
            // Disable for now, useless command.
            return false;
        });
    }
    execute() {
        this.handler.viewDetails(this.data);
    }
}
exports.ViewDetailsCommand = ViewDetailsCommand;
//# sourceMappingURL=viewDetails.js.map