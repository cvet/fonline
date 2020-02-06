"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const factory_1 = require("./factory");
const types_1 = require("./types");
function registerTypes(serviceManager) {
    serviceManager.addSingleton(types_1.INodeFactory, factory_1.StandardNodeFactory, 'standard');
    serviceManager.addSingleton(types_1.INodeFactory, factory_1.ComparisonNodeFactory, 'comparison');
}
exports.registerTypes = registerTypes;
//# sourceMappingURL=serviceRegistry.js.map