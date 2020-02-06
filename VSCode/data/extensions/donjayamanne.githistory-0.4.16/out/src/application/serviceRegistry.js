"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const applicationShell_1 = require("./applicationShell");
const commandManager_1 = require("./commandManager");
const disposableRegistry_1 = require("./disposableRegistry");
const documentManager_1 = require("./documentManager");
const stateStore_1 = require("./stateStore");
const types_1 = require("./types");
const commandManager_2 = require("./types/commandManager");
const disposableRegistry_2 = require("./types/disposableRegistry");
const documentManager_2 = require("./types/documentManager");
const stateStore_2 = require("./types/stateStore");
const workspace_1 = require("./types/workspace");
const workspace_2 = require("./workspace");
function registerTypes(serviceManager) {
    serviceManager.addSingleton(types_1.IApplicationShell, applicationShell_1.ApplicationShell);
    serviceManager.addSingleton(commandManager_2.ICommandManager, commandManager_1.CommandManager);
    serviceManager.addSingleton(disposableRegistry_2.IDisposableRegistry, disposableRegistry_1.DisposableRegistry);
    serviceManager.addSingleton(documentManager_2.IDocumentManager, documentManager_1.DocumentManager);
    serviceManager.addSingleton(workspace_1.IWorkspaceService, workspace_2.WorkspaceService);
    serviceManager.add(stateStore_2.IStateStoreFactory, stateStore_1.WorkspaceStateStoreFactory);
}
exports.registerTypes = registerTypes;
//# sourceMappingURL=serviceRegistry.js.map