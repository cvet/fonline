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
const fs = require("async-file");
const customActions_1 = require("./customActions");
const paths_utils_1 = require("../utils/paths.utils");
const vscode = require("vscode");
class ConfigurationParse {
    isWhenExist(path, whenPath) {
        return __awaiter(this, void 0, void 0, function* () {
            try {
                if (whenPath && (yield fs.exists(path + "\\" + whenPath))) {
                    return true;
                }
            }
            catch (e) {
                console.log("TerminalStatusBar");
                console.log(e);
            }
            return false;
        });
    }
    parse(fullPath) {
        return __awaiter(this, void 0, void 0, function* () {
            const fileContent = yield fs.readTextFile(fullPath);
            try {
                this.create(JSON.parse(fileContent));
            }
            catch (_a) {
                vscode.window.showInformationMessage("Terminal Status bar: Problem with parsing the json");
            }
        });
    }
    create(fileContentParseJson) {
        return __awaiter(this, void 0, void 0, function* () {
            customActions_1.customActions.removeAll();
            fileContentParseJson.forEach((actionsInfo) => __awaiter(this, void 0, void 0, function* () {
                if (actionsInfo.hide) {
                    return;
                }
                if (actionsInfo.when &&
                    !(yield this.isWhenExist(paths_utils_1.getRealPath(actionsInfo.path), actionsInfo.when))) {
                    return;
                }
                customActions_1.customActions.add(actionsInfo);
            }));
        });
    }
}
exports.configurationParse = new ConfigurationParse();
//# sourceMappingURL=configurationParse.js.map