"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
var __metadata = (this && this.__metadata) || function (k, v) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(k, v);
};
var __param = (this && this.__param) || function (paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
};
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const types_1 = require("../commandHandlers/types");
const checkout_1 = require("../commands/commit/checkout");
const cherryPick_1 = require("../commands/commit/cherryPick");
const compare_1 = require("../commands/commit/compare");
const createBranch_1 = require("../commands/commit/createBranch");
const createTag_1 = require("../commands/commit/createTag");
const merge_1 = require("../commands/commit/merge");
const rebase_1 = require("../commands/commit/rebase");
const revert_1 = require("../commands/commit/revert");
const selectForComparion_1 = require("../commands/commit/selectForComparion");
const viewDetails_1 = require("../commands/commit/viewDetails");
let CommitCommandFactory = class CommitCommandFactory {
    constructor(branchCreationCommandHandler, tagCreationCommandHandler, cherryPickHandler, checkoutHandler, compareHandler, mergeHandler, rebaseHandler, revertHandler, viewChangeLogHandler) {
        this.branchCreationCommandHandler = branchCreationCommandHandler;
        this.tagCreationCommandHandler = tagCreationCommandHandler;
        this.cherryPickHandler = cherryPickHandler;
        this.checkoutHandler = checkoutHandler;
        this.compareHandler = compareHandler;
        this.mergeHandler = mergeHandler;
        this.rebaseHandler = rebaseHandler;
        this.revertHandler = revertHandler;
        this.viewChangeLogHandler = viewChangeLogHandler;
    }
    createCommands(commit) {
        return __awaiter(this, void 0, void 0, function* () {
            const commands = [
                new cherryPick_1.CherryPickCommand(commit, this.cherryPickHandler),
                new checkout_1.CheckoutCommand(commit, this.checkoutHandler),
                new viewDetails_1.ViewDetailsCommand(commit, this.viewChangeLogHandler),
                new selectForComparion_1.SelectForComparison(commit, this.compareHandler),
                new revert_1.RevertCommand(commit, this.revertHandler),
                new compare_1.CompareCommand(commit, this.compareHandler),
                new merge_1.MergeCommand(commit, this.mergeHandler),
                new rebase_1.RebaseCommand(commit, this.rebaseHandler)
            ];
            return (yield Promise.all(commands.map((cmd) => __awaiter(this, void 0, void 0, function* () {
                const result = cmd.preExecute();
                const available = typeof result === 'boolean' ? result : yield result;
                return available ? cmd : undefined;
            }))))
                .filter(cmd => !!cmd)
                .map(cmd => cmd);
        });
    }
    createNewCommands(commit) {
        return __awaiter(this, void 0, void 0, function* () {
            const commands = [
                new createBranch_1.CreateBranchCommand(commit, this.branchCreationCommandHandler),
                new createTag_1.CreateTagCommand(commit, this.tagCreationCommandHandler)
            ];
            return (yield Promise.all(commands.map((cmd) => __awaiter(this, void 0, void 0, function* () {
                const result = cmd.preExecute();
                const available = typeof result === 'boolean' ? result : yield result;
                return available ? cmd : undefined;
            }))))
                .filter(cmd => !!cmd)
                .map(cmd => cmd);
        });
    }
};
CommitCommandFactory = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_1.IGitBranchFromCommitCommandHandler)),
    __param(1, inversify_1.inject(types_1.IGitTagFromCommitCommandHandler)),
    __param(2, inversify_1.inject(types_1.IGitCherryPickCommandHandler)),
    __param(3, inversify_1.inject(types_1.IGitCheckoutCommandHandler)),
    __param(4, inversify_1.inject(types_1.IGitCompareCommandHandler)),
    __param(5, inversify_1.inject(types_1.IGitMergeCommandHandler)),
    __param(6, inversify_1.inject(types_1.IGitRebaseCommandHandler)),
    __param(7, inversify_1.inject(types_1.IGitRevertCommandHandler)),
    __param(8, inversify_1.inject(types_1.IGitCommitViewDetailsCommandHandler)),
    __metadata("design:paramtypes", [Object, Object, Object, Object, Object, Object, Object, Object, Object])
], CommitCommandFactory);
exports.CommitCommandFactory = CommitCommandFactory;
//# sourceMappingURL=commitFactory.js.map