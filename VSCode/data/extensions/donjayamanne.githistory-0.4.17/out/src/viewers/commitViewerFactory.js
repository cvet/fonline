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
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const commandManager_1 = require("../application/types/commandManager");
const types_1 = require("../commandFactories/types");
const types_2 = require("../formatters/types");
const nodeBuilder_1 = require("../nodes/nodeBuilder");
const types_3 = require("../nodes/types");
const types_4 = require("../platform/types");
const types_5 = require("../types");
const commitViewer_1 = require("./commitViewer");
let CommitViewerFactory = class CommitViewerFactory {
    constructor(outputChannel, commitFormatter, commandManager, platformService, fileCommitFactory, standardNodeFactory, compareNodeFactory) {
        this.outputChannel = outputChannel;
        this.commitFormatter = commitFormatter;
        this.commandManager = commandManager;
        this.platformService = platformService;
        this.fileCommitFactory = fileCommitFactory;
        this.standardNodeFactory = standardNodeFactory;
        this.compareNodeFactory = compareNodeFactory;
    }
    getCommitViewer() {
        if (this.commitViewer) {
            return this.commitViewer;
        }
        return this.commitViewer = new commitViewer_1.CommitViewer(this.outputChannel, this.commitFormatter, this.commandManager, new nodeBuilder_1.NodeBuilder(this.fileCommitFactory, this.standardNodeFactory, this.platformService), 'commitViewProvider', 'git.commit.view.show');
    }
    getCompareCommitViewer() {
        if (this.compareViewer) {
            return this.compareViewer;
        }
        return this.compareViewer = new commitViewer_1.CommitViewer(this.outputChannel, this.commitFormatter, this.commandManager, new nodeBuilder_1.NodeBuilder(this.fileCommitFactory, this.compareNodeFactory, this.platformService), 'compareCommitViewProvider', 'git.commit.compare.view.show');
    }
};
CommitViewerFactory = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_5.IOutputChannel)),
    __param(1, inversify_1.inject(types_2.ICommitViewFormatter)),
    __param(2, inversify_1.inject(commandManager_1.ICommandManager)),
    __param(3, inversify_1.inject(types_4.IPlatformService)),
    __param(4, inversify_1.inject(types_1.IFileCommitCommandFactory)),
    __param(5, inversify_1.inject(types_3.INodeFactory)), __param(5, inversify_1.named('standard')),
    __param(6, inversify_1.inject(types_3.INodeFactory)), __param(6, inversify_1.named('comparison')),
    __metadata("design:paramtypes", [Object, Object, Object, Object, Object, Object, Object])
], CommitViewerFactory);
exports.CommitViewerFactory = CommitViewerFactory;
//# sourceMappingURL=commitViewerFactory.js.map