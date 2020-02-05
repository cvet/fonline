"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const commitFactory_1 = require("./commitFactory");
const fileCommitFactory_1 = require("./fileCommitFactory");
const types_1 = require("./types");
function registerTypes(serviceManager) {
    serviceManager.add(types_1.IFileCommitCommandFactory, fileCommitFactory_1.FileCommitCommandFactory);
    serviceManager.add(types_1.ICommitCommandFactory, commitFactory_1.CommitCommandFactory);
}
exports.registerTypes = registerTypes;
//# sourceMappingURL=serviceRegistry.js.map