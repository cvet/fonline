"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
// Do not include any VS Code types in here
// This file will be imported by the client side code (WebPack)
var BranchSelection;
(function (BranchSelection) {
    BranchSelection[BranchSelection["Current"] = 1] = "Current";
    BranchSelection[BranchSelection["All"] = 2] = "All";
})(BranchSelection = exports.BranchSelection || (exports.BranchSelection = {}));
var RefType;
(function (RefType) {
    RefType[RefType["Head"] = 0] = "Head";
    RefType[RefType["RemoteHead"] = 1] = "RemoteHead";
    RefType[RefType["Tag"] = 2] = "Tag";
})(RefType = exports.RefType || (exports.RefType = {}));
var Status;
(function (Status) {
    Status[Status["Modified"] = 0] = "Modified";
    Status[Status["Added"] = 1] = "Added";
    Status[Status["Deleted"] = 2] = "Deleted";
    Status[Status["Renamed"] = 3] = "Renamed";
    Status[Status["Copied"] = 4] = "Copied";
    Status[Status["Unmerged"] = 5] = "Unmerged";
    Status[Status["Unknown"] = 6] = "Unknown";
    Status[Status["Broken"] = 7] = "Broken";
    Status[Status["TypeChanged"] = 8] = "TypeChanged";
})(Status = exports.Status || (exports.Status = {}));
exports.IGitService = Symbol('IGitService');
exports.IOutputChannel = Symbol('IOutputChannel');
exports.IGitServiceFactory = Symbol('IGitServiceFactory');
var CommitInfo;
(function (CommitInfo) {
    CommitInfo[CommitInfo["ParentFullHash"] = 0] = "ParentFullHash";
    CommitInfo[CommitInfo["ParentShortHash"] = 1] = "ParentShortHash";
    CommitInfo[CommitInfo["RefsNames"] = 2] = "RefsNames";
    CommitInfo[CommitInfo["AuthorName"] = 3] = "AuthorName";
    CommitInfo[CommitInfo["AuthorEmail"] = 4] = "AuthorEmail";
    CommitInfo[CommitInfo["AuthorDateUnixTime"] = 5] = "AuthorDateUnixTime";
    CommitInfo[CommitInfo["CommitterName"] = 6] = "CommitterName";
    CommitInfo[CommitInfo["CommitterEmail"] = 7] = "CommitterEmail";
    CommitInfo[CommitInfo["CommitterDateUnixTime"] = 8] = "CommitterDateUnixTime";
    CommitInfo[CommitInfo["Body"] = 9] = "Body";
    CommitInfo[CommitInfo["Notes"] = 10] = "Notes";
    CommitInfo[CommitInfo["FullHash"] = 11] = "FullHash";
    CommitInfo[CommitInfo["ShortHash"] = 12] = "ShortHash";
    CommitInfo[CommitInfo["TreeFullHash"] = 13] = "TreeFullHash";
    CommitInfo[CommitInfo["TreeShortHash"] = 14] = "TreeShortHash";
    CommitInfo[CommitInfo["Subject"] = 15] = "Subject";
    CommitInfo[CommitInfo["NewLine"] = 16] = "NewLine";
})(CommitInfo = exports.CommitInfo || (exports.CommitInfo = {}));
//# sourceMappingURL=types.js.map