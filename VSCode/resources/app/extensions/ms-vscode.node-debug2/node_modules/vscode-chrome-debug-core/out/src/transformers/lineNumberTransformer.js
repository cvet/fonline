"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
/**
 * Converts from 1 based lines/cols on the client side to 0 based lines/cols on the target side
 */
class LineColTransformer {
    constructor(_session) {
        this._session = _session;
    }
    setBreakpoints(args) {
        args.breakpoints.forEach(bp => this.convertClientLocationToDebugger(bp));
        if (!this.columnBreakpointsEnabled) {
            args.breakpoints.forEach(bp => bp.column = undefined);
        }
        return args;
    }
    setBreakpointsResponse(response) {
        response.breakpoints.forEach(bp => this.convertDebuggerLocationToClient(bp));
        if (!this.columnBreakpointsEnabled) {
            response.breakpoints.forEach(bp => bp.column = 1);
        }
    }
    stackTraceResponse(response) {
        response.stackFrames.forEach(frame => this.convertDebuggerLocationToClient(frame));
    }
    breakpointResolved(bp) {
        this.convertDebuggerLocationToClient(bp);
        if (!this.columnBreakpointsEnabled) {
            bp.column = undefined;
        }
    }
    scopeResponse(scopeResponse) {
        scopeResponse.scopes.forEach(scope => this.mapScopeLocations(scope));
    }
    mappedExceptionStack(location) {
        this.convertDebuggerLocationToClient(location);
    }
    mapScopeLocations(scope) {
        this.convertDebuggerLocationToClient(scope);
        if (typeof scope.endLine === 'number') {
            const endScope = { line: scope.endLine, column: scope.endColumn };
            this.convertDebuggerLocationToClient(endScope);
            scope.endLine = endScope.line;
            scope.endColumn = endScope.column;
        }
    }
    convertClientLocationToDebugger(location) {
        if (typeof location.line === 'number') {
            location.line = this.convertClientLineToDebugger(location.line);
        }
        if (typeof location.column === 'number') {
            location.column = this.convertClientColumnToDebugger(location.column);
        }
    }
    convertDebuggerLocationToClient(location) {
        if (typeof location.line === 'number') {
            location.line = this.convertDebuggerLineToClient(location.line);
        }
        if (typeof location.column === 'number') {
            location.column = this.convertDebuggerColumnToClient(location.column);
        }
    }
    convertClientLineToDebugger(line) {
        return this._session.convertClientLineToDebugger(line);
    }
    convertDebuggerLineToClient(line) {
        return this._session.convertDebuggerLineToClient(line);
    }
    convertClientColumnToDebugger(column) {
        return this._session.convertClientColumnToDebugger(column);
    }
    convertDebuggerColumnToClient(column) {
        return this._session.convertDebuggerColumnToClient(column);
    }
}
exports.LineColTransformer = LineColTransformer;

//# sourceMappingURL=lineNumberTransformer.js.map
