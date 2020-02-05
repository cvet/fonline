"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const terminalCommands = require("./terminal/terminalCommands");
const actionsCommands = require("./actions/actionsCommands");
const actionsCustomCommands = require("./customActions/customActionsInit");
function activate(context) {
    actionsCommands.default(context);
    terminalCommands.default(context);
    actionsCustomCommands.default(context);
}
exports.activate = activate;
function deactivate() { }
exports.deactivate = deactivate;
//# sourceMappingURL=extension.js.map