"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const types_1 = require("../../types");
const factory_1 = require("./factory");
const git_1 = require("./git");
const gitArgsService_1 = require("./gitArgsService");
const types_2 = require("./types");
function registerTypes(serviceManager) {
    serviceManager.add(types_1.IGitService, git_1.Git);
    serviceManager.add(types_2.IGitArgsService, gitArgsService_1.GitArgsService);
    serviceManager.add(types_1.IGitServiceFactory, factory_1.GitServiceFactory);
}
exports.registerTypes = registerTypes;
//# sourceMappingURL=serviceRegistry.js.map