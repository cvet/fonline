"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const commitViewerFactory_1 = require("./commitViewerFactory");
const types_1 = require("./types");
function registerTypes(serviceManager) {
    serviceManager.addSingleton(types_1.ICommitViewerFactory, commitViewerFactory_1.CommitViewerFactory);
}
exports.registerTypes = registerTypes;
//# sourceMappingURL=serviceRegistry.js.map