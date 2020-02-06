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
const vscode_1 = require("vscode");
const types_1 = require("../../application/types");
const registration_1 = require("../registration");
const types_2 = require("../types");
const mimeTypes_1 = require("./mimeTypes");
let FileCommandHandler = class FileCommandHandler {
    constructor(commandManager, documentManager) {
        this.commandManager = commandManager;
        this.documentManager = documentManager;
    }
    openFile(file) {
        return __awaiter(this, void 0, void 0, function* () {
            if (mimeTypes_1.isTextFile(file)) {
                const doc = yield this.documentManager.openTextDocument(file);
                yield this.documentManager.showTextDocument(doc, { preview: true });
            }
            else {
                yield this.commandManager.executeCommand('vscode.open', file);
            }
        });
    }
};
__decorate([
    registration_1.command('git.openFileInViewer', types_2.IFileCommandHandler),
    __metadata("design:type", Function),
    __metadata("design:paramtypes", [vscode_1.Uri]),
    __metadata("design:returntype", Promise)
], FileCommandHandler.prototype, "openFile", null);
FileCommandHandler = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_1.ICommandManager)),
    __param(1, inversify_1.inject(types_1.IDocumentManager)),
    __metadata("design:paramtypes", [Object, Object])
], FileCommandHandler);
exports.FileCommandHandler = FileCommandHandler;
//# sourceMappingURL=file.js.map