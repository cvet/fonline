"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const commitViewExplorer_1 = require("./commit/commitViewExplorer");
const compare_1 = require("./commit/compare");
const compareViewExplorer_1 = require("./commit/compareViewExplorer");
const gitBranchFromCommit_1 = require("./commit/gitBranchFromCommit");
const gitTagFromCommit_1 = require("./commit/gitTagFromCommit");
const gitCheckout_1 = require("./commit/gitCheckout");
const gitCherryPick_1 = require("./commit/gitCherryPick");
const gitCommit_1 = require("./commit/gitCommit");
const gitCommitDetails_1 = require("./commit/gitCommitDetails");
const gitMerge_1 = require("./commit/gitMerge");
const gitRebase_1 = require("./commit/gitRebase");
const revert_1 = require("./commit/revert");
const file_1 = require("./file/file");
const fileCompare_1 = require("./fileCommit/fileCompare");
const fileHistory_1 = require("./fileCommit/fileHistory");
const gitHistory_1 = require("./gitHistory");
const handlerManager_1 = require("./handlerManager");
const types_1 = require("./types");
function registerTypes(serviceManager) {
    serviceManager.addSingleton(types_1.IGitFileHistoryCommandHandler, fileHistory_1.GitFileHistoryCommandHandler);
    serviceManager.addSingleton(types_1.IGitBranchFromCommitCommandHandler, gitBranchFromCommit_1.GitBranchFromCommitCommandHandler);
    serviceManager.addSingleton(types_1.IGitTagFromCommitCommandHandler, gitTagFromCommit_1.GitTagFromCommitCommandHandler);
    serviceManager.addSingleton(types_1.IGitHistoryCommandHandler, gitHistory_1.GitHistoryCommandHandler);
    serviceManager.addSingleton(types_1.IGitCommitCommandHandler, gitCommit_1.GitCommitCommandHandler);
    serviceManager.addSingleton(types_1.IGitCommitViewDetailsCommandHandler, gitCommitDetails_1.GitCommitViewDetailsCommandHandler);
    serviceManager.addSingleton(types_1.IGitCherryPickCommandHandler, gitCherryPick_1.GitCherryPickCommandHandler);
    serviceManager.addSingleton(types_1.IGitCheckoutCommandHandler, gitCheckout_1.GitCheckoutCommandHandler);
    serviceManager.addSingleton(types_1.IGitCompareFileCommandHandler, fileCompare_1.GitCompareFileCommitCommandHandler);
    serviceManager.addSingleton(types_1.IGitCommitViewExplorerCommandHandler, commitViewExplorer_1.GitCommitViewExplorerCommandHandler);
    serviceManager.addSingleton(types_1.IGitCompareCommandHandler, compare_1.GitCompareCommitCommandHandler);
    serviceManager.addSingleton(types_1.IGitCompareCommitViewExplorerCommandHandler, compareViewExplorer_1.GitCompareCommitViewExplorerCommandHandler);
    serviceManager.addSingleton(types_1.IFileCommandHandler, file_1.FileCommandHandler);
    serviceManager.addSingleton(types_1.IGitMergeCommandHandler, gitMerge_1.GitMergeCommandHandler);
    serviceManager.addSingleton(types_1.IGitRebaseCommandHandler, gitRebase_1.GitRebaseCommandHandler);
    serviceManager.addSingleton(types_1.IGitRevertCommandHandler, revert_1.GitRevertCommandHandler);
    [types_1.IGitFileHistoryCommandHandler, types_1.IGitBranchFromCommitCommandHandler, types_1.IGitHistoryCommandHandler,
        types_1.IGitCommitCommandHandler, types_1.IGitCherryPickCommandHandler, types_1.IGitCompareFileCommandHandler,
        types_1.IGitCommitViewExplorerCommandHandler, types_1.IGitCompareCommitViewExplorerCommandHandler,
        types_1.IFileCommandHandler, types_1.IGitMergeCommandHandler, types_1.IGitRebaseCommandHandler,
        types_1.IGitRevertCommandHandler].forEach(serviceIdentifier => {
        const instance = serviceManager.get(serviceIdentifier);
        serviceManager.addSingletonInstance(types_1.ICommandHandler, instance);
    });
    serviceManager.addSingleton(types_1.ICommandHandlerManager, handlerManager_1.CommandHandlerManager);
    const handlerManager = serviceManager.get(types_1.ICommandHandlerManager);
    handlerManager.registerHandlers();
}
exports.registerTypes = registerTypes;
//# sourceMappingURL=serviceRegistry.js.map