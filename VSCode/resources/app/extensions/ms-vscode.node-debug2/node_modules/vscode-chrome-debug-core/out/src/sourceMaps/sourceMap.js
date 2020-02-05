"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const source_map_1 = require("source-map");
const path = require("path");
const sourceMapUtils = require("./sourceMapUtils");
const utils = require("../utils");
const vscode_debugadapter_1 = require("vscode-debugadapter");
class SourceMap {
    /**
     * generatedPath: an absolute local path or a URL
     * json: sourcemap contents as string
     */
    constructor(generatedPath, json, pathMapping, sourceMapPathOverrides, isVSClient = false) {
        this._authoredPathCaseMap = new Map(); // Maintain pathCase map because VSCode is case sensitive
        this._generatedPath = generatedPath;
        const sm = JSON.parse(json);
        vscode_debugadapter_1.logger.log(`SourceMap: creating for ${generatedPath}`);
        vscode_debugadapter_1.logger.log(`SourceMap: sourceRoot: ${sm.sourceRoot}`);
        if (sm.sourceRoot && sm.sourceRoot.toLowerCase() === '/source/') {
            vscode_debugadapter_1.logger.log('Warning: if you are using gulp-sourcemaps < 2.0 directly or indirectly, you may need to set sourceRoot manually in your build config, if your files are not actually under a directory called /source');
        }
        vscode_debugadapter_1.logger.log(`SourceMap: sources: ${JSON.stringify(sm.sources)}`);
        if (pathMapping) {
            vscode_debugadapter_1.logger.log(`SourceMap: pathMapping: ${JSON.stringify(pathMapping)}`);
        }
        // Absolute path
        const computedSourceRoot = sourceMapUtils.getComputedSourceRoot(sm.sourceRoot, this._generatedPath, pathMapping);
        // Overwrite the sourcemap's sourceRoot with the version that's resolved to an absolute path,
        // so the work above only has to be done once
        this._originalSourceRoot = sm.sourceRoot;
        this._originalSources = sm.sources;
        sm.sourceRoot = null;
        // sm.sources are initially relative paths, file:/// urls, made-up urls like webpack:///./app.js, or paths that start with /.
        // resolve them to file:/// urls, using computedSourceRoot, to be simpler and unambiguous, since
        // it needs to look them up later in exactly the same format.
        this._sources = sm.sources.map(sourcePath => {
            if (sourceMapPathOverrides) {
                const fullSourceEntry = sourceMapUtils.getFullSourceEntry(this._originalSourceRoot, sourcePath);
                const mappedFullSourceEntry = sourceMapUtils.applySourceMapPathOverrides(fullSourceEntry, sourceMapPathOverrides, isVSClient);
                if (fullSourceEntry !== mappedFullSourceEntry) {
                    return utils.canonicalizeUrl(mappedFullSourceEntry);
                }
            }
            if (sourcePath.startsWith('file://')) {
                // strip file://
                return utils.canonicalizeUrl(sourcePath);
            }
            if (!path.isAbsolute(sourcePath)) {
                // Overrides not applied, use the computed sourceRoot
                sourcePath = utils.properResolve(computedSourceRoot, sourcePath);
            }
            return utils.canonicalizeUrl(sourcePath);
        });
        // Rewrite sm.sources to same as this._sources but file url with forward slashes
        sm.sources = this._sources.map(sourceAbsPath => {
            // Convert to file:/// url. After this, it's a file URL for an absolute path to a file on disk with forward slashes.
            // We lowercase so authored <-> generated mapping is not case sensitive.
            const lowerCaseSourceAbsPath = sourceAbsPath.toLowerCase();
            this._authoredPathCaseMap.set(lowerCaseSourceAbsPath, sourceAbsPath);
            return utils.pathToFileURL(lowerCaseSourceAbsPath, true);
        });
        this._smc = new source_map_1.SourceMapConsumer(sm);
    }
    /**
     * Returns list of ISourcePathDetails for all sources in this sourcemap, sorted by their
     * positions within the sourcemap.
     */
    get allSourcePathDetails() {
        if (!this._allSourcePathDetails) {
            // Lazy compute because the source-map lib handles the bulk of the sourcemap parsing lazily, and this info
            // is not always needed.
            this._allSourcePathDetails = this._sources.map((inferredPath, i) => {
                const originalSource = this._originalSources[i];
                const originalPath = this._originalSourceRoot ? sourceMapUtils.getFullSourceEntry(this._originalSourceRoot, originalSource) : originalSource;
                return {
                    inferredPath,
                    originalPath,
                    startPosition: this.generatedPositionFor(inferredPath, 0, 0)
                };
            }).sort((a, b) => {
                // https://github.com/Microsoft/vscode-chrome-debug/issues/353
                if (!a.startPosition) {
                    vscode_debugadapter_1.logger.log(`Could not map start position for: ${a.inferredPath}`);
                    return -1;
                }
                else if (!b.startPosition) {
                    vscode_debugadapter_1.logger.log(`Could not map start position for: ${b.inferredPath}`);
                    return 1;
                }
                if (a.startPosition.line === b.startPosition.line) {
                    return a.startPosition.column - b.startPosition.column;
                }
                else {
                    return a.startPosition.line - b.startPosition.line;
                }
            });
        }
        return this._allSourcePathDetails;
    }
    /*
     * Return all mapped sources as absolute paths
     */
    get authoredSources() {
        return this._sources;
    }
    /*
     * The generated file of this source map.
     */
    generatedPath() {
        return this._generatedPath;
    }
    /*
     * Returns true if this source map originates from the given source.
     */
    doesOriginateFrom(absPath) {
        return this.authoredSources.some(path => path === absPath);
    }
    /*
     * Finds the nearest source location for the given location in the generated file.
     * Will return null instead of a mapping on the next line (different from generatedPositionFor).
     */
    authoredPositionFor(line, column) {
        // source-map lib uses 1-indexed lines.
        line++;
        const lookupArgs = {
            line,
            column,
            bias: source_map_1.SourceMapConsumer.GREATEST_LOWER_BOUND
        };
        let position = this._smc.originalPositionFor(lookupArgs);
        if (!position.source) {
            // If it can't find a match, it returns a mapping with null props. Try looking the other direction.
            lookupArgs.bias = source_map_1.SourceMapConsumer.LEAST_UPPER_BOUND;
            position = this._smc.originalPositionFor(lookupArgs);
        }
        if (position.source) {
            // file:/// -> absolute path
            position.source = utils.canonicalizeUrl(position.source);
            // Convert back to original case
            position.source = this._authoredPathCaseMap.get(position.source) || position.source;
            // Back to 0-indexed lines
            position.line--;
            return position;
        }
        else {
            return null;
        }
    }
    /*
     * Finds the nearest location in the generated file for the given source location.
     * Will return a mapping on the next line, if there is no subsequent mapping on the expected line.
     */
    generatedPositionFor(source, line, column) {
        // source-map lib uses 1-indexed lines.
        line++;
        // sources in the sourcemap have been forced to file:///
        // Convert to lowerCase so search is case insensitive
        source = utils.pathToFileURL(source.toLowerCase(), true);
        const lookupArgs = {
            line,
            column,
            source,
            bias: source_map_1.SourceMapConsumer.LEAST_UPPER_BOUND
        };
        let position = this._smc.generatedPositionFor(lookupArgs);
        if (position.line === null) {
            // If it can't find a match, it returns a mapping with null props. Try looking the other direction.
            lookupArgs.bias = source_map_1.SourceMapConsumer.GREATEST_LOWER_BOUND;
            position = this._smc.generatedPositionFor(lookupArgs);
        }
        if (position.line === null) {
            return null;
        }
        else {
            return {
                line: position.line - 1,
                column: position.column,
                source: this._generatedPath
            };
        }
    }
    sourceContentFor(authoredSourcePath) {
        authoredSourcePath = utils.pathToFileURL(authoredSourcePath, true);
        return this._smc.sourceContentFor(authoredSourcePath, /*returnNullOnMissing=*/ true);
    }
}
exports.SourceMap = SourceMap;

//# sourceMappingURL=sourceMap.js.map
