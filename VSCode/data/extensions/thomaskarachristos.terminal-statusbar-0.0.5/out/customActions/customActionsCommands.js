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
const vscode = require("vscode");
const fs = require("async-file");
const configurationParse_1 = require("./configurationParse");
const commandsNames_1 = require("../model/commandsNames");
class ConfigurationFile {
    constructor() {
        this.configurationFileName = "//sideBarActions.json";
    }
    checkIfFileExistAndCrete(context) {
        return __awaiter(this, void 0, void 0, function* () {
            this.globalStoragePath = context.globalStoragePath;
            this.fullPath = this.globalStoragePath + this.configurationFileName;
            if (!(yield fs.exists(this.globalStoragePath))) {
                yield fs.createDirectory(this.globalStoragePath);
            }
            if (!(yield fs.exists(this.fullPath))) {
                yield this.createFile();
            }
        });
    }
    createFile() {
        return __awaiter(this, void 0, void 0, function* () {
            yield fs.writeTextFile(this.fullPath, `[ ]`);
        });
    }
    watchConfigurationChanges(context) {
        fs.watchFile(this.fullPath, (event) => __awaiter(this, void 0, void 0, function* () {
            this.parseFile();
        }));
    }
    parseFile() {
        return __awaiter(this, void 0, void 0, function* () {
            configurationParse_1.configurationParse.parse(this.fullPath);
        });
    }
    editConfigurationFile(context) {
        let editTerminals = vscode.commands.registerCommand(commandsNames_1.commandsNames.editConfigurationFIle, () => {
            let terminalsJsonPath = path.join(context.globalStoragePath, this.configurationFileName);
            vscode.window.showTextDocument(vscode.Uri.file(terminalsJsonPath));
        });
        context.subscriptions.push(editTerminals);
    }
}
exports.configurationFile = new ConfigurationFile();
//# sourceMappingURL=customActionsCommands.js.map