"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const enumHelper_1 = require("../common/enumHelper");
const types_1 = require("../types");
// tslint:disable-next-line:no-stateless-class no-unnecessary-class
class Helpers {
    // tslint:disable-next-line:function-name
    static GetLogArguments() {
        const args = [];
        for (const item of enumHelper_1.EnumEx.getValues(types_1.CommitInfo)) {
            if (item !== types_1.CommitInfo.NewLine) {
                args.push(Helpers.GetCommitInfoFormatCode(item));
            }
        }
        return args;
    }
    // tslint:disable-next-line:function-name
    static GetCommitInfoFormatCode(info) {
        switch (info) {
            case types_1.CommitInfo.FullHash: {
                return '%H';
            }
            case types_1.CommitInfo.ShortHash: {
                return '%h';
            }
            case types_1.CommitInfo.TreeFullHash: {
                return '%T';
            }
            case types_1.CommitInfo.TreeShortHash: {
                return '%t';
            }
            case types_1.CommitInfo.ParentFullHash: {
                return '%P';
            }
            case types_1.CommitInfo.ParentShortHash: {
                return '%p';
            }
            case types_1.CommitInfo.AuthorName: {
                return '%an';
            }
            case types_1.CommitInfo.AuthorEmail: {
                return '%ae';
            }
            case types_1.CommitInfo.AuthorDateUnixTime: {
                return '%at';
            }
            case types_1.CommitInfo.CommitterName: {
                return '%cn';
            }
            case types_1.CommitInfo.CommitterEmail: {
                return '%ce';
            }
            case types_1.CommitInfo.CommitterDateUnixTime: {
                return '%ct';
            }
            case types_1.CommitInfo.RefsNames: {
                return '%D';
            }
            case types_1.CommitInfo.Subject: {
                return '%s';
            }
            case types_1.CommitInfo.Body: {
                return '%b';
            }
            case types_1.CommitInfo.Notes: {
                return '%N';
            }
            case types_1.CommitInfo.NewLine: {
                return '%n';
            }
            default: {
                throw new Error(`Unrecognized Commit Info type ${info}`);
            }
        }
    }
}
exports.Helpers = Helpers;
//# sourceMappingURL=helpers.js.map