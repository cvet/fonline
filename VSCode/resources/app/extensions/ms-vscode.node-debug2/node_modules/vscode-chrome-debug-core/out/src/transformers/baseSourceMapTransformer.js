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
const path = require("path");
const sourceMaps_1 = require("../sourceMaps/sourceMaps");
const utils = require("../utils");
const vscode_debugadapter_1 = require("vscode-debugadapter");
const nls = require("vscode-nls");
const localize = nls.loadMessageBundle(__filename);
/**
 * If sourcemaps are enabled, converts from source files on the client side to runtime files on the target side
 */
class BaseSourceMapTransformer {
    constructor(sourceHandles) {
        this._preLoad = Promise.resolve();
        this._processingNewSourceMap = Promise.resolve();
        this._isVSClient = false;
        this._scriptContainer = sourceHandles;
    }
    get sourceMaps() {
        return this._sourceMaps;
    }
    set isVSClient(newValue) {
        this._isVSClient = newValue;
    }
    launch(args) {
        this.init(args);
    }
    attach(args) {
        this.init(args);
    }
    init(args) {
        if (args.sourceMaps) {
            this._enableSourceMapCaching = args.enableSourceMapCaching;
            this._sourceMaps = new sourceMaps_1.SourceMaps(args.pathMapping, args.sourceMapPathOverrides, this._enableSourceMapCaching);
            this._requestSeqToSetBreakpointsArgs = new Map();
            this._allRuntimeScriptPaths = new Set();
            this._authoredPathsToMappedBPs = new Map();
            this._authoredPathsToClientBreakpointIds = new Map();
        }
    }
    clearTargetContext() {
        this._allRuntimeScriptPaths = new Set();
    }
    /**
     * Apply sourcemapping to the setBreakpoints request path/lines.
     * Returns true if completed successfully, and setBreakpoint should continue.
     */
    setBreakpoints(args, requestSeq, ids) {
        if (!this._sourceMaps) {
            return { args, ids };
        }
        const originalBPs = JSON.parse(JSON.stringify(args.breakpoints));
        if (args.source.sourceReference) {
            // If the source contents were inlined, then args.source has no path, but we
            // stored it in the handle
            const handle = this._scriptContainer.getSource(args.source.sourceReference);
            if (handle && handle.mappedPath) {
                args.source.path = handle.mappedPath;
            }
        }
        if (args.source.path) {
            const argsPath = args.source.path;
            const mappedPath = this._sourceMaps.getGeneratedPathFromAuthoredPath(argsPath);
            if (mappedPath) {
                vscode_debugadapter_1.logger.log(`SourceMaps.setBP: Mapped ${argsPath} to ${mappedPath}`);
                args.authoredPath = argsPath;
                args.source.path = mappedPath;
                // DebugProtocol doesn't send cols yet, but they need to be added from sourcemaps
                args.breakpoints.forEach(bp => {
                    const { line, column = 0 } = bp;
                    const mapped = this._sourceMaps.mapToGenerated(argsPath, line, column);
                    if (mapped) {
                        vscode_debugadapter_1.logger.log(`SourceMaps.setBP: Mapped ${argsPath}:${line + 1}:${column + 1} to ${mappedPath}:${mapped.line + 1}:${mapped.column + 1}`);
                        bp.line = mapped.line;
                        bp.column = mapped.column;
                    }
                    else {
                        vscode_debugadapter_1.logger.log(`SourceMaps.setBP: Mapped ${argsPath} but not line ${line + 1}, column 1`);
                        bp.column = column; // take 0 default if needed
                    }
                });
                this._authoredPathsToMappedBPs.set(argsPath, args.breakpoints);
                // Store the client breakpoint Ids for the mapped BPs as well
                if (ids) {
                    this._authoredPathsToClientBreakpointIds.set(argsPath, ids);
                }
                // Include BPs from other files that map to the same file. Ensure the current file's breakpoints go first
                this._sourceMaps.allMappedSources(mappedPath).forEach(sourcePath => {
                    if (sourcePath === argsPath) {
                        return;
                    }
                    const sourceBPs = this._authoredPathsToMappedBPs.get(sourcePath);
                    if (sourceBPs) {
                        // Don't modify the cached array
                        args.breakpoints = args.breakpoints.concat(sourceBPs);
                        // We need to assign the client IDs we generated for the mapped breakpoints becuase the runtime IDs may change
                        // So make sure we concat the client ids to the ids array so that they get mapped to the respective breakpoints later
                        const clientBreakpointIds = this._authoredPathsToClientBreakpointIds.get(sourcePath);
                        if (ids) {
                            ids = ids.concat(clientBreakpointIds);
                        }
                    }
                });
            }
            else if (this.isRuntimeScript(argsPath)) {
                // It's a generated file which is loaded
                vscode_debugadapter_1.logger.log(`SourceMaps.setBP: SourceMaps are enabled but ${argsPath} is a runtime script`);
            }
            else {
                // Source (or generated) file which is not loaded.
                vscode_debugadapter_1.logger.log(`SourceMaps.setBP: ${argsPath} can't be resolved to a loaded script. It may just not be loaded yet.`);
            }
        }
        else {
            // No source.path
        }
        this._requestSeqToSetBreakpointsArgs.set(requestSeq, {
            originalBPs,
            authoredPath: args.authoredPath,
            generatedPath: args.source.path
        });
        return { args, ids };
    }
    /**
     * Apply sourcemapping back to authored files from the response
     */
    setBreakpointsResponse(breakpoints, shouldFilter, requestSeq) {
        if (this._sourceMaps && this._requestSeqToSetBreakpointsArgs.has(requestSeq)) {
            const args = this._requestSeqToSetBreakpointsArgs.get(requestSeq);
            if (args.authoredPath) {
                // authoredPath is set, so the file was mapped to source.
                // Remove breakpoints from files that map to the same file, and map back to source.
                if (shouldFilter) {
                    breakpoints = breakpoints.filter((_, i) => i < args.originalBPs.length);
                }
                breakpoints.forEach((bp, i) => {
                    const mapped = this._sourceMaps.mapToAuthored(args.generatedPath, bp.line, bp.column);
                    if (mapped) {
                        vscode_debugadapter_1.logger.log(`SourceMaps.setBP: Mapped ${args.generatedPath}:${bp.line + 1}:${bp.column + 1} to ${mapped.source}:${mapped.line + 1}`);
                        bp.line = mapped.line;
                        bp.column = mapped.column;
                    }
                    else {
                        vscode_debugadapter_1.logger.log(`SourceMaps.setBP: Can't map ${args.generatedPath}:${bp.line + 1}:${bp.column + 1}, keeping original line numbers.`);
                        if (args.originalBPs[i]) {
                            bp.line = args.originalBPs[i].line;
                            bp.column = args.originalBPs[i].column;
                        }
                    }
                    this._requestSeqToSetBreakpointsArgs.delete(requestSeq);
                });
            }
        }
        return breakpoints;
    }
    /**
     * Apply sourcemapping to the stacktrace response
     */
    stackTraceResponse(response) {
        return __awaiter(this, void 0, void 0, function* () {
            if (this._sourceMaps) {
                yield this._processingNewSourceMap;
                for (let stackFrame of response.stackFrames) {
                    yield this.fixSourceLocation(stackFrame);
                }
            }
        });
    }
    fixSourceLocation(sourceLocation) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!this._sourceMaps) {
                return;
            }
            if (!sourceLocation.source) {
                return;
            }
            yield this._processingNewSourceMap;
            const mapped = this._sourceMaps.mapToAuthored(sourceLocation.source.path, sourceLocation.line, sourceLocation.column);
            if (mapped && utils.existsSync(mapped.source)) {
                // Script was mapped to a valid path
                sourceLocation.source.path = mapped.source;
                sourceLocation.source.sourceReference = undefined;
                sourceLocation.source.name = path.basename(mapped.source);
                sourceLocation.line = mapped.line;
                sourceLocation.column = mapped.column;
                sourceLocation.isSourceMapped = true;
            }
            else {
                const inlinedSource = mapped && this._sourceMaps.sourceContentFor(mapped.source);
                if (mapped && inlinedSource) {
                    // Clear the path and set the sourceReference - the client will ask for
                    // the source later and it will be returned from the sourcemap
                    sourceLocation.source.name = path.basename(mapped.source);
                    sourceLocation.source.path = mapped.source;
                    sourceLocation.source.sourceReference = this._scriptContainer.getSourceReferenceForScriptPath(mapped.source, inlinedSource);
                    sourceLocation.source.origin = localize(0, null);
                    sourceLocation.line = mapped.line;
                    sourceLocation.column = mapped.column;
                    sourceLocation.isSourceMapped = true;
                }
                else if (utils.existsSync(sourceLocation.source.path)) {
                    // Script could not be mapped, but does exist on disk. Keep it and clear the sourceReference.
                    sourceLocation.source.sourceReference = undefined;
                    sourceLocation.source.origin = undefined;
                }
            }
        });
    }
    scriptParsed(pathToGenerated, originalUrlToGenerated, sourceMapURL) {
        return __awaiter(this, void 0, void 0, function* () {
            if (this._sourceMaps) {
                this._allRuntimeScriptPaths.add(this.fixPathCasing(pathToGenerated));
                if (!sourceMapURL)
                    return null;
                // Load the sourcemap for this new script and log its sources
                const processNewSourceMapP = this._sourceMaps.processNewSourceMap(pathToGenerated, originalUrlToGenerated, sourceMapURL, this._isVSClient);
                this._processingNewSourceMap = Promise.all([this._processingNewSourceMap, processNewSourceMapP]);
                yield processNewSourceMapP;
                const sources = this._sourceMaps.allMappedSources(pathToGenerated);
                if (sources) {
                    vscode_debugadapter_1.logger.log(`SourceMaps.scriptParsed: ${pathToGenerated} was just loaded and has mapped sources: ${JSON.stringify(sources)}`);
                }
                return sources;
            }
            else {
                return null;
            }
        });
    }
    breakpointResolved(bp, scriptPath) {
        if (this._sourceMaps) {
            const mapped = this._sourceMaps.mapToAuthored(scriptPath, bp.line, bp.column);
            if (mapped) {
                // No need to send back the path, the bp can only move within its script
                bp.line = mapped.line;
                bp.column = mapped.column;
            }
        }
    }
    scopesResponse(pathToGenerated, scopesResponse) {
        if (this._sourceMaps) {
            scopesResponse.scopes.forEach(scope => this.mapScopeLocations(pathToGenerated, scope));
        }
    }
    mapScopeLocations(pathToGenerated, scope) {
        // The runtime can return invalid scope locations. Just skip those scopes. https://github.com/Microsoft/vscode-chrome-debug-core/issues/333
        if (typeof scope.line !== 'number' || scope.line < 0 || scope.endLine < 0 || scope.column < 0 || scope.endColumn < 0) {
            return;
        }
        let mappedStart = this._sourceMaps.mapToAuthored(pathToGenerated, scope.line, scope.column);
        let shiftedScopeStartForward = false;
        // If the scope is an async function, then the function declaration line may be missing a source mapping.
        // So if we failed, try to get the next line.
        if (!mappedStart) {
            mappedStart = this._sourceMaps.mapToAuthored(pathToGenerated, scope.line + 1, scope.column);
            shiftedScopeStartForward = true;
        }
        if (mappedStart) {
            // Only apply changes if both mappings are found
            const mappedEnd = this._sourceMaps.mapToAuthored(pathToGenerated, scope.endLine, scope.endColumn);
            if (mappedEnd) {
                scope.line = mappedStart.line;
                if (shiftedScopeStartForward) {
                    scope.line--;
                }
                scope.column = mappedStart.column;
                scope.endLine = mappedEnd.line;
                scope.endColumn = mappedEnd.column;
            }
        }
    }
    mapToGenerated(authoredPath, line, column) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!this._sourceMaps)
                return null;
            yield this.wait();
            return this._sourceMaps.mapToGenerated(authoredPath, line, column);
        });
    }
    mapToAuthored(pathToGenerated, line, column) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!this._sourceMaps)
                return null;
            yield this.wait();
            return this._sourceMaps.mapToAuthored(pathToGenerated, line, column);
        });
    }
    getGeneratedPathFromAuthoredPath(authoredPath) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!this._sourceMaps)
                return authoredPath;
            yield this.wait();
            // Find the generated path, or check whether this script is actually a runtime path - if so, return that
            return this._sourceMaps.getGeneratedPathFromAuthoredPath(authoredPath) ||
                (this.isRuntimeScript(authoredPath) ? authoredPath : null);
        });
    }
    allSources(pathToGenerated) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!this._sourceMaps)
                return [];
            yield this.wait();
            return this._sourceMaps.allMappedSources(pathToGenerated) || [];
        });
    }
    allSourcePathDetails(pathToGenerated) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!this._sourceMaps)
                return [];
            yield this.wait();
            return this._sourceMaps.allSourcePathDetails(pathToGenerated) || [];
        });
    }
    wait() {
        return Promise.all([this._preLoad, this._processingNewSourceMap]);
    }
    isRuntimeScript(scriptPath) {
        return this._allRuntimeScriptPaths.has(this.fixPathCasing(scriptPath));
    }
    fixPathCasing(str) {
        return str && (this.caseSensitivePaths ? str : str.toLowerCase());
    }
}
exports.BaseSourceMapTransformer = BaseSourceMapTransformer;

//# sourceMappingURL=baseSourceMapTransformer.js.map
