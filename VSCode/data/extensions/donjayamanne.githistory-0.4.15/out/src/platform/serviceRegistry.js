"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const fileSystem_1 = require("./fileSystem");
const platformService_1 = require("./platformService");
const types_1 = require("./types");
function registerTypes(serviceManager) {
    serviceManager.add(types_1.IPlatformService, platformService_1.PlatformService);
    serviceManager.add(types_1.IFileSystem, fileSystem_1.FileSystem);
}
exports.registerTypes = registerTypes;
//# sourceMappingURL=serviceRegistry.js.map