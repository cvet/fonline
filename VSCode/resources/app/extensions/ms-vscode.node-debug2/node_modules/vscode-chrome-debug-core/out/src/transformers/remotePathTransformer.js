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
const fs = require("fs");
const path = require("path");
const vscode_debugadapter_1 = require("vscode-debugadapter");
const errors = require("../errors");
const urlPathTransformer_1 = require("../transformers/urlPathTransformer");
const utils = require("../utils");
const nls = require("vscode-nls");
const localize = nls.loadMessageBundle(__filename);
/**
 * Converts a local path from Code to a path on the target.
 */
class RemotePathTransformer extends urlPathTransformer_1.UrlPathTransformer {
    launch(args) {
        const _super = name => super[name];
        return __awaiter(this, void 0, void 0, function* () {
            yield _super("launch").call(this, args);
            return this.init(args);
        });
    }
    attach(args) {
        const _super = name => super[name];
        return __awaiter(this, void 0, void 0, function* () {
            yield _super("attach").call(this, args);
            return this.init(args);
        });
    }
    init(args) {
        return __awaiter(this, void 0, void 0, function* () {
            if ((args.localRoot && !args.remoteRoot) || (args.remoteRoot && !args.localRoot)) {
                throw new Error(localize(0, null));
            }
            // Maybe validate that it's absolute, for either windows or unix
            this._remoteRoot = args.remoteRoot;
            // Validate that localRoot is absolute and exists
            let localRootP = Promise.resolve();
            if (args.localRoot) {
                const localRoot = args.localRoot;
                if (!path.isAbsolute(localRoot)) {
                    return Promise.reject(errors.attributePathRelative('localRoot', localRoot));
                }
                localRootP = new Promise((resolve, reject) => {
                    fs.exists(localRoot, exists => {
                        if (!exists) {
                            reject(errors.attributePathNotExist('localRoot', localRoot));
                        }
                        this._localRoot = localRoot;
                        resolve();
                    });
                });
            }
            return localRootP;
        });
    }
    scriptParsed(scriptPath) {
        const _super = name => super[name];
        return __awaiter(this, void 0, void 0, function* () {
            if (!this.shouldMapPaths(scriptPath)) {
                scriptPath = yield _super("scriptParsed").call(this, scriptPath);
            }
            scriptPath = this.getClientPathFromTargetPath(scriptPath) || scriptPath;
            return scriptPath;
        });
    }
    stackTraceResponse(response) {
        return __awaiter(this, void 0, void 0, function* () {
            yield Promise.all(response.stackFrames.map(stackFrame => this.fixSource(stackFrame.source)));
        });
    }
    fixSource(source) {
        const _super = name => super[name];
        return __awaiter(this, void 0, void 0, function* () {
            yield _super("fixSource").call(this, source);
            const remotePath = source && source.path;
            if (remotePath) {
                const localPath = this.getClientPathFromTargetPath(remotePath) || remotePath;
                if (utils.existsSync(localPath)) {
                    source.path = localPath;
                    source.sourceReference = undefined;
                    source.origin = undefined;
                }
            }
        });
    }
    shouldMapPaths(remotePath) {
        // Map paths only if localRoot/remoteRoot are set, and the remote path is absolute on some system
        return !!this._localRoot && !!this._remoteRoot && (path.posix.isAbsolute(remotePath) || path.win32.isAbsolute(remotePath) || utils.isFileUrl(remotePath));
    }
    getClientPathFromTargetPath(remotePath) {
        remotePath = super.getClientPathFromTargetPath(remotePath) || remotePath;
        // Map as non-file-uri because remoteRoot won't expect a file uri
        remotePath = utils.fileUrlToPath(remotePath);
        if (!this.shouldMapPaths(remotePath))
            return '';
        const relPath = relative(this._remoteRoot, remotePath);
        if (relPath.startsWith('../'))
            return '';
        let localPath = join(this._localRoot, relPath);
        localPath = utils.fixDriveLetterAndSlashes(localPath);
        vscode_debugadapter_1.logger.log(`Mapped remoteToLocal: ${remotePath} -> ${localPath}`);
        return localPath;
    }
    getTargetPathFromClientPath(localPath) {
        localPath = super.getTargetPathFromClientPath(localPath) || localPath;
        if (!this.shouldMapPaths(localPath))
            return localPath;
        const relPath = relative(this._localRoot, localPath);
        if (relPath.startsWith('../'))
            return '';
        let remotePath = join(this._remoteRoot, relPath);
        remotePath = utils.fixDriveLetterAndSlashes(remotePath, /*uppercaseDriveLetter=*/ true);
        vscode_debugadapter_1.logger.log(`Mapped localToRemote: ${localPath} -> ${remotePath}`);
        return remotePath;
    }
}
exports.RemotePathTransformer = RemotePathTransformer;
/**
 * Cross-platform path.relative
 */
function relative(a, b) {
    return a.match(/^[A-Za-z]:/) ?
        path.win32.relative(a, b) :
        path.posix.relative(a, b);
}
/**
 * Cross-platform path.join
 */
function join(a, b) {
    return a.match(/^[A-Za-z]:/) ?
        path.win32.join(a, b) :
        utils.forceForwardSlashes(path.posix.join(a, b));
}

//# sourceMappingURL=remotePathTransformer.js.map
