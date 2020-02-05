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
const sourceMapFactory_1 = require("./sourceMapFactory");
class SourceMaps {
    constructor(pathMapping, sourceMapPathOverrides, enableSourceMapCaching) {
        // Maps absolute paths to generated/authored source files to their corresponding SourceMap object
        this._generatedPathToSourceMap = new Map();
        this._authoredPathToSourceMap = new Map();
        this._sourceMapFactory = new sourceMapFactory_1.SourceMapFactory(pathMapping, sourceMapPathOverrides, enableSourceMapCaching);
    }
    /**
     * Returns the generated script path for an authored source path
     * @param pathToSource - The absolute path to the authored file
     */
    getGeneratedPathFromAuthoredPath(authoredPath) {
        authoredPath = authoredPath.toLowerCase();
        return this._authoredPathToSourceMap.has(authoredPath) ?
            this._authoredPathToSourceMap.get(authoredPath).generatedPath() :
            null;
    }
    mapToGenerated(authoredPath, line, column) {
        authoredPath = authoredPath.toLowerCase();
        return this._authoredPathToSourceMap.has(authoredPath) ?
            this._authoredPathToSourceMap.get(authoredPath)
                .generatedPositionFor(authoredPath, line, column) :
            null;
    }
    mapToAuthored(pathToGenerated, line, column) {
        pathToGenerated = pathToGenerated.toLowerCase();
        return this._generatedPathToSourceMap.has(pathToGenerated) ?
            this._generatedPathToSourceMap.get(pathToGenerated)
                .authoredPositionFor(line, column) :
            null;
    }
    allMappedSources(pathToGenerated) {
        pathToGenerated = pathToGenerated.toLowerCase();
        return this._generatedPathToSourceMap.has(pathToGenerated) ?
            this._generatedPathToSourceMap.get(pathToGenerated).authoredSources :
            null;
    }
    allSourcePathDetails(pathToGenerated) {
        pathToGenerated = pathToGenerated.toLowerCase();
        return this._generatedPathToSourceMap.has(pathToGenerated) ?
            this._generatedPathToSourceMap.get(pathToGenerated).allSourcePathDetails :
            null;
    }
    sourceContentFor(authoredPath) {
        authoredPath = authoredPath.toLowerCase();
        return this._authoredPathToSourceMap.has(authoredPath) ?
            this._authoredPathToSourceMap.get(authoredPath)
                .sourceContentFor(authoredPath) :
            null;
    }
    /**
     * Given a new path to a new script file, finds and loads the sourcemap for that file
     */
    processNewSourceMap(pathToGenerated, originalUrlToGenerated, sourceMapURL, isVSClient = false) {
        return __awaiter(this, void 0, void 0, function* () {
            const sourceMap = yield this._sourceMapFactory.getMapForGeneratedPath(pathToGenerated, originalUrlToGenerated, sourceMapURL, isVSClient);
            if (sourceMap) {
                this._generatedPathToSourceMap.set(pathToGenerated.toLowerCase(), sourceMap);
                sourceMap.authoredSources.forEach(authoredSource => this._authoredPathToSourceMap.set(authoredSource.toLowerCase(), sourceMap));
            }
        });
    }
}
exports.SourceMaps = SourceMaps;

//# sourceMappingURL=sourceMaps.js.map
