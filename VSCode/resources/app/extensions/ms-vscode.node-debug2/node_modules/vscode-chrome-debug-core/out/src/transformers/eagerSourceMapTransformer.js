"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const path = require("path");
const baseSourceMapTransformer_1 = require("./baseSourceMapTransformer");
const utils = require("../utils");
const vscode_debugadapter_1 = require("vscode-debugadapter");
/**
 * Load SourceMaps on launch. Requires reading the file and parsing out the sourceMappingURL, because
 * if you wait until the script is loaded as in LazySMT, you get that info from the runtime.
 */
class EagerSourceMapTransformer extends baseSourceMapTransformer_1.BaseSourceMapTransformer {
    init(args) {
        super.init(args);
        if (args.sourceMaps) {
            const generatedCodeGlobs = args.outFiles ?
                args.outFiles :
                args.outDir ?
                    [path.join(args.outDir, '**/*.js')] :
                    [];
            // try to find all source files upfront asynchronously
            if (generatedCodeGlobs.length > 0) {
                vscode_debugadapter_1.logger.log('SourceMaps: preloading sourcemaps for scripts in globs: ' + JSON.stringify(generatedCodeGlobs));
                this._preLoad = utils.multiGlob(generatedCodeGlobs)
                    .then(paths => {
                    vscode_debugadapter_1.logger.log(`SourceMaps: expanded globs and found ${paths.length} scripts`);
                    return Promise.all(paths.map(scriptPath => this.discoverSourceMapForGeneratedScript(scriptPath)));
                })
                    .then(() => { });
            }
            else {
                this._preLoad = Promise.resolve();
            }
        }
    }
    discoverSourceMapForGeneratedScript(generatedScriptPath) {
        return this.findSourceMapUrlInFile(generatedScriptPath)
            .then(uri => {
            if (uri) {
                vscode_debugadapter_1.logger.log(`SourceMaps: sourcemap url parsed from end of generated content: ${uri}`);
                return this._sourceMaps.processNewSourceMap(generatedScriptPath, undefined, uri, this._isVSClient);
            }
            else {
                vscode_debugadapter_1.logger.log(`SourceMaps: no sourcemap url found in generated script: ${generatedScriptPath}`);
                return undefined;
            }
        })
            .catch(err => {
            // If we fail to preload one, ignore and keep going
            vscode_debugadapter_1.logger.log(`SourceMaps: could not preload for generated script: ${generatedScriptPath}. Error: ${err.toString()}`);
        });
    }
    /**
     * Try to find the 'sourceMappingURL' in content or the file with the given path.
     * Returns null if no source map url is found or if an error occured.
     */
    findSourceMapUrlInFile(pathToGenerated, content) {
        if (content) {
            return Promise.resolve(this.findSourceMapUrl(content));
        }
        return utils.readFileP(pathToGenerated)
            .then(fileContents => this.findSourceMapUrl(fileContents));
    }
    /**
     * Try to find the 'sourceMappingURL' at the end of the given contents.
     * Relative file paths are converted into absolute paths.
     * Returns null if no source map url is found.
     */
    findSourceMapUrl(contents) {
        const lines = contents.split('\n');
        for (let l = lines.length - 1; l >= Math.max(lines.length - 10, 0); l--) {
            const line = lines[l].trim();
            const matches = EagerSourceMapTransformer.SOURCE_MAPPING_MATCHER.exec(line);
            if (matches && matches.length === 2) {
                return matches[1].trim();
            }
        }
        return null;
    }
}
EagerSourceMapTransformer.SOURCE_MAPPING_MATCHER = new RegExp('^//[#@] ?sourceMappingURL=(.+)$');
exports.EagerSourceMapTransformer = EagerSourceMapTransformer;

//# sourceMappingURL=eagerSourceMapTransformer.js.map
