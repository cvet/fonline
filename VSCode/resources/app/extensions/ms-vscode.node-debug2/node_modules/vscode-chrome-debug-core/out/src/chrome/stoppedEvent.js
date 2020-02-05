"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const vscode_debugadapter_1 = require("vscode-debugadapter");
const utils = require("../utils");
const nls = require("vscode-nls");
const localize = nls.loadMessageBundle(__filename);
class StoppedEvent2 extends vscode_debugadapter_1.StoppedEvent {
    constructor(reason, threadId, exception) {
        const exceptionText = exception && exception.description && utils.firstLine(exception.description);
        super(reason, threadId, exceptionText);
        switch (reason) {
            case 'step':
                this.body.description = localize(0, null);
                break;
            case 'breakpoint':
                this.body.description = localize(1, null);
                break;
            case 'exception':
                const uncaught = exception && exception.uncaught; // Currently undocumented
                if (typeof uncaught === 'undefined') {
                    this.body.description = localize(2, null);
                }
                else if (uncaught) {
                    this.body.description = localize(3, null);
                }
                else {
                    this.body.description = localize(4, null);
                }
                break;
            case 'pause':
                this.body.description = localize(5, null);
                break;
            case 'entry':
                this.body.description = localize(6, null);
                break;
            case 'debugger_statement':
                this.body.description = localize(7, null);
                break;
            case 'frame_entry':
                this.body.description = localize(8, null);
                break;
            case 'promise_rejection':
                this.body.description = localize(9, null);
                this.body.reason = 'exception';
                break;
            default:
                this.body.description = 'Unknown pause reason';
                break;
        }
    }
}
exports.StoppedEvent2 = StoppedEvent2;

//# sourceMappingURL=stoppedEvent.js.map
