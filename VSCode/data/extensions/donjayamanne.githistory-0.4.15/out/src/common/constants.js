"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
function isTestEnvironment() {
    return process.env.VSC_GITHISTORY_CI_TEST === '1';
}
exports.isTestEnvironment = isTestEnvironment;
//# sourceMappingURL=constants.js.map