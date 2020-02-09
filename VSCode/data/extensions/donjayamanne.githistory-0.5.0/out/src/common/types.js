"use strict";
// tslint:disable-next-line:max-classes-per-file
// tslint:disable:max-classes-per-file
Object.defineProperty(exports, "__esModule", { value: true });
exports.ILogService = Symbol('ILogService');
exports.IUiService = Symbol('IUiService');
var CallContextSource;
(function (CallContextSource) {
    CallContextSource[CallContextSource["viewer"] = 0] = "viewer";
    CallContextSource[CallContextSource["commandPalette"] = 1] = "commandPalette";
})(CallContextSource = exports.CallContextSource || (exports.CallContextSource = {}));
class CallContext {
    constructor(source, data) {
        this.source = source;
        this.data = data;
    }
}
exports.CallContext = CallContext;
class BranchDetails {
    constructor(workspaceFolder, branch) {
        this.workspaceFolder = workspaceFolder;
        this.branch = branch;
    }
}
exports.BranchDetails = BranchDetails;
class CommitDetails extends BranchDetails {
    constructor(workspaceFolder, branch, logEntry) {
        super(workspaceFolder, branch);
        this.logEntry = logEntry;
    }
}
exports.CommitDetails = CommitDetails;
class CompareCommitDetails extends CommitDetails {
    constructor(leftCommit, rightCommit, committedFiles) {
        super(leftCommit.workspaceFolder, leftCommit.branch, leftCommit.logEntry);
        this.rightCommit = rightCommit;
        this.committedFiles = committedFiles;
    }
}
exports.CompareCommitDetails = CompareCommitDetails;
// tslint:disable-next-line:max-classes-per-file
class FileCommitDetails extends CommitDetails {
    constructor(workspaceFolder, branch, logEntry, committedFile) {
        super(workspaceFolder, branch, logEntry);
        this.committedFile = committedFile;
    }
}
exports.FileCommitDetails = FileCommitDetails;
class CompareFileCommitDetails extends FileCommitDetails {
    constructor(leftCommit, rightCommit, committedFile) {
        super(leftCommit.workspaceFolder, leftCommit.branch, leftCommit.logEntry, committedFile);
        this.rightCommit = rightCommit;
        this.committedFile = committedFile;
    }
}
exports.CompareFileCommitDetails = CompareFileCommitDetails;
//# sourceMappingURL=types.js.map