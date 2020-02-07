"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
var __metadata = (this && this.__metadata) || function (k, v) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(k, v);
};
var __param = (this && this.__param) || function (paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
};
Object.defineProperty(exports, "__esModule", { value: true });
var FileStatParser_1;
"use strict";
const inversify_1 = require("inversify");
const path = require("path");
const vscode_1 = require("vscode");
const types_1 = require("../../../ioc/types");
const types_2 = require("../../../types");
const types_3 = require("../types");
let FileStatParser = FileStatParser_1 = class FileStatParser {
    constructor(serviceContainer) {
        this.serviceContainer = serviceContainer;
    }
    static parseFileMovement(fileInfo) {
        // src/client/{common/comms => }/Socket Stream.ts
        // src/client/common/{space in folder => comms}/id Dispenser.ts
        // src/client/common/space in folder/{idDispenser.ts => id Dispenser.ts}
        // src/client/common/{comms => space in folder}/SocketStream.ts
        // src/client/common/{comms => }/socketCallbackHandler.ts
        // src/client/common/comms/{ => another dir}/id Dispenser.ts
        // src/{test/autocomplete => client/common/comms/another dir}/base.test.ts
        // src/{client/common/comms/another dir => }/id Dispenser.ts
        // src/test/jupyter/{extension.jupyter.comms.jupyterKernelManager.test.ts => jupyterKernelManager.test.ts}
        // Another exampe is as follows, where a tab is used as a separator
        // src/vs/workbench/services/extensions/node/ipcRemoteCom.ts       src/vs/workbench/services/extensions/node/rpcProtocol.ts
        const diffSeparator = [' => ', '\t'].reduce((separator, item) => {
            if (typeof separator === 'string') {
                return separator;
            }
            return fileInfo.indexOf(item) === -1 ? undefined : item;
        }, undefined);
        if (!diffSeparator) {
            console.error(`Parsing file movements failed for ${fileInfo}`);
            return;
        }
        const startIndex = fileInfo.indexOf('{');
        const endIndex = fileInfo.lastIndexOf('}');
        if (startIndex === -1 || endIndex === -1) {
            // Change in file name within root directory (that's why we don't have paths)
            const parts = fileInfo.split(diffSeparator);
            return { original: parts[0], current: parts[1] };
        }
        else {
            const partWithDifference = fileInfo.substring(startIndex, endIndex + 1);
            if (!partWithDifference.startsWith('{') || !partWithDifference.endsWith('}')) {
                console.error(`Invalid entry cotaining => for ${fileInfo}`);
                return;
            }
            const parts = partWithDifference.split(diffSeparator)
                .map(part => part.startsWith('{') ? part.substring(1) : part)
                .map(part => part.endsWith('}') ? part.substring(0, part.length - 1) : part)
                .map(part => part.trim());
            if (parts.length !== 2) {
                console.error(`Invalid number of items after splitting parts of file movements ${fileInfo}`);
            }
            // First build the original path
            const original = fileInfo.replace(partWithDifference, parts[0]);
            const originalPathParts = original.split(/\/|\/\/|\\|\\\\/g).filter(part => part.length > 0);
            const current = fileInfo.replace(partWithDifference, parts[1]);
            const currentPathParts = current.split(/\/|\/\/|\\|\\\\/g).filter(part => part.length > 0);
            return { original: path.join(...originalPathParts), current: path.join(...currentPathParts) };
        }
    }
    /**
     * Parses a line containing file information returned by `git log --name-stat` and returns just the file names
     * @param {string} line
     * @returns {({ original?: string, current: string } | undefined)}
     * @memberof FileStatParser
     */
    static getNewAndOldFileNameFromNumStatLine(line, status) {
        const statusParts = line.split('\t');
        const fileName = statusParts[1].trim();
        if (status === types_2.Status.Renamed || status === types_2.Status.Copied) {
            return FileStatParser_1.parseFileMovement(line.substring(line.indexOf(fileName)));
        }
        return { current: fileName };
    }
    /**
     * Parses a line containing file information returned by `git log --numstat`
     * @param {string} line
     * @returns {({ additions?: number, deletions?: number } | undefined)}
     * @memberof FileStatParser
     */
    static getAdditionsAndDeletionsFromNumStatLine(line) {
        // 0       0       src/client/common/{comms => }/socketCallbackHandler.ts
        const numStatParts = line.split('\t').map(part => part.trim()).filter(part => part.length > 0);
        if (numStatParts.length < 3) {
            console.error(`Failed to identify additions and deletions for line ${line}`);
            return;
        }
        let additions = numStatParts[0] === '-' ? undefined : parseInt(numStatParts[0], 10);
        additions = isNaN(additions) ? undefined : additions;
        let deletions = numStatParts[1] === '-' ? undefined : parseInt(numStatParts[1], 10);
        deletions = isNaN(deletions) ? undefined : deletions;
        return { additions, deletions, fileName: numStatParts[2] };
    }
    /**
     * Parsers the file status
     * @param {string[]} filesWithNumStat Files returned using `git log --numstat`
     * @param {string[]} filesWithNameStat Files returned using `git log --name-status`
     * @returns {CommittedFile[]} An array of committed files
     */
    parse(gitRootPath, filesWithNumStat, filesWithNameStat) {
        return filesWithNameStat.map((line, index) => {
            if (line.trim().length === 0 && filesWithNumStat.length > index && filesWithNumStat[index].trim().length === 0) {
                return;
            }
            const numStatParts = FileStatParser_1.getAdditionsAndDeletionsFromNumStatLine(filesWithNumStat[index]);
            const additions = numStatParts ? numStatParts.additions : undefined;
            const deletions = numStatParts ? numStatParts.deletions : undefined;
            const statusParts = line.split('\t');
            const statusCode = statusParts[0].trim();
            const statusParser = this.serviceContainer.get(types_3.IFileStatStatusParser);
            if (!statusParser.canParse(statusCode)) {
                return;
            }
            const status = statusParser.parse(statusCode);
            const currentAndOriginalFile = FileStatParser_1.getNewAndOldFileNameFromNumStatLine(line, status);
            const oldRelativePath = currentAndOriginalFile ? currentAndOriginalFile.original : undefined;
            const relativePath = currentAndOriginalFile.current;
            const oldUri = oldRelativePath ? vscode_1.Uri.file(path.join(gitRootPath, oldRelativePath)) : undefined;
            // tslint:disable-next-line:no-unnecessary-local-variable
            const fileInfo = {
                additions,
                deletions,
                status,
                relativePath,
                oldRelativePath,
                uri: vscode_1.Uri.file(path.join(gitRootPath, relativePath)),
                oldUri
            };
            // uri.fsPath getter sporadically becomes a slash as prefix (E.g  "/z:/folder/subfolder").
            // By fetching fsPath through the getter, the internal method _makeFsPath(this) immediate get called here
            // and the fsPath is set correctly.
            //
            // PLEASE NOTE: While DEBUGGING the property is  always resolved correctly
            fileInfo.uri.fsPath;
            if (fileInfo.oldUri !== undefined) {
                fileInfo.oldUri.fsPath;
            }
            return fileInfo;
        })
            .filter(commitFile => commitFile !== undefined)
            .map(commitFile => commitFile);
    }
};
FileStatParser = FileStatParser_1 = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(types_1.IServiceContainer)),
    __metadata("design:paramtypes", [Object])
], FileStatParser);
exports.FileStatParser = FileStatParser;
//# sourceMappingURL=parser.js.map