"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const basePathTransformer_1 = require("./basePathTransformer");
const utils = require("../utils");
const vscode_debugadapter_1 = require("vscode-debugadapter");
const ChromeUtils = require("../chrome/chromeUtils");
const path = require("path");
/**
 * Converts a local path from Code to a path on the target.
 */
class UrlPathTransformer extends basePathTransformer_1.BasePathTransformer {
    constructor() {
        super(...arguments);
        this._clientPathToTargetUrl = new Map();
        this._targetUrlToClientPath = new Map();
    }
    launch(args) {
        this._pathMapping = args.pathMapping;
        return super.launch(args);
    }
    attach(args) {
        this._pathMapping = args.pathMapping;
        return super.attach(args);
    }
    setBreakpoints(source) {
        if (!source.path) {
            // sourceReference script, nothing to do
            return source;
        }
        if (utils.isURL(source.path)) {
            // already a url, use as-is
            vscode_debugadapter_1.logger.log(`Paths.setBP: ${source.path} is already a URL`);
            return source;
        }
        const path = utils.canonicalizeUrl(source.path);
        const url = this.getTargetPathFromClientPath(path);
        if (url) {
            source.path = url;
            vscode_debugadapter_1.logger.log(`Paths.setBP: Resolved ${path} to ${source.path}`);
            return source;
        }
        else {
            vscode_debugadapter_1.logger.log(`Paths.setBP: No target url cached yet for client path: ${path}.`);
            source.path = path;
            return source;
        }
    }
    clearTargetContext() {
        this._clientPathToTargetUrl = new Map();
        this._targetUrlToClientPath = new Map();
    }
    scriptParsed(scriptUrl) {
        return __awaiter(this, void 0, void 0, function* () {
            const clientPath = yield this.targetUrlToClientPath(scriptUrl);
            if (!clientPath) {
                // It's expected that eval scripts (eval://) won't be resolved
                if (!scriptUrl.startsWith(ChromeUtils.EVAL_NAME_PREFIX)) {
                    vscode_debugadapter_1.logger.log(`Paths.scriptParsed: could not resolve ${scriptUrl} to a file with pathMapping/webRoot: ${JSON.stringify(this._pathMapping)}. It may be external or served directly from the server's memory (and that's OK).`);
                }
            }
            else {
                vscode_debugadapter_1.logger.log(`Paths.scriptParsed: resolved ${scriptUrl} to ${clientPath}. pathMapping/webroot: ${JSON.stringify(this._pathMapping)}`);
                const canonicalizedClientPath = utils.canonicalizeUrl(clientPath);
                this._clientPathToTargetUrl.set(canonicalizedClientPath, scriptUrl);
                this._targetUrlToClientPath.set(scriptUrl, clientPath);
                scriptUrl = clientPath;
            }
            return Promise.resolve(scriptUrl);
        });
    }
    stackTraceResponse(response) {
        return __awaiter(this, void 0, void 0, function* () {
            yield Promise.all(response.stackFrames.map(frame => this.fixSource(frame.source)));
        });
    }
    fixSource(source) {
        return __awaiter(this, void 0, void 0, function* () {
            if (source && source.path) {
                // Try to resolve the url to a path in the workspace. If it's not in the workspace,
                // just use the script.url as-is. It will be resolved or cleared by the SourceMapTransformer.
                const clientPath = this.getClientPathFromTargetPath(source.path) ||
                    (yield this.targetUrlToClientPath(source.path));
                // Incoming stackFrames have sourceReference and path set. If the path was resolved to a file in the workspace,
                // clear the sourceReference since it's not needed.
                if (clientPath) {
                    source.path = clientPath;
                    source.sourceReference = undefined;
                    source.origin = undefined;
                    source.name = path.basename(clientPath);
                }
            }
        });
    }
    getTargetPathFromClientPath(clientPath) {
        // If it's already a URL, skip the Map
        return path.isAbsolute(clientPath) ?
            this._clientPathToTargetUrl.get(utils.canonicalizeUrl(clientPath)) :
            clientPath;
    }
    getClientPathFromTargetPath(targetPath) {
        return this._targetUrlToClientPath.get(targetPath);
    }
    /**
     * Overridable for VS to ask Client to resolve path
     */
    targetUrlToClientPath(scriptUrl) {
        return __awaiter(this, void 0, void 0, function* () {
            return ChromeUtils.targetUrlToClientPath(scriptUrl, this._pathMapping);
        });
    }
}
exports.UrlPathTransformer = UrlPathTransformer;

//# sourceMappingURL=urlPathTransformer.js.map
