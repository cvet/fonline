"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const constants_1 = require("./constants");
function isRemoteHead(ref) {
    return typeof ref === 'string' && constants_1.REMOTE_REF_PREFIXES.filter(item => ref.startsWith(item)).length > 0;
}
exports.isRemoteHead = isRemoteHead;
function getRemoteHeadName(ref) {
    ref = ref || '';
    const prefix = constants_1.REMOTE_REF_PREFIXES.find(item => ref.startsWith(item));
    return prefix ? ref.substring(prefix.length) : '';
}
exports.getRemoteHeadName = getRemoteHeadName;
//# sourceMappingURL=helpers.js.map