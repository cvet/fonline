"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
/**
 * Implements ILogger as a no-op
 */
class NullLogger {
    log(msg, level) {
        // no-op
    }
    verbose(msg) {
        // no-op
    }
    warn(msg) {
        // no-op
    }
    error(msg) {
        // no-op
    }
}
exports.NullLogger = NullLogger;

//# sourceMappingURL=nullLogger.js.map
