"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const parser_1 = require("./actionDetails/parser");
const parser_2 = require("./fileStat/parser");
const parser_3 = require("./fileStatStatus/parser");
const parser_4 = require("./log/parser");
const parser_5 = require("./refs/parser");
const headRefParser_1 = require("./refs/parsers/headRefParser");
const remoteHeadParser_1 = require("./refs/parsers/remoteHeadParser");
const tagRefParser_1 = require("./refs/parsers/tagRefParser");
const types_1 = require("./refs/types");
const types_2 = require("./types");
function registerTypes(serviceManager) {
    serviceManager.addSingleton(types_1.IRefParser, headRefParser_1.HeadRefParser);
    serviceManager.addSingleton(types_1.IRefParser, remoteHeadParser_1.RemoteHeadParser);
    serviceManager.addSingleton(types_1.IRefParser, tagRefParser_1.TagRefParser);
    serviceManager.addSingleton(types_2.IRefsParser, parser_5.RefsParser);
    serviceManager.add(types_2.IActionDetailsParser, parser_1.ActionDetailsParser);
    serviceManager.add(types_2.IFileStatStatusParser, parser_3.FileStatStatusParser);
    serviceManager.add(types_2.IFileStatParser, parser_2.FileStatParser);
    serviceManager.add(types_2.ILogParser, parser_4.LogParser);
}
exports.registerTypes = registerTypes;
//# sourceMappingURL=serviceRegistry.js.map