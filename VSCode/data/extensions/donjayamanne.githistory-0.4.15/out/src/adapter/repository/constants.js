"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const types_1 = require("../../types");
const helpers_1 = require("../helpers");
exports.LOG_ENTRY_SEPARATOR = '95E9659B-27DC-43C4-A717-D75969757EA5';
exports.ITEM_ENTRY_SEPARATOR = '95E9659B-27DC-43C4-A717-D75969757EA6';
exports.STATS_SEPARATOR = '95E9659B-27DC-43C4-A717-D75969757EA7';
// const LOG_FORMAT_ARGS = ['%D', '%H', '%h', '%T', '%t', '%P', '%p', '%an', '%ae', '%at', '%c', '%ce', '%ct', '%s', '%b', '%N'];
exports.LOG_FORMAT_ARGS = helpers_1.Helpers.GetLogArguments();
exports.newLineFormatCode = helpers_1.Helpers.GetCommitInfoFormatCode(types_1.CommitInfo.NewLine);
exports.LOG_FORMAT = `--format=${exports.LOG_ENTRY_SEPARATOR}${[...exports.LOG_FORMAT_ARGS, exports.STATS_SEPARATOR, exports.ITEM_ENTRY_SEPARATOR].join(exports.ITEM_ENTRY_SEPARATOR)}`;
//# sourceMappingURL=constants.js.map