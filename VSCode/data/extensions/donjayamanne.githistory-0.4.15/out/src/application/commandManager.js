"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
// tslint:disable:no-any
const inversify_1 = require("inversify");
const vscode_1 = require("vscode");
let CommandManager = class CommandManager {
    registerCommand(command, callback, thisArg) {
        return vscode_1.commands.registerCommand(command, callback, thisArg);
    }
    registerTextEditorCommand(command, callback, thisArg) {
        return vscode_1.commands.registerTextEditorCommand(command, callback, thisArg);
    }
    executeCommand(command, ...rest) {
        return vscode_1.commands.executeCommand(command, ...rest);
    }
    getCommands(filterInternal) {
        return vscode_1.commands.getCommands(filterInternal);
    }
};
CommandManager = __decorate([
    inversify_1.injectable()
], CommandManager);
exports.CommandManager = CommandManager;
//# sourceMappingURL=commandManager.js.map