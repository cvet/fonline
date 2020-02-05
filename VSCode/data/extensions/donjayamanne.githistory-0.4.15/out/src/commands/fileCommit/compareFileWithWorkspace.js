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
const path = require("path");
const types_1 = require("../../platform/types");
const baseFileCommitCommand_1 = require("../baseFileCommitCommand");
class CompareFileWithWorkspaceCommand extends baseFileCommitCommand_1.BaseFileCommitCommand {
    constructor(fileCommit, handler, serviceContainer) {
        super(fileCommit);
        this.handler = handler;
        this.serviceContainer = serviceContainer;
        this.setTitle('$(git-compare) Compare against workspace file');
        this.setCommand('git.commit.FileEntry.CompareAgainstWorkspace');
        this.setCommandArguments([fileCommit]);
    }
    preExecute() {
        return __awaiter(this, void 0, void 0, function* () {
            const localFile = path.join(this.data.workspaceFolder, this.data.committedFile.relativePath);
            const fileSystem = this.serviceContainer.get(types_1.IFileSystem);
            return fileSystem.fileExistsAsync(localFile);
        });
    }
    execute() {
        this.handler.compareFileWithWorkspace(this.data);
    }
}
exports.CompareFileWithWorkspaceCommand = CompareFileWithWorkspaceCommand;
//# sourceMappingURL=compareFileWithWorkspace.js.map