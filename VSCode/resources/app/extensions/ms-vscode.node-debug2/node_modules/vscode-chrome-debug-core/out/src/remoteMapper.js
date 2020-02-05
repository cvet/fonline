"use strict";
/*---------------------------------------------------------
* Copyright (C) Microsoft Corporation. All rights reserved.
*--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const vscode_debugadapter_1 = require("vscode-debugadapter");
const vscode_uri_1 = require("vscode-uri");
const path = require("path");
const remoteUriScheme = 'vscode-remote';
const remotePathComponent = '__vscode-remote-uri__';
const isWindows = process.platform === 'win32';
function getFsPath(uri) {
    const fsPath = uri.fsPath;
    return isWindows && !fsPath.match(/^[a-zA-Z]:/) ?
        fsPath.replace(/\\/g, '/') : // Hack - undo the slash normalization that URI does when windows is the current platform
        fsPath;
}
function mapRemoteClientToInternalPath(remoteUri) {
    if (remoteUri.startsWith(remoteUriScheme + ':')) {
        const uri = vscode_uri_1.URI.parse(remoteUri);
        const uriPath = getFsPath(uri);
        const driveLetterMatch = uriPath.match(/^[A-Za-z]:/);
        let internalPath;
        if (!!driveLetterMatch) {
            internalPath = path.win32.join(driveLetterMatch[0], remotePathComponent, uriPath.substr(2));
        }
        else {
            internalPath = path.posix.join('/', remotePathComponent, uriPath);
        }
        vscode_debugadapter_1.logger.log(`remoteMapper: mapping remote uri ${remoteUri} to internal path: ${internalPath}`);
        return internalPath;
    }
    else {
        return remoteUri;
    }
}
exports.mapRemoteClientToInternalPath = mapRemoteClientToInternalPath;
function mapInternalSourceToRemoteClient(source, remoteAuthority) {
    if (source && source.path && isInternalRemotePath(source.path) && remoteAuthority) {
        const remoteUri = vscode_uri_1.URI.file(source.path.replace(new RegExp(remotePathComponent + '[\\/\\\\]'), ''))
            .with({
            scheme: remoteUriScheme,
            authority: remoteAuthority
        });
        return Object.assign({}, source, { path: remoteUri.toString(), origin: undefined, sourceReference: undefined });
    }
    else {
        return source;
    }
}
exports.mapInternalSourceToRemoteClient = mapInternalSourceToRemoteClient;
function isInternalRemotePath(path) {
    return path.startsWith('/' + remotePathComponent) || !!path.match(new RegExp('[a-zA-Z]:[\\/\\\\]' + remotePathComponent));
}
exports.isInternalRemotePath = isInternalRemotePath;

//# sourceMappingURL=remoteMapper.js.map
