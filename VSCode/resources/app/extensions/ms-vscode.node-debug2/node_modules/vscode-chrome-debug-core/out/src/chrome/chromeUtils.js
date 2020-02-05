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
const url = require("url");
const path = require("path");
const vscode_debugadapter_1 = require("vscode-debugadapter");
const utils = require("../utils");
const utils_1 = require("../utils");
const remoteMapper_1 = require("../remoteMapper");
const net_1 = require("net");
const errors = require("../errors");
/**
 * Takes the path component of a target url (starting with '/') and applies pathMapping
 */
function applyPathMappingsToTargetUrlPath(scriptUrlPath, pathMapping) {
    if (!pathMapping) {
        return '';
    }
    if (!scriptUrlPath || !scriptUrlPath.startsWith('/')) {
        return '';
    }
    const mappingKeys = Object.keys(pathMapping)
        .sort((a, b) => b.length - a.length);
    for (let pattern of mappingKeys) {
        // empty pattern match nothing use / to match root
        if (!pattern) {
            continue;
        }
        const mappingRHS = pathMapping[pattern];
        if (pattern[0] !== '/') {
            vscode_debugadapter_1.logger.log(`PathMapping keys should be absolute: ${pattern}`);
            pattern = '/' + pattern;
        }
        if (pathMappingPatternMatchesPath(pattern, scriptUrlPath)) {
            return toClientPath(pattern, mappingRHS, scriptUrlPath);
        }
    }
    return '';
}
exports.applyPathMappingsToTargetUrlPath = applyPathMappingsToTargetUrlPath;
function pathMappingPatternMatchesPath(pattern, scriptPath) {
    if (pattern === scriptPath) {
        return true;
    }
    if (!pattern.endsWith('/')) {
        // Don't match /foo with /foobar/something
        pattern += '/';
    }
    return scriptPath.startsWith(pattern);
}
function applyPathMappingsToTargetUrl(scriptUrl, pathMapping) {
    const parsedUrl = url.parse(scriptUrl);
    if (!parsedUrl.protocol || parsedUrl.protocol.startsWith('file') || !parsedUrl.pathname) {
        // Skip file: URLs and paths, and invalid things
        return '';
    }
    return applyPathMappingsToTargetUrlPath(parsedUrl.pathname, pathMapping);
}
exports.applyPathMappingsToTargetUrl = applyPathMappingsToTargetUrl;
function toClientPath(pattern, mappingRHS, scriptPath) {
    const rest = decodeURIComponent(scriptPath.substring(pattern.length));
    const mappedResult = rest ?
        utils.properJoin(mappingRHS, rest) :
        mappingRHS;
    return mappedResult;
}
/**
 * Maps a url from target to an absolute local path, if it exists.
 * If not given an absolute path (with file: prefix), searches the current working directory for a matching file.
 * http://localhost/scripts/code.js => d:/app/scripts/code.js
 * file:///d:/scripts/code.js => d:/scripts/code.js
 */
function targetUrlToClientPath(aUrl, pathMapping) {
    return __awaiter(this, void 0, void 0, function* () {
        if (!aUrl) {
            return '';
        }
        // If the url is an absolute path to a file that exists, return it without file:///.
        // A remote absolute url (cordova) will still need the logic below.
        const canonicalUrl = utils.canonicalizeUrl(aUrl);
        if (utils.isFileUrl(aUrl)) {
            if (yield utils.exists(canonicalUrl)) {
                return canonicalUrl;
            }
            const networkPath = utils.fileUrlToNetworkPath(aUrl);
            if (networkPath !== aUrl && (yield utils.exists(networkPath))) {
                return networkPath;
            }
        }
        // Search the filesystem under the webRoot for the file that best matches the given url
        let pathName = url.parse(canonicalUrl).pathname;
        if (!pathName || pathName === '/') {
            return '';
        }
        // Dealing with the path portion of either a url or an absolute path to remote file.
        const pathParts = pathName
            .replace(/^\//, '') // Strip leading /
            .split(/[\/\\]/);
        while (pathParts.length > 0) {
            const joinedPath = '/' + pathParts.join('/');
            const clientPath = applyPathMappingsToTargetUrlPath(joinedPath, pathMapping);
            if (remoteMapper_1.isInternalRemotePath(clientPath)) {
                return clientPath;
            }
            else if (clientPath && (yield utils.exists(clientPath))) {
                return utils.canonicalizeUrl(clientPath);
            }
            pathParts.shift();
        }
        return '';
    });
}
exports.targetUrlToClientPath = targetUrlToClientPath;
/**
 * Convert a RemoteObject to a value+variableHandleRef for the client.
 * TODO - Delete after Microsoft/vscode#12019!!
 */
function remoteObjectToValue(object, stringify = true) {
    let value = '';
    let variableHandleRef;
    if (object) {
        if (object.type === 'object') {
            if (object.subtype === 'null') {
                value = 'null';
            }
            else {
                // If it's a non-null object, create a variable reference so the client can ask for its props
                variableHandleRef = object.objectId;
                value = object.description;
            }
        }
        else if (object.type === 'undefined') {
            value = 'undefined';
        }
        else if (object.type === 'function') {
            const firstBraceIdx = object.description.indexOf('{');
            if (firstBraceIdx >= 0) {
                value = object.description.substring(0, firstBraceIdx) + '{ … }';
            }
            else {
                const firstArrowIdx = object.description.indexOf('=>');
                value = firstArrowIdx >= 0 ?
                    object.description.substring(0, firstArrowIdx + 2) + ' …' :
                    object.description;
            }
        }
        else {
            // The value is a primitive value, or something that has a description (not object, primitive, or undefined). And force to be string
            if (typeof object.value === 'undefined') {
                value = object.description;
            }
            else if (object.type === 'number') {
                // .value is truncated, so use .description, the full string representation
                // Should be like '3' or 'Infinity'.
                value = object.description;
            }
            else {
                value = stringify ? JSON.stringify(object.value) : object.value;
            }
        }
    }
    return { value, variableHandleRef };
}
exports.remoteObjectToValue = remoteObjectToValue;
/**
 * Returns the targets from the given list that match the targetUrl, which may have * wildcards.
 * Ignores the protocol and is case-insensitive.
 */
function getMatchingTargets(targets, targetUrlPattern) {
    const standardizeMatch = (aUrl) => {
        aUrl = aUrl.toLowerCase();
        if (utils.isFileUrl(aUrl)) {
            // Strip file:///, if present
            aUrl = utils.fileUrlToPath(aUrl);
        }
        else if (utils.isURL(aUrl) && aUrl.indexOf('://') >= 0) {
            // Strip the protocol, if present
            aUrl = aUrl.substr(aUrl.indexOf('://') + 3);
        }
        // Remove optional trailing /
        if (aUrl.endsWith('/'))
            aUrl = aUrl.substr(0, aUrl.length - 1);
        return aUrl;
    };
    targetUrlPattern = standardizeMatch(targetUrlPattern);
    targetUrlPattern = utils.escapeRegexSpecialChars(targetUrlPattern, '/*').replace(/\*/g, '.*');
    const targetUrlRegex = new RegExp('^' + targetUrlPattern + '$', 'g');
    return targets.filter(target => !!standardizeMatch(target.url).match(targetUrlRegex));
}
exports.getMatchingTargets = getMatchingTargets;
const PROTO_NAME = '__proto__';
const NUM_REGEX = /^[0-9]+$/;
function compareVariableNames(var1, var2) {
    // __proto__ at the end
    if (var1 === PROTO_NAME) {
        return 1;
    }
    else if (var2 === PROTO_NAME) {
        return -1;
    }
    const isNum1 = !!var1.match(NUM_REGEX);
    const isNum2 = !!var2.match(NUM_REGEX);
    if (isNum1 && !isNum2) {
        // Numbers after names
        return 1;
    }
    else if (!isNum1 && isNum2) {
        // Names before numbers
        return -1;
    }
    else if (isNum1 && isNum2) {
        // Compare numbers as numbers
        const int1 = parseInt(var1, 10);
        const int2 = parseInt(var2, 10);
        return int1 - int2;
    }
    // Compare strings as strings
    return var1.localeCompare(var2);
}
exports.compareVariableNames = compareVariableNames;
function remoteObjectToCallArgument(object) {
    return {
        objectId: object.objectId,
        unserializableValue: object.unserializableValue,
        value: object.value
    };
}
exports.remoteObjectToCallArgument = remoteObjectToCallArgument;
/**
 * .exception is not present in Node < 6.6 - TODO this would be part of a generic solution for handling
 * protocol differences in the future.
 * This includes the error message and full stack
 */
function descriptionFromExceptionDetails(exceptionDetails) {
    let description;
    if (exceptionDetails.exception) {
        // Take exception object description, or if a value was thrown, the value
        description = exceptionDetails.exception.description ||
            'Error: ' + exceptionDetails.exception.value;
    }
    else {
        description = exceptionDetails.text;
    }
    return description || '';
}
exports.descriptionFromExceptionDetails = descriptionFromExceptionDetails;
/**
 * Get just the error message from the exception details - the first line without the full stack
 */
function errorMessageFromExceptionDetails(exceptionDetails) {
    let description = descriptionFromExceptionDetails(exceptionDetails);
    const newlineIdx = description.indexOf('\n');
    if (newlineIdx >= 0) {
        description = description.substr(0, newlineIdx);
    }
    return description;
}
exports.errorMessageFromExceptionDetails = errorMessageFromExceptionDetails;
function getEvaluateName(parentEvaluateName, name) {
    if (!parentEvaluateName)
        return name;
    let nameAccessor;
    if (/^[a-zA-Z_$][a-zA-Z_$0-9]*$/.test(name)) {
        nameAccessor = '.' + name;
    }
    else if (/^\d+$/.test(name)) {
        nameAccessor = `[${name}]`;
    }
    else {
        nameAccessor = `[${JSON.stringify(name)}]`;
    }
    return parentEvaluateName + nameAccessor;
}
exports.getEvaluateName = getEvaluateName;
function selectBreakpointLocation(lineNumber, columnNumber, locations) {
    for (let i = locations.length - 1; i >= 0; i--) {
        if (locations[i].columnNumber <= columnNumber) {
            return locations[i];
        }
    }
    return locations[0];
}
exports.selectBreakpointLocation = selectBreakpointLocation;
exports.EVAL_NAME_PREFIX = 'VM';
function isEvalScript(scriptPath) {
    return scriptPath.startsWith(exports.EVAL_NAME_PREFIX);
}
exports.isEvalScript = isEvalScript;
/* Constructs the regex for files to enable break on load
For example, for a file index.js the regex will match urls containing index.js, index.ts, abc/index.ts, index.bin.js etc
It won't match index100.js, indexabc.ts etc */
function getUrlRegexForBreakOnLoad(url) {
    const fileNameWithoutFullPath = path.parse(url).base;
    const fileNameWithoutExtension = path.parse(fileNameWithoutFullPath).name;
    const escapedFileName = utils_1.pathToRegex(fileNameWithoutExtension);
    return '.*[\\\\\\/]' + escapedFileName + '([^A-z^0-9].*)?$';
}
exports.getUrlRegexForBreakOnLoad = getUrlRegexForBreakOnLoad;
/**
 * Checks if a given tcp port is currently in use (more accurately, is there a server socket accepting connections on that port)
 * @param port The port to check
 * @param host Optional host, defaults to 127.0.0.1
 * @param timeout Timeout for the socket connect attempt
 * @returns True if a server socket is listening on the given port, false otherwise
 */
function isPortInUse(port, host = '127.0.0.1', timeout = 400) {
    return __awaiter(this, void 0, void 0, function* () {
        // Basically just create a socket and try to connect on that port, if we can connect, it's open
        return new Promise((resolve, _reject) => {
            const socket = new net_1.Socket();
            function createCallback(inUse) {
                return () => {
                    resolve(inUse);
                    socket.removeAllListeners();
                    socket.destroy();
                };
            }
            socket.setTimeout(timeout);
            socket.on('connect', createCallback(true));
            socket.on('timeout', createCallback(false));
            socket.on('error', createCallback(false));
            socket.connect(port, host);
        });
    });
}
exports.isPortInUse = isPortInUse;
/**
 * Get the port on which chrome was launched when passed "--remote-debugging-port=0"
 * @param userDataDir The profile data directory for the Chrome instance to check
 * @throws If reading the port failed for any reason
 */
function getLaunchedPort(userDataDir) {
    return __awaiter(this, void 0, void 0, function* () {
        const activePortFilePath = path.join(userDataDir, 'DevToolsActivePort');
        try {
            const activePortArgs = yield utils.readFileP(activePortFilePath, 'utf-8');
            const [portStr] = activePortArgs.split('\n'); // chrome uses \n regardless of platform in this file
            const port = parseInt(portStr, 10);
            if (isNaN(port))
                return Promise.reject(errors.activePortFileContentsInvalid(activePortFilePath, activePortArgs));
            return port;
        }
        catch (err) {
            return Promise.reject(errors.failedToReadPortFromUserDataDir(userDataDir, err));
        }
    });
}
exports.getLaunchedPort = getLaunchedPort;

//# sourceMappingURL=chromeUtils.js.map
