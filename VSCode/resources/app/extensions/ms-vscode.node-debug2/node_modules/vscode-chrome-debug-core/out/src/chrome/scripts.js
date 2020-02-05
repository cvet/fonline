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
const utils = require("../utils");
const ChromeUtils = require("./chromeUtils");
const chromeDebugAdapter_1 = require("./chromeDebugAdapter");
const path = require("path");
const remoteMapper_1 = require("../remoteMapper");
/**
 * A container class for loaded script files
 */
class ScriptContainer {
    constructor() {
        this._scriptsById = new Map();
        this._scriptsByUrl = new Map();
        this._sourceHandles = new utils.ReverseHandles();
    }
    /**
     * @deprecated use the function calls instead
     */
    get scriptsByIdMap() { return this._scriptsById; }
    /**
     * Get a list of all currently loaded scripts
     */
    get loadedScripts() { return this._scriptsById.values(); }
    /**
     * Get a script by its url
     */
    getScriptByUrl(url) {
        const canonUrl = utils.canonicalizeUrl(url);
        return this._scriptsByUrl.get(canonUrl) || this._scriptsByUrl.get(utils.fixDriveLetter(canonUrl));
    }
    /**
     * Clear this container of all loaded scripts
     */
    reset() {
        this._scriptsById = new Map();
        this._scriptsByUrl = new Map();
    }
    /**
     * Add a newly parsed script to this container
     * @param script The scriptParsed event from the chrome-devtools target
     */
    add(script) {
        this._scriptsById.set(script.scriptId, script);
        this._scriptsByUrl.set(utils.canonicalizeUrl(script.url), script);
    }
    /**
     * Get a script by its chrome-devtools script identifier
     * @param id The script id which came from a chrome-devtools scriptParsed event
     */
    getScriptById(id) {
        return this._scriptsById.get(id);
    }
    /**
     * Get a list of all loaded script urls (as a string)
     */
    getAllScriptsString(pathTransformer, sourceMapTransformer) {
        const runtimeScripts = Array.from(this._scriptsByUrl.keys())
            .sort();
        return Promise.all(runtimeScripts.map(script => this.getOneScriptString(script, pathTransformer, sourceMapTransformer))).then(strs => {
            return strs.join('\n');
        });
    }
    /**
     * Get a script string?
     */
    getOneScriptString(runtimeScriptPath, pathTransformer, sourceMapTransformer) {
        let result = '› ' + runtimeScriptPath;
        const clientPath = pathTransformer.getClientPathFromTargetPath(runtimeScriptPath);
        if (clientPath && clientPath !== runtimeScriptPath)
            result += ` (${clientPath})`;
        return sourceMapTransformer.allSourcePathDetails(clientPath || runtimeScriptPath).then(sourcePathDetails => {
            let mappedSourcesStr = sourcePathDetails.map(details => `    - ${details.originalPath} (${details.inferredPath})`).join('\n');
            if (sourcePathDetails.length)
                mappedSourcesStr = '\n' + mappedSourcesStr;
            return result + mappedSourcesStr;
        });
    }
    /**
     * Get the existing handle for this script, identified by runtime scriptId, or create a new one
     */
    getSourceReferenceForScriptId(scriptId) {
        return this._sourceHandles.lookupF(container => container.scriptId === scriptId) ||
            this._sourceHandles.create({ scriptId });
    }
    /**
     * Get the existing handle for this script, identified by the on-disk path it was mapped to, or create a new one
     */
    getSourceReferenceForScriptPath(mappedPath, contents) {
        return this._sourceHandles.lookupF(container => container.mappedPath === mappedPath) ||
            this._sourceHandles.create({ contents, mappedPath });
    }
    /**
     * Map a chrome script to a DAP source
     * @param script The scriptParsed event object from chrome-devtools target
     * @param origin The origin of the script (node only)
     */
    scriptToSource(script, origin, remoteAuthority) {
        return __awaiter(this, void 0, void 0, function* () {
            const sourceReference = this.getSourceReferenceForScriptId(script.scriptId);
            const properlyCasedScriptUrl = utils.canonicalizeUrl(script.url);
            const displayPath = this.realPathToDisplayPath(properlyCasedScriptUrl);
            const exists = yield utils.existsAsync(properlyCasedScriptUrl); // script.url can start with file:/// so we use the canonicalized version
            const source = {
                name: path.basename(displayPath),
                path: displayPath,
                // if the path exists, do not send the sourceReference
                sourceReference: exists ? undefined : sourceReference,
                origin
            };
            if (remoteAuthority) {
                return remoteMapper_1.mapInternalSourceToRemoteClient(source, remoteAuthority);
            }
            else {
                return source;
            }
        });
    }
    /**
     * Get a source handle by it's reference number
     * @param ref Reference number of a source object
     */
    getSource(ref) {
        return this._sourceHandles.get(ref);
    }
    fakeUrlForSourceReference(sourceReference) {
        const handle = this._sourceHandles.get(sourceReference);
        return `${ChromeUtils.EVAL_NAME_PREFIX}${handle.scriptId}`;
    }
    displayNameForSourceReference(sourceReference) {
        const handle = this._sourceHandles.get(sourceReference);
        return (handle && this.displayNameForScriptId(handle.scriptId)) || sourceReference + '';
    }
    displayNameForScriptId(scriptId) {
        return `${ChromeUtils.EVAL_NAME_PREFIX}${scriptId}`;
    }
    /**
     * Called when returning a stack trace, for the path for Sources that have a sourceReference, so consumers can
     * tweak it, since it's only for display.
     */
    realPathToDisplayPath(realPath) {
        if (ChromeUtils.isEvalScript(realPath)) {
            return `${chromeDebugAdapter_1.ChromeDebugAdapter.EVAL_ROOT}/${realPath}`;
        }
        return realPath;
    }
    /**
     * Get the original path back from a displayPath created from `realPathToDisplayPath`
     */
    displayPathToRealPath(displayPath) {
        if (displayPath.startsWith(chromeDebugAdapter_1.ChromeDebugAdapter.EVAL_ROOT)) {
            return displayPath.substr(chromeDebugAdapter_1.ChromeDebugAdapter.EVAL_ROOT.length + 1); // Trim "<eval>/"
        }
        return displayPath;
    }
}
exports.ScriptContainer = ScriptContainer;

//# sourceMappingURL=scripts.js.map
