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
const crypto = require("crypto");
const path = require("path");
const os = require("os");
const url = require("url");
const sourceMapUtils = require("./sourceMapUtils");
const utils = require("../utils");
const vscode_debugadapter_1 = require("vscode-debugadapter");
const sourceMap_1 = require("./sourceMap");
const remoteMapper_1 = require("../remoteMapper");
class SourceMapFactory {
    constructor(_pathMapping, _sourceMapPathOverrides, _enableSourceMapCaching) {
        this._pathMapping = _pathMapping;
        this._sourceMapPathOverrides = _sourceMapPathOverrides;
        this._enableSourceMapCaching = _enableSourceMapCaching;
    }
    /**
     * pathToGenerated - an absolute local path or a URL.
     * mapPath - a path relative to pathToGenerated.
     */
    getMapForGeneratedPath(pathToGenerated, originalUrlToGenerated, mapPath, isVSClient = false) {
        let msg = `SourceMaps.getMapForGeneratedPath: Finding SourceMap for ${pathToGenerated} by URI: ${mapPath}`;
        if (this._pathMapping) {
            msg += ` and webRoot/pathMapping: ${JSON.stringify(this._pathMapping)}`;
        }
        vscode_debugadapter_1.logger.log(msg);
        // For an inlined sourcemap, mapPath is a data URI containing a blob of base64 encoded data, starting
        // with a tag like "data:application/json;charset:utf-8;base64,". The data should start after the last comma.
        let sourceMapContentsP;
        if (mapPath.indexOf('data:application/json') >= 0) {
            // Sourcemap is inlined
            vscode_debugadapter_1.logger.log(`SourceMaps.getMapForGeneratedPath: Using inlined sourcemap in ${pathToGenerated}`);
            sourceMapContentsP = Promise.resolve(this.getInlineSourceMapContents(mapPath));
        }
        else {
            const accessPath = remoteMapper_1.isInternalRemotePath(pathToGenerated) && originalUrlToGenerated ?
                originalUrlToGenerated :
                pathToGenerated;
            sourceMapContentsP = this.getSourceMapContent(accessPath, mapPath);
        }
        return sourceMapContentsP.then(contents => {
            if (contents) {
                try {
                    // Throws for invalid JSON
                    return new sourceMap_1.SourceMap(pathToGenerated, contents, this._pathMapping, this._sourceMapPathOverrides, isVSClient);
                }
                catch (e) {
                    vscode_debugadapter_1.logger.error(`SourceMaps.getMapForGeneratedPath: exception while processing path: ${pathToGenerated}, sourcemap: ${mapPath}\n${e.stack}`);
                    return null;
                }
            }
            else {
                return null;
            }
        });
    }
    /**
     * Parses sourcemap contents from inlined base64-encoded data
     */
    getInlineSourceMapContents(sourceMapData) {
        const firstCommaPos = sourceMapData.indexOf(',');
        if (firstCommaPos < 0) {
            vscode_debugadapter_1.logger.log(`SourceMaps.getInlineSourceMapContents: Inline sourcemap is malformed. Starts with: ${sourceMapData.substr(0, 200)}`);
            return null;
        }
        const header = sourceMapData.substr(0, firstCommaPos);
        const data = sourceMapData.substr(firstCommaPos + 1);
        try {
            if (header.indexOf(';base64') !== -1) {
                const buffer = Buffer.from(data, 'base64');
                return buffer.toString();
            }
            else {
                // URI encoded.
                return decodeURI(data);
            }
        }
        catch (e) {
            vscode_debugadapter_1.logger.error(`SourceMaps.getInlineSourceMapContents: exception while processing data uri (${e.stack})`);
        }
        return null;
    }
    /**
     * Resolves a sourcemap's path and loads the data
     */
    getSourceMapContent(pathToGenerated, mapPath) {
        mapPath = sourceMapUtils.resolveMapPath(pathToGenerated, mapPath, this._pathMapping);
        if (!mapPath) {
            return Promise.resolve(null);
        }
        return this.loadSourceMapContents(mapPath).then(contents => {
            if (!contents) {
                // Last ditch effort - just look for a .js.map next to the script
                const mapPathNextToSource = pathToGenerated + '.map';
                if (mapPathNextToSource !== mapPath) {
                    return this.loadSourceMapContents(mapPathNextToSource);
                }
            }
            return contents;
        });
    }
    loadSourceMapContents(mapPathOrURL) {
        let contentsP;
        if (utils.isURL(mapPathOrURL) && !utils.isFileUrl(mapPathOrURL)) {
            vscode_debugadapter_1.logger.log(`SourceMaps.loadSourceMapContents: Downloading sourcemap file from ${mapPathOrURL}`);
            contentsP = this.downloadSourceMapContents(mapPathOrURL).catch(e => {
                vscode_debugadapter_1.logger.log(`SourceMaps.loadSourceMapContents: Could not download sourcemap from ${mapPathOrURL}`);
                return null;
            });
        }
        else {
            mapPathOrURL = utils.canonicalizeUrl(mapPathOrURL);
            contentsP = new Promise((resolve, reject) => {
                vscode_debugadapter_1.logger.log(`SourceMaps.loadSourceMapContents: Reading local sourcemap file from ${mapPathOrURL}`);
                fs.readFile(mapPathOrURL, (err, data) => {
                    if (err) {
                        vscode_debugadapter_1.logger.log(`SourceMaps.loadSourceMapContents: Could not read sourcemap file - ` + err.message);
                        resolve(null);
                    }
                    else {
                        resolve(data && data.toString());
                    }
                });
            });
        }
        return contentsP;
    }
    downloadSourceMapContents(sourceMapUri) {
        return __awaiter(this, void 0, void 0, function* () {
            try {
                return yield this._downloadSourceMapContents(sourceMapUri);
            }
            catch (e) {
                if (url.parse(sourceMapUri).hostname === 'localhost') {
                    vscode_debugadapter_1.logger.log(`Sourcemaps.downloadSourceMapContents: downlading from 127.0.0.1 instead of localhost`);
                    return this._downloadSourceMapContents(sourceMapUri.replace('localhost', '127.0.0.1'));
                }
                throw e;
            }
        });
    }
    _downloadSourceMapContents(sourceMapUri) {
        return __awaiter(this, void 0, void 0, function* () {
            // use sha256 to ensure the hash value can be used in filenames
            let cachedSourcemapPath;
            if (this._enableSourceMapCaching) {
                const hash = crypto.createHash('sha256').update(sourceMapUri).digest('hex');
                const cachePath = path.join(os.tmpdir(), 'com.microsoft.VSCode', 'node-debug2', 'sm-cache');
                cachedSourcemapPath = path.join(cachePath, hash);
                const exists = utils.existsSync(cachedSourcemapPath);
                if (exists) {
                    vscode_debugadapter_1.logger.log(`Sourcemaps.downloadSourceMapContents: Reading cached sourcemap file from ${cachedSourcemapPath}`);
                    return this.loadSourceMapContents(cachedSourcemapPath);
                }
            }
            const responseText = yield utils.getURL(sourceMapUri);
            if (cachedSourcemapPath && this._enableSourceMapCaching) {
                vscode_debugadapter_1.logger.log(`Sourcemaps.downloadSourceMapContents: Caching sourcemap file at ${cachedSourcemapPath}`);
                yield utils.writeFileP(cachedSourcemapPath, responseText);
            }
            return responseText;
        });
    }
}
exports.SourceMapFactory = SourceMapFactory;

//# sourceMappingURL=sourceMapFactory.js.map
