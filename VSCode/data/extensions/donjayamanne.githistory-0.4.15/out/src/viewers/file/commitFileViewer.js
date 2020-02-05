'use strict';
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const querystring = require("query-string");
const vscode_1 = require("vscode");
const types_1 = require("../../types");
class CommitFileViewerProvider {
    constructor(svcContainer) {
        this.svcContainer = svcContainer;
    }
    provideTextDocumentContent(uri, _token) {
        return __awaiter(this, void 0, void 0, function* () {
            const query = querystring.parse(uri.query);
            const gitService = yield this.svcContainer.get(types_1.IGitServiceFactory).createGitService(query.workspaceFolder, vscode_1.Uri.parse(query.fsPath));
            return gitService.getCommitFileContent(query.hash, vscode_1.Uri.file(query.fsPath));
        });
    }
}
exports.CommitFileViewerProvider = CommitFileViewerProvider;
//# sourceMappingURL=commitFileViewer.js.map