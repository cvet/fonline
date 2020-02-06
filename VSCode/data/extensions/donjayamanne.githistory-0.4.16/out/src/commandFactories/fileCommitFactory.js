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
const commandManager_1 = require("../application/types/commandManager");
const types_1 = require("../commandHandlers/types");
const compareFile_1 = require("../commands/fileCommit/compareFile");
const compareFileAcrossCommits_1 = require("../commands/fileCommit/compareFileAcrossCommits");
const compareFileWithPrevious_1 = require("../commands/fileCommit/compareFileWithPrevious");
const compareFileWithWorkspace_1 = require("../commands/fileCommit/compareFileWithWorkspace");
const fileHistory_1 = require("../commands/fileCommit/fileHistory");
const selectFileForComparison_1 = require("../commands/fileCommit/selectFileForComparison");
const viewFile_1 = require("../commands/fileCommit/viewFile");
const viewPreviousFile_1 = require("../commands/fileCommit/viewPreviousFile");
const types_2 = require("../common/types");
const types_3 = require("../ioc/types");
let FileCommitCommandFactory = class FileCommitCommandFactory {
    constructor(fileHistoryCommandHandler, fileCompareHandler, commandManager, serviceContainer) {
        this.fileHistoryCommandHandler = fileHistoryCommandHandler;
        this.fileCompareHandler = fileCompareHandler;
        this.commandManager = commandManager;
        this.serviceContainer = serviceContainer;
    }
    createCommands(fileCommit) {
        return __awaiter(this, void 0, void 0, function* () {
            const commands = [
                new viewFile_1.ViewFileCommand(fileCommit, this.fileHistoryCommandHandler),
                new compareFileWithWorkspace_1.CompareFileWithWorkspaceCommand(fileCommit, this.fileHistoryCommandHandler, this.serviceContainer),
                new compareFileWithPrevious_1.CompareFileWithPreviousCommand(fileCommit, this.fileHistoryCommandHandler),
                new selectFileForComparison_1.SelectFileForComparison(fileCommit, this.fileCompareHandler),
                new compareFile_1.CompareFileCommand(fileCommit, this.fileCompareHandler),
                new fileHistory_1.ViewFileHistoryCommand(fileCommit, this.commandManager)
            ];
            return (yield Promise.all(commands.map((cmd) => __awaiter(this, void 0, void 0, function* () {
                return (yield cmd.preExecute()) ? cmd : undefined;
            }))))
                .filter(cmd => !!cmd)
                .map(cmd => cmd);
        });
    }
    getDefaultFileCommand(fileCommit) {
        return __awaiter(this, void 0, void 0, function* () {
            const commands = [];
            if (fileCommit instanceof types_2.CompareFileCommitDetails) {
                commands.push(new compareFileAcrossCommits_1.CompareFileWithAcrossCommitCommand(fileCommit, this.fileHistoryCommandHandler), new viewFile_1.ViewFileCommand(fileCommit, this.fileHistoryCommandHandler), new viewPreviousFile_1.ViewPreviousFileCommand(fileCommit, this.fileHistoryCommandHandler));
            }
            else {
                commands.push(new compareFileWithPrevious_1.CompareFileWithPreviousCommand(fileCommit, this.fileHistoryCommandHandler), new viewFile_1.ViewFileCommand(fileCommit, this.fileHistoryCommandHandler), new viewPreviousFile_1.ViewPreviousFileCommand(fileCommit, this.fileHistoryCommandHandler));
            }
            const availableCommands = (yield Promise.all(commands.map((cmd) => __awaiter(this, void 0, void 0, function* () {
                return (yield cmd.preExecute()) ? cmd : undefined;
            }))))
                .filter(cmd => !!cmd)
                .map(cmd => cmd);
            return availableCommands.length === 0 ? undefined : availableCommands[0];
        });
    }
};
FileCommitCommandFactory = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_1.IGitFileHistoryCommandHandler)),
    __param(1, inversify_1.inject(types_1.IGitCompareFileCommandHandler)),
    __param(2, inversify_1.inject(commandManager_1.ICommandManager)),
    __param(3, inversify_1.inject(types_3.IServiceContainer)),
    __metadata("design:paramtypes", [Object, Object, Object, Object])
], FileCommitCommandFactory);
exports.FileCommitCommandFactory = FileCommitCommandFactory;
//# sourceMappingURL=fileCommitFactory.js.map