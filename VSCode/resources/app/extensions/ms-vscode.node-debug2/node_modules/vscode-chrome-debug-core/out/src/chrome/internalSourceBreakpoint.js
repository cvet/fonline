"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
class InternalSourceBreakpoint {
    constructor(breakpoint) {
        this.line = breakpoint.line;
        this.column = breakpoint.column;
        this.hitCondition = breakpoint.hitCondition;
        if (breakpoint.logMessage) {
            this.condition = logMessageToExpression(breakpoint.logMessage);
            if (breakpoint.condition) {
                this.condition = `(${breakpoint.condition}) && ${this.condition}`;
            }
        }
        else if (breakpoint.condition) {
            this.condition = breakpoint.condition;
        }
    }
}
InternalSourceBreakpoint.LOGPOINT_URL = 'vscode.logpoint.js';
exports.InternalSourceBreakpoint = InternalSourceBreakpoint;
function isLogpointStack(stackTrace) {
    return stackTrace && stackTrace.callFrames.length > 0 && stackTrace.callFrames[0].url === InternalSourceBreakpoint.LOGPOINT_URL;
}
function stackTraceWithoutLogpointFrame(stackTrace) {
    if (isLogpointStack(stackTrace)) {
        return Object.assign({}, stackTrace, { callFrames: stackTrace.callFrames.slice(1) });
    }
    return stackTrace;
}
exports.stackTraceWithoutLogpointFrame = stackTraceWithoutLogpointFrame;
const LOGMESSAGE_VARIABLE_REGEXP = /{(.*?)}/g;
function logMessageToExpression(msg) {
    msg = msg.replace('%', '%%');
    const args = [];
    let format = msg.replace(LOGMESSAGE_VARIABLE_REGEXP, (match, group) => {
        const a = group.trim();
        if (a) {
            args.push(`(${a})`);
            return '%O';
        }
        else {
            return '';
        }
    });
    format = format.replace('\'', '\\\'');
    const argStr = args.length ? `, ${args.join(', ')}` : '';
    return `console.log('${format}'${argStr});\n//# sourceURL=${InternalSourceBreakpoint.LOGPOINT_URL}`;
}

//# sourceMappingURL=internalSourceBreakpoint.js.map
