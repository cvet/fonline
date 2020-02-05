// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
'use strict';
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const vscode = require("vscode");
let ApplicationShell = class ApplicationShell {
    // tslint:disable-next-line:no-any
    showInformationMessage(message, options, ...items) {
        return vscode.window.showInformationMessage(message, options, ...items);
    }
    // tslint:disable-next-line:no-any
    showWarningMessage(message, options, ...items) {
        return vscode.window.showWarningMessage(message, options, ...items);
    }
    // tslint:disable-next-line:no-any
    showErrorMessage(message, options, ...items) {
        return vscode.window.showErrorMessage(message, options, ...items);
    }
    // tslint:disable-next-line:no-any
    showQuickPick(items, options, token) {
        return vscode.window.showQuickPick(items, options, token);
    }
    showOpenDialog(options) {
        return vscode.window.showOpenDialog(options);
    }
    showSaveDialog(options) {
        return vscode.window.showSaveDialog(options);
    }
    showInputBox(options, token) {
        return vscode.window.showInputBox(options, token);
    }
    openUrl(_url) {
        // opn(url);
    }
};
ApplicationShell = __decorate([
    inversify_1.injectable()
], ApplicationShell);
exports.ApplicationShell = ApplicationShell;
//# sourceMappingURL=applicationShell.js.map