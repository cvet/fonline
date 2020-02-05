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
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
const vscode_debugadapter_1 = require("vscode-debugadapter");
const urlPathTransformer_1 = require("./urlPathTransformer");
const ChromeUtils = require("../chrome/chromeUtils");
/**
 * Converts a local path from Code to a path on the target. Uses the UrlPathTransforme logic and fallbacks to asking the client if neccesary
 */
class FallbackToClientPathTransformer extends urlPathTransformer_1.UrlPathTransformer {
    constructor(_session) {
        super();
        this._session = _session;
    }
    targetUrlToClientPath(scriptUrl) {
        const _super = name => super[name];
        return __awaiter(this, void 0, void 0, function* () {
            // First try the default UrlPathTransformer transformation
            return _super("targetUrlToClientPath").call(this, scriptUrl).then(filePath => {
                // If it returns a valid non empty file path then that should be a valid result, so we use that
                // If it's an eval script we won't be able to map it, so we also return that
                return (filePath || ChromeUtils.isEvalScript(scriptUrl))
                    ? filePath
                    // In any other case we ask the client to map it as a fallback, and return filePath if there is any failures
                    : this.requestClientToMapURLToFilePath(scriptUrl).catch(rejection => {
                        vscode_debugadapter_1.logger.log('The fallback transformation failed due to: ' + rejection);
                        return filePath;
                    });
            });
        });
    }
    requestClientToMapURLToFilePath(url) {
        return __awaiter(this, void 0, void 0, function* () {
            return new Promise((resolve, reject) => {
                this._session.sendRequest('mapURLToFilePath', { url: url }, FallbackToClientPathTransformer.ASK_CLIENT_TO_MAP_URL_TO_FILE_PATH_TIMEOUT, response => {
                    if (response.success) {
                        vscode_debugadapter_1.logger.log(`The client responded that the url "${url}" maps to the file path "${response.body.filePath}"`);
                        resolve(response.body.filePath);
                    }
                    else {
                        reject(new Error(`The client responded that the url "${url}" couldn't be mapped to a file path due to: ${response.message}`));
                    }
                });
            });
        });
    }
}
FallbackToClientPathTransformer.ASK_CLIENT_TO_MAP_URL_TO_FILE_PATH_TIMEOUT = 500;
exports.FallbackToClientPathTransformer = FallbackToClientPathTransformer;

//# sourceMappingURL=fallbackToClientPathTransformer.js.map
