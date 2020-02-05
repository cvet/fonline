"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const github_1 = require("./avatar/github");
const gravatar_1 = require("./avatar/gravatar");
const types_1 = require("./avatar/types");
const index_1 = require("./exec/index");
function registerTypes(serviceManager) {
    serviceManager.add(index_1.IGitCommandExecutor, index_1.GitCommandExecutor);
    serviceManager.add(types_1.IAvatarProvider, github_1.GithubAvatarProvider);
    serviceManager.add(types_1.IAvatarProvider, gravatar_1.GravatarAvatarProvider);
}
exports.registerTypes = registerTypes;
//# sourceMappingURL=serviceRegistry.js.map