"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const types_1 = require("../common/types");
const types_2 = require("./types");
let StandardNodeFactory = class StandardNodeFactory {
    createDirectoryNode(commit, relativePath) {
        return new types_2.DirectoryNode(commit, relativePath);
    }
    createFileNode(commit, committedFile) {
        return new types_2.FileNode(new types_1.FileCommitDetails(commit.workspaceFolder, commit.branch, commit.logEntry, committedFile));
    }
};
StandardNodeFactory = __decorate([
    inversify_1.injectable()
], StandardNodeFactory);
exports.StandardNodeFactory = StandardNodeFactory;
let ComparisonNodeFactory = class ComparisonNodeFactory {
    createDirectoryNode(commit, relativePath) {
        return new types_2.DirectoryNode(commit, relativePath);
    }
    createFileNode(commit, committedFile) {
        const compareCommit = commit;
        return new types_2.FileNode(new types_1.CompareFileCommitDetails(compareCommit, compareCommit.rightCommit, committedFile));
    }
};
ComparisonNodeFactory = __decorate([
    inversify_1.injectable()
], ComparisonNodeFactory);
exports.ComparisonNodeFactory = ComparisonNodeFactory;
//# sourceMappingURL=factory.js.map