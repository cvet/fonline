"use strict";
/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const protocol_1 = require("./protocol");
const messages_1 = require("./messages");
const Net = require("net");
const url_1 = require("url");
class Source {
    constructor(name, path, id = 0, origin, data) {
        this.name = name;
        this.path = path;
        this.sourceReference = id;
        if (origin) {
            this.origin = origin;
        }
        if (data) {
            this.adapterData = data;
        }
    }
}
exports.Source = Source;
class Scope {
    constructor(name, reference, expensive = false) {
        this.name = name;
        this.variablesReference = reference;
        this.expensive = expensive;
    }
}
exports.Scope = Scope;
class StackFrame {
    constructor(i, nm, src, ln = 0, col = 0) {
        this.id = i;
        this.source = src;
        this.line = ln;
        this.column = col;
        this.name = nm;
    }
}
exports.StackFrame = StackFrame;
class Thread {
    constructor(id, name) {
        this.id = id;
        if (name) {
            this.name = name;
        }
        else {
            this.name = 'Thread #' + id;
        }
    }
}
exports.Thread = Thread;
class Variable {
    constructor(name, value, ref = 0, indexedVariables, namedVariables) {
        this.name = name;
        this.value = value;
        this.variablesReference = ref;
        if (typeof namedVariables === 'number') {
            this.namedVariables = namedVariables;
        }
        if (typeof indexedVariables === 'number') {
            this.indexedVariables = indexedVariables;
        }
    }
}
exports.Variable = Variable;
class Breakpoint {
    constructor(verified, line, column, source) {
        this.verified = verified;
        const e = this;
        if (typeof line === 'number') {
            e.line = line;
        }
        if (typeof column === 'number') {
            e.column = column;
        }
        if (source) {
            e.source = source;
        }
    }
}
exports.Breakpoint = Breakpoint;
class Module {
    constructor(id, name) {
        this.id = id;
        this.name = name;
    }
}
exports.Module = Module;
class CompletionItem {
    constructor(label, start, length = 0) {
        this.label = label;
        this.start = start;
        this.length = length;
    }
}
exports.CompletionItem = CompletionItem;
class StoppedEvent extends messages_1.Event {
    constructor(reason, threadId, exceptionText) {
        super('stopped');
        this.body = {
            reason: reason
        };
        if (typeof threadId === 'number') {
            this.body.threadId = threadId;
        }
        if (typeof exceptionText === 'string') {
            this.body.text = exceptionText;
        }
    }
}
exports.StoppedEvent = StoppedEvent;
class ContinuedEvent extends messages_1.Event {
    constructor(threadId, allThreadsContinued) {
        super('continued');
        this.body = {
            threadId: threadId
        };
        if (typeof allThreadsContinued === 'boolean') {
            this.body.allThreadsContinued = allThreadsContinued;
        }
    }
}
exports.ContinuedEvent = ContinuedEvent;
class InitializedEvent extends messages_1.Event {
    constructor() {
        super('initialized');
    }
}
exports.InitializedEvent = InitializedEvent;
class TerminatedEvent extends messages_1.Event {
    constructor(restart) {
        super('terminated');
        if (typeof restart === 'boolean' || restart) {
            const e = this;
            e.body = {
                restart: restart
            };
        }
    }
}
exports.TerminatedEvent = TerminatedEvent;
class OutputEvent extends messages_1.Event {
    constructor(output, category = 'console', data) {
        super('output');
        this.body = {
            category: category,
            output: output
        };
        if (data !== undefined) {
            this.body.data = data;
        }
    }
}
exports.OutputEvent = OutputEvent;
class ThreadEvent extends messages_1.Event {
    constructor(reason, threadId) {
        super('thread');
        this.body = {
            reason: reason,
            threadId: threadId
        };
    }
}
exports.ThreadEvent = ThreadEvent;
class BreakpointEvent extends messages_1.Event {
    constructor(reason, breakpoint) {
        super('breakpoint');
        this.body = {
            reason: reason,
            breakpoint: breakpoint
        };
    }
}
exports.BreakpointEvent = BreakpointEvent;
class ModuleEvent extends messages_1.Event {
    constructor(reason, module) {
        super('module');
        this.body = {
            reason: reason,
            module: module
        };
    }
}
exports.ModuleEvent = ModuleEvent;
class LoadedSourceEvent extends messages_1.Event {
    constructor(reason, source) {
        super('loadedSource');
        this.body = {
            reason: reason,
            source: source
        };
    }
}
exports.LoadedSourceEvent = LoadedSourceEvent;
class CapabilitiesEvent extends messages_1.Event {
    constructor(capabilities) {
        super('capabilities');
        this.body = {
            capabilities: capabilities
        };
    }
}
exports.CapabilitiesEvent = CapabilitiesEvent;
var ErrorDestination;
(function (ErrorDestination) {
    ErrorDestination[ErrorDestination["User"] = 1] = "User";
    ErrorDestination[ErrorDestination["Telemetry"] = 2] = "Telemetry";
})(ErrorDestination = exports.ErrorDestination || (exports.ErrorDestination = {}));
;
class DebugSession extends protocol_1.ProtocolServer {
    constructor(obsolete_debuggerLinesAndColumnsStartAt1, obsolete_isServer) {
        super();
        const linesAndColumnsStartAt1 = typeof obsolete_debuggerLinesAndColumnsStartAt1 === 'boolean' ? obsolete_debuggerLinesAndColumnsStartAt1 : false;
        this._debuggerLinesStartAt1 = linesAndColumnsStartAt1;
        this._debuggerColumnsStartAt1 = linesAndColumnsStartAt1;
        this._debuggerPathsAreURIs = false;
        this._clientLinesStartAt1 = true;
        this._clientColumnsStartAt1 = true;
        this._clientPathsAreURIs = false;
        this._isServer = typeof obsolete_isServer === 'boolean' ? obsolete_isServer : false;
        this.on('close', () => {
            this.shutdown();
        });
        this.on('error', (error) => {
            this.shutdown();
        });
    }
    setDebuggerPathFormat(format) {
        this._debuggerPathsAreURIs = format !== 'path';
    }
    setDebuggerLinesStartAt1(enable) {
        this._debuggerLinesStartAt1 = enable;
    }
    setDebuggerColumnsStartAt1(enable) {
        this._debuggerColumnsStartAt1 = enable;
    }
    setRunAsServer(enable) {
        this._isServer = enable;
    }
    /**
     * A virtual constructor...
     */
    static run(debugSession) {
        // parse arguments
        let port = 0;
        const args = process.argv.slice(2);
        args.forEach(function (val, index, array) {
            const portMatch = /^--server=(\d{4,5})$/.exec(val);
            if (portMatch) {
                port = parseInt(portMatch[1], 10);
            }
        });
        if (port > 0) {
            // start as a server
            console.error(`waiting for debug protocol on port ${port}`);
            Net.createServer((socket) => {
                console.error('>> accepted connection from client');
                socket.on('end', () => {
                    console.error('>> client connection closed\n');
                });
                const session = new debugSession(false, true);
                session.setRunAsServer(true);
                session.start(socket, socket);
            }).listen(port);
        }
        else {
            // start a session
            //console.error('waiting for debug protocol on stdin/stdout');
            const session = new debugSession(false);
            process.on('SIGTERM', () => {
                session.shutdown();
            });
            session.start(process.stdin, process.stdout);
        }
    }
    shutdown() {
        if (this._isServer) {
            // shutdown ignored in server mode
        }
        else {
            // wait a bit before shutting down
            setTimeout(() => {
                process.exit(0);
            }, 100);
        }
    }
    sendErrorResponse(response, codeOrMessage, format, variables, dest = ErrorDestination.User) {
        let msg;
        if (typeof codeOrMessage === 'number') {
            msg = {
                id: codeOrMessage,
                format: format
            };
            if (variables) {
                msg.variables = variables;
            }
            if (dest & ErrorDestination.User) {
                msg.showUser = true;
            }
            if (dest & ErrorDestination.Telemetry) {
                msg.sendTelemetry = true;
            }
        }
        else {
            msg = codeOrMessage;
        }
        response.success = false;
        response.message = DebugSession.formatPII(msg.format, true, msg.variables);
        if (!response.body) {
            response.body = {};
        }
        response.body.error = msg;
        this.sendResponse(response);
    }
    runInTerminalRequest(args, timeout, cb) {
        this.sendRequest('runInTerminal', args, timeout, cb);
    }
    dispatchRequest(request) {
        const response = new messages_1.Response(request);
        try {
            if (request.command === 'initialize') {
                var args = request.arguments;
                if (typeof args.linesStartAt1 === 'boolean') {
                    this._clientLinesStartAt1 = args.linesStartAt1;
                }
                if (typeof args.columnsStartAt1 === 'boolean') {
                    this._clientColumnsStartAt1 = args.columnsStartAt1;
                }
                if (args.pathFormat !== 'path') {
                    this.sendErrorResponse(response, 2018, 'debug adapter only supports native paths', null, ErrorDestination.Telemetry);
                }
                else {
                    const initializeResponse = response;
                    initializeResponse.body = {};
                    this.initializeRequest(initializeResponse, args);
                }
            }
            else if (request.command === 'launch') {
                this.launchRequest(response, request.arguments, request);
            }
            else if (request.command === 'attach') {
                this.attachRequest(response, request.arguments, request);
            }
            else if (request.command === 'disconnect') {
                this.disconnectRequest(response, request.arguments, request);
            }
            else if (request.command === 'terminate') {
                this.terminateRequest(response, request.arguments, request);
            }
            else if (request.command === 'restart') {
                this.restartRequest(response, request.arguments, request);
            }
            else if (request.command === 'setBreakpoints') {
                this.setBreakPointsRequest(response, request.arguments, request);
            }
            else if (request.command === 'setFunctionBreakpoints') {
                this.setFunctionBreakPointsRequest(response, request.arguments, request);
            }
            else if (request.command === 'setExceptionBreakpoints') {
                this.setExceptionBreakPointsRequest(response, request.arguments, request);
            }
            else if (request.command === 'configurationDone') {
                this.configurationDoneRequest(response, request.arguments, request);
            }
            else if (request.command === 'continue') {
                this.continueRequest(response, request.arguments, request);
            }
            else if (request.command === 'next') {
                this.nextRequest(response, request.arguments, request);
            }
            else if (request.command === 'stepIn') {
                this.stepInRequest(response, request.arguments, request);
            }
            else if (request.command === 'stepOut') {
                this.stepOutRequest(response, request.arguments, request);
            }
            else if (request.command === 'stepBack') {
                this.stepBackRequest(response, request.arguments, request);
            }
            else if (request.command === 'reverseContinue') {
                this.reverseContinueRequest(response, request.arguments, request);
            }
            else if (request.command === 'restartFrame') {
                this.restartFrameRequest(response, request.arguments, request);
            }
            else if (request.command === 'goto') {
                this.gotoRequest(response, request.arguments, request);
            }
            else if (request.command === 'pause') {
                this.pauseRequest(response, request.arguments, request);
            }
            else if (request.command === 'stackTrace') {
                this.stackTraceRequest(response, request.arguments, request);
            }
            else if (request.command === 'scopes') {
                this.scopesRequest(response, request.arguments, request);
            }
            else if (request.command === 'variables') {
                this.variablesRequest(response, request.arguments, request);
            }
            else if (request.command === 'setVariable') {
                this.setVariableRequest(response, request.arguments, request);
            }
            else if (request.command === 'setExpression') {
                this.setExpressionRequest(response, request.arguments, request);
            }
            else if (request.command === 'source') {
                this.sourceRequest(response, request.arguments, request);
            }
            else if (request.command === 'threads') {
                this.threadsRequest(response, request);
            }
            else if (request.command === 'terminateThreads') {
                this.terminateThreadsRequest(response, request.arguments, request);
            }
            else if (request.command === 'evaluate') {
                this.evaluateRequest(response, request.arguments, request);
            }
            else if (request.command === 'stepInTargets') {
                this.stepInTargetsRequest(response, request.arguments, request);
            }
            else if (request.command === 'gotoTargets') {
                this.gotoTargetsRequest(response, request.arguments, request);
            }
            else if (request.command === 'completions') {
                this.completionsRequest(response, request.arguments, request);
            }
            else if (request.command === 'exceptionInfo') {
                this.exceptionInfoRequest(response, request.arguments, request);
            }
            else if (request.command === 'loadedSources') {
                this.loadedSourcesRequest(response, request.arguments, request);
            }
            else if (request.command === 'dataBreakpointInfo') {
                this.dataBreakpointInfoRequest(response, request.arguments, request);
            }
            else if (request.command === 'setDataBreakpoints') {
                this.setDataBreakpointsRequest(response, request.arguments, request);
            }
            else if (request.command === 'readMemory') {
                this.readMemoryRequest(response, request.arguments, request);
            }
            else if (request.command === 'disassemble') {
                this.disassembleRequest(response, request.arguments, request);
            }
            else if (request.command === 'cancel') {
                this.cancelRequest(response, request.arguments, request);
            }
            else if (request.command === 'breakpointLocations') {
                this.breakpointLocationsRequest(response, request.arguments, request);
            }
            else {
                this.customRequest(request.command, response, request.arguments, request);
            }
        }
        catch (e) {
            this.sendErrorResponse(response, 1104, '{_stack}', { _exception: e.message, _stack: e.stack }, ErrorDestination.Telemetry);
        }
    }
    initializeRequest(response, args) {
        // This default debug adapter does not support conditional breakpoints.
        response.body.supportsConditionalBreakpoints = false;
        // This default debug adapter does not support hit conditional breakpoints.
        response.body.supportsHitConditionalBreakpoints = false;
        // This default debug adapter does not support function breakpoints.
        response.body.supportsFunctionBreakpoints = false;
        // This default debug adapter implements the 'configurationDone' request.
        response.body.supportsConfigurationDoneRequest = true;
        // This default debug adapter does not support hovers based on the 'evaluate' request.
        response.body.supportsEvaluateForHovers = false;
        // This default debug adapter does not support the 'stepBack' request.
        response.body.supportsStepBack = false;
        // This default debug adapter does not support the 'setVariable' request.
        response.body.supportsSetVariable = false;
        // This default debug adapter does not support the 'restartFrame' request.
        response.body.supportsRestartFrame = false;
        // This default debug adapter does not support the 'stepInTargets' request.
        response.body.supportsStepInTargetsRequest = false;
        // This default debug adapter does not support the 'gotoTargets' request.
        response.body.supportsGotoTargetsRequest = false;
        // This default debug adapter does not support the 'completions' request.
        response.body.supportsCompletionsRequest = false;
        // This default debug adapter does not support the 'restart' request.
        response.body.supportsRestartRequest = false;
        // This default debug adapter does not support the 'exceptionOptions' attribute on the 'setExceptionBreakpoints' request.
        response.body.supportsExceptionOptions = false;
        // This default debug adapter does not support the 'format' attribute on the 'variables', 'evaluate', and 'stackTrace' request.
        response.body.supportsValueFormattingOptions = false;
        // This debug adapter does not support the 'exceptionInfo' request.
        response.body.supportsExceptionInfoRequest = false;
        // This debug adapter does not support the 'TerminateDebuggee' attribute on the 'disconnect' request.
        response.body.supportTerminateDebuggee = false;
        // This debug adapter does not support delayed loading of stack frames.
        response.body.supportsDelayedStackTraceLoading = false;
        // This debug adapter does not support the 'loadedSources' request.
        response.body.supportsLoadedSourcesRequest = false;
        // This debug adapter does not support the 'logMessage' attribute of the SourceBreakpoint.
        response.body.supportsLogPoints = false;
        // This debug adapter does not support the 'terminateThreads' request.
        response.body.supportsTerminateThreadsRequest = false;
        // This debug adapter does not support the 'setExpression' request.
        response.body.supportsSetExpression = false;
        // This debug adapter does not support the 'terminate' request.
        response.body.supportsTerminateRequest = false;
        // This debug adapter does not support data breakpoints.
        response.body.supportsDataBreakpoints = false;
        /** This debug adapter does not support the 'readMemory' request. */
        response.body.supportsReadMemoryRequest = false;
        /** The debug adapter does not support the 'disassemble' request. */
        response.body.supportsDisassembleRequest = false;
        /** The debug adapter does not support the 'cancel' request. */
        response.body.supportsCancelRequest = false;
        /** The debug adapter does not support the 'breakpointLocations' request. */
        response.body.supportsBreakpointLocationsRequest = false;
        this.sendResponse(response);
    }
    disconnectRequest(response, args, request) {
        this.sendResponse(response);
        this.shutdown();
    }
    launchRequest(response, args, request) {
        this.sendResponse(response);
    }
    attachRequest(response, args, request) {
        this.sendResponse(response);
    }
    terminateRequest(response, args, request) {
        this.sendResponse(response);
    }
    restartRequest(response, args, request) {
        this.sendResponse(response);
    }
    setBreakPointsRequest(response, args, request) {
        this.sendResponse(response);
    }
    setFunctionBreakPointsRequest(response, args, request) {
        this.sendResponse(response);
    }
    setExceptionBreakPointsRequest(response, args, request) {
        this.sendResponse(response);
    }
    configurationDoneRequest(response, args, request) {
        this.sendResponse(response);
    }
    continueRequest(response, args, request) {
        this.sendResponse(response);
    }
    nextRequest(response, args, request) {
        this.sendResponse(response);
    }
    stepInRequest(response, args, request) {
        this.sendResponse(response);
    }
    stepOutRequest(response, args, request) {
        this.sendResponse(response);
    }
    stepBackRequest(response, args, request) {
        this.sendResponse(response);
    }
    reverseContinueRequest(response, args, request) {
        this.sendResponse(response);
    }
    restartFrameRequest(response, args, request) {
        this.sendResponse(response);
    }
    gotoRequest(response, args, request) {
        this.sendResponse(response);
    }
    pauseRequest(response, args, request) {
        this.sendResponse(response);
    }
    sourceRequest(response, args, request) {
        this.sendResponse(response);
    }
    threadsRequest(response, request) {
        this.sendResponse(response);
    }
    terminateThreadsRequest(response, args, request) {
        this.sendResponse(response);
    }
    stackTraceRequest(response, args, request) {
        this.sendResponse(response);
    }
    scopesRequest(response, args, request) {
        this.sendResponse(response);
    }
    variablesRequest(response, args, request) {
        this.sendResponse(response);
    }
    setVariableRequest(response, args, request) {
        this.sendResponse(response);
    }
    setExpressionRequest(response, args, request) {
        this.sendResponse(response);
    }
    evaluateRequest(response, args, request) {
        this.sendResponse(response);
    }
    stepInTargetsRequest(response, args, request) {
        this.sendResponse(response);
    }
    gotoTargetsRequest(response, args, request) {
        this.sendResponse(response);
    }
    completionsRequest(response, args, request) {
        this.sendResponse(response);
    }
    exceptionInfoRequest(response, args, request) {
        this.sendResponse(response);
    }
    loadedSourcesRequest(response, args, request) {
        this.sendResponse(response);
    }
    dataBreakpointInfoRequest(response, args, request) {
        this.sendResponse(response);
    }
    setDataBreakpointsRequest(response, args, request) {
        this.sendResponse(response);
    }
    readMemoryRequest(response, args, request) {
        this.sendResponse(response);
    }
    disassembleRequest(response, args, request) {
        this.sendResponse(response);
    }
    cancelRequest(response, args, request) {
        this.sendResponse(response);
    }
    breakpointLocationsRequest(response, args, request) {
        this.sendResponse(response);
    }
    /**
     * Override this hook to implement custom requests.
     */
    customRequest(command, response, args, request) {
        this.sendErrorResponse(response, 1014, 'unrecognized request', null, ErrorDestination.Telemetry);
    }
    //---- protected -------------------------------------------------------------------------------------------------
    convertClientLineToDebugger(line) {
        if (this._debuggerLinesStartAt1) {
            return this._clientLinesStartAt1 ? line : line + 1;
        }
        return this._clientLinesStartAt1 ? line - 1 : line;
    }
    convertDebuggerLineToClient(line) {
        if (this._debuggerLinesStartAt1) {
            return this._clientLinesStartAt1 ? line : line - 1;
        }
        return this._clientLinesStartAt1 ? line + 1 : line;
    }
    convertClientColumnToDebugger(column) {
        if (this._debuggerColumnsStartAt1) {
            return this._clientColumnsStartAt1 ? column : column + 1;
        }
        return this._clientColumnsStartAt1 ? column - 1 : column;
    }
    convertDebuggerColumnToClient(column) {
        if (this._debuggerColumnsStartAt1) {
            return this._clientColumnsStartAt1 ? column : column - 1;
        }
        return this._clientColumnsStartAt1 ? column + 1 : column;
    }
    convertClientPathToDebugger(clientPath) {
        if (this._clientPathsAreURIs !== this._debuggerPathsAreURIs) {
            if (this._clientPathsAreURIs) {
                return DebugSession.uri2path(clientPath);
            }
            else {
                return DebugSession.path2uri(clientPath);
            }
        }
        return clientPath;
    }
    convertDebuggerPathToClient(debuggerPath) {
        if (this._debuggerPathsAreURIs !== this._clientPathsAreURIs) {
            if (this._debuggerPathsAreURIs) {
                return DebugSession.uri2path(debuggerPath);
            }
            else {
                return DebugSession.path2uri(debuggerPath);
            }
        }
        return debuggerPath;
    }
    //---- private -------------------------------------------------------------------------------
    static path2uri(path) {
        if (process.platform === 'win32') {
            if (/^[A-Z]:/.test(path)) {
                path = path[0].toLowerCase() + path.substr(1);
            }
            path = path.replace(/\\/g, '/');
        }
        path = encodeURI(path);
        let uri = new url_1.URL(`file:`); // ignore 'path' for now
        uri.pathname = path; // now use 'path' to get the correct percent encoding (see https://url.spec.whatwg.org)
        return uri.toString();
    }
    static uri2path(sourceUri) {
        let uri = new url_1.URL(sourceUri);
        let s = decodeURIComponent(uri.pathname);
        if (process.platform === 'win32') {
            if (/^\/[a-zA-Z]:/.test(s)) {
                s = s[1].toLowerCase() + s.substr(2);
            }
            s = s.replace(/\//g, '\\');
        }
        return s;
    }
    /*
    * If argument starts with '_' it is OK to send its value to telemetry.
    */
    static formatPII(format, excludePII, args) {
        return format.replace(DebugSession._formatPIIRegexp, function (match, paramName) {
            if (excludePII && paramName.length > 0 && paramName[0] !== '_') {
                return match;
            }
            return args[paramName] && args.hasOwnProperty(paramName) ?
                args[paramName] :
                match;
        });
    }
}
DebugSession._formatPIIRegexp = /{([^}]+)}/g;
exports.DebugSession = DebugSession;
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiZGVidWdTZXNzaW9uLmpzIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsiLi4vc3JjL2RlYnVnU2Vzc2lvbi50cyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiO0FBQUE7OztnR0FHZ0c7O0FBR2hHLHlDQUEwQztBQUMxQyx5Q0FBMkM7QUFDM0MsMkJBQTJCO0FBQzNCLDZCQUF3QjtBQUd4QixNQUFhLE1BQU07SUFLbEIsWUFBbUIsSUFBWSxFQUFFLElBQWEsRUFBRSxLQUFhLENBQUMsRUFBRSxNQUFlLEVBQUUsSUFBVTtRQUMxRixJQUFJLENBQUMsSUFBSSxHQUFHLElBQUksQ0FBQztRQUNqQixJQUFJLENBQUMsSUFBSSxHQUFHLElBQUksQ0FBQztRQUNqQixJQUFJLENBQUMsZUFBZSxHQUFHLEVBQUUsQ0FBQztRQUMxQixJQUFJLE1BQU0sRUFBRTtZQUNMLElBQUssQ0FBQyxNQUFNLEdBQUcsTUFBTSxDQUFDO1NBQzVCO1FBQ0QsSUFBSSxJQUFJLEVBQUU7WUFDSCxJQUFLLENBQUMsV0FBVyxHQUFHLElBQUksQ0FBQztTQUMvQjtJQUNGLENBQUM7Q0FDRDtBQWhCRCx3QkFnQkM7QUFFRCxNQUFhLEtBQUs7SUFLakIsWUFBbUIsSUFBWSxFQUFFLFNBQWlCLEVBQUUsWUFBcUIsS0FBSztRQUM3RSxJQUFJLENBQUMsSUFBSSxHQUFHLElBQUksQ0FBQztRQUNqQixJQUFJLENBQUMsa0JBQWtCLEdBQUcsU0FBUyxDQUFDO1FBQ3BDLElBQUksQ0FBQyxTQUFTLEdBQUcsU0FBUyxDQUFDO0lBQzVCLENBQUM7Q0FDRDtBQVZELHNCQVVDO0FBRUQsTUFBYSxVQUFVO0lBT3RCLFlBQW1CLENBQVMsRUFBRSxFQUFVLEVBQUUsR0FBWSxFQUFFLEtBQWEsQ0FBQyxFQUFFLE1BQWMsQ0FBQztRQUN0RixJQUFJLENBQUMsRUFBRSxHQUFHLENBQUMsQ0FBQztRQUNaLElBQUksQ0FBQyxNQUFNLEdBQUcsR0FBRyxDQUFDO1FBQ2xCLElBQUksQ0FBQyxJQUFJLEdBQUcsRUFBRSxD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R0FBRyxFQUFFO2dCQUMvRCxPQUFPLEtBQUssQ0FBQzthQUNiO1lBQ0QsT0FBTyxJQUFJLENBQUMsU0FBUyxDQUFDLElBQUksSUFBSSxDQUFDLGNBQWMsQ0FBQyxTQUFTLENBQUMsQ0FBQyxDQUFDO2dCQUN6RCxJQUFJLENBQUMsU0FBUyxDQUFDLENBQUMsQ0FBQztnQkFDakIsS0FBSyxDQUFDO1FBQ1IsQ0FBQyxDQUFDLENBQUE7SUFDSCxDQUFDOztBQWRjLDZCQUFnQixHQUFHLFlBQVksQ0FBQztBQWhtQmhELG9DQSttQkMiLCJzb3VyY2VzQ29udGVudCI6WyIvKi0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLVxuICogIENvcHlyaWdodCAoYykgTWljcm9zb2Z0IENvcnBvcmF0aW9uLiBBbGwgcmlnaHRzIHJlc2VydmVkLlxuICogIExpY2Vuc2VkIHVuZGVyIHRoZSBNSVQgTGljZW5zZS4gU2VlIExpY2Vuc2UudHh0IGluIHRoZSBwcm9qZWN0IHJvb3QgZm9yIGxpY2Vuc2UgaW5mb3JtYXRpb24uXG4gKi0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tKi9cblxuaW1wb3J0IHtEZWJ1Z1Byb3RvY29sfSBmcm9tICd2c2NvZGUtZGVidWdwcm90b2NvbCc7XG5pbXBvcnQge1Byb3RvY29sU2VydmVyfSBmcm9tICcuL3Byb3RvY29sJztcbmltcG9ydCB7UmVzcG9uc2UsIEV2ZW50fSBmcm9tICcuL21lc3NhZ2VzJztcbmltcG9ydCAqIGFzIE5ldCBmcm9tICduZXQnO1xuaW1wb3J0IHtVUkx9IGZyb20gJ3VybCc7XG5cblxuZXhwb3J0IGNsYXNzIFNvdXJjZSBpbXBsZW1lbnRzIERlYnVnUHJvdG9jb2wuU291cmNlIHtcblx0bmFtZTogc3RyaW5nO1xuXHRwYXRoOiBzdHJpbmc7XG5cdHNvdXJjZVJlZmVyZW5jZTogbnVtYmVyO1xuXG5cdHB1YmxpYyBjb25zdHJ1Y3RvcihuYW1lOiBzdHJpbmcsIHBhdGg/OiBzdHJpbmcsIGlkOiBudW1iZXIgPSAwLCBvcmlnaW4/OiBzdHJpbmcsIGRhdGE/OiBhbnkpIHtcblx0XHR0aGlzLm5hbWUgPSBuYW1lO1xuXHRcdHRoaXMucGF0aCA9IHBhdGg7XG5cdFx0dGhpcy5zb3VyY2VSZWZlcmVuY2UgPSBpZDtcblx0XHRpZiAob3JpZ2luKSB7XG5cdFx0XHQoPGFueT50aGlzKS5vcmlnaW4gPSBvcmlnaW47XG5cdFx0fVxuXHRcdGlmIChkYXRhKSB7XG5cdFx0XHQoPGFueT50aGlzKS5hZGFwdGVyRGF0YSA9IGRhdGE7XG5cdFx0fVxuXHR9XG59XG5cbmV4cG9ydCBjbGFzcyBTY29wZSBpbXBsZW1lbnRzIERlYnVnUHJvdG9jb2wuU2NvcGUge1xuXHRuYW1lOiBzdHJpbmc7XG5cdHZhcmlhYmxlc1JlZmVyZW5jZTogbnVtYmVyO1xuXHRleHBlbnNpdmU6IGJvb2xlYW47XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKG5hbWU6IHN0cmluZywgcmVmZXJlbmNlOiBudW1iZXIsIGV4cGVuc2l2ZTogYm9vbGVhbiA9IGZhbHNlKSB7XG5cdFx0dGhpcy5uYW1lID0gbmFtZTtcblx0XHR0aGlzLnZhcmlhYmxlc1JlZmVyZW5jZSA9IHJlZmVyZW5jZTtcblx0XHR0aGlzLmV4cGVuc2l2ZSA9IGV4cGVuc2l2ZTtcblx0fVxufVxuXG5leHBvcnQgY2xhc3MgU3RhY2tGcmFtZSBpbXBsZW1lbnRzIERlYnVnUHJvdG9jb2wuU3RhY2tGcmFtZSB7XG5cdGlkOiBudW1iZXI7XG5cdHNvdXJjZTogU291cmNlO1xuXHRsaW5lOiBudW1iZXI7XG5cdGNvbHVtbjogbnVtYmVyO1xuXHRuYW1lOiBzdHJpbmc7XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKGk6IG51bWJlciwgbm06IHN0cmluZywgc3JjPzogU291cmNlLCBsbjogbnVtYmVyID0gMCwgY29sOiBudW1iZXIgPSAwKSB7XG5cdFx0dGhpcy5pZCA9IGk7XG5cdFx0dGhpcy5zb3VyY2UgPSBzcmM7XG5cdFx0dGhpcy5saW5lID0gbG47XG5cdFx0dGhpcy5jb2x1bW4gPSBjb2w7XG5cdFx0dGhpcy5uYW1lID0gbm07XG5cdH1cbn1cblxuZXhwb3J0IGNsYXNzIFRocmVhZCBpbXBsZW1lbnRzIERlYnVnUHJvdG9jb2wuVGhyZWFkIHtcblx0aWQ6IG51bWJlcjtcblx0bmFtZTogc3RyaW5nO1xuXG5cdHB1YmxpYyBjb25zdHJ1Y3RvcihpZDogbnVtYmVyLCBuYW1lOiBzdHJpbmcpIHtcblx0XHR0aGlzLmlkID0gaWQ7XG5cdFx0aWYgKG5hbWUpIHtcblx0XHRcdHRoaXMubmFtZSA9IG5hbWU7XG5cdFx0fSBlbHNlIHtcblx0XHRcdHRoaXMubmFtZSA9ICdUaHJlYWQgIycgKyBpZDtcblx0XHR9XG5cdH1cbn1cblxuZXhwb3J0IGNsYXNzIFZhcmlhYmxlIGltcGxlbWVudHMgRGVidWdQcm90b2NvbC5WYXJpYWJsZSB7XG5cdG5hbWU6IHN0cmluZztcblx0dmFsdWU6IHN0cmluZztcblx0dmFyaWFibGVzUmVmZXJlbmNlOiBudW1iZXI7XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKG5hbWU6IHN0cmluZywgdmFsdWU6IHN0cmluZywgcmVmOiBudW1iZXIgPSAwLCBpbmRleGVkVmFyaWFibGVzPzogbnVtYmVyLCBuYW1lZFZhcmlhYmxlcz86IG51bWJlcikge1xuXHRcdHRoaXMubmFtZSA9IG5hbWU7XG5cdFx0dGhpcy52YWx1ZSA9IHZhbHVlO1xuXHRcdHRoaXMudmFyaWFibGVzUmVmZXJlbmNlID0gcmVmO1xuXHRcdGlmICh0eXBlb2YgbmFtZWRWYXJpYWJsZXMgPT09ICdudW1iZXInKSB7XG5cdFx0XHQoPERlYnVnUHJvdG9jb2wuVmFyaWFibGU+dGhpcykubmFtZWRWYXJpYWJsZXMgPSBuYW1lZFZhcmlhYmxlcztcblx0XHR9XG5cdFx0aWYgKHR5cGVvZiBpbmRleGVkVmFyaWFibGVzID09PSAnbnVtYmVyJykge1xuXHRcdFx0KDxEZWJ1Z1Byb3RvY29sLlZhcmlhYmxlPnRoaXMpLmluZGV4ZWRWYXJpYWJsZXMgPSBpbmRleGVkVmFyaWFibGVzO1xuXHRcdH1cblx0fVxufVxuXG5leHBvcnQgY2xhc3MgQnJlYWtwb2ludCBpbXBsZW1lbnRzIERlYnVnUHJvdG9jb2wuQnJlYWtwb2ludCB7XG5cdHZlcmlmaWVkOiBib29sZWFuO1xuXG5cdHB1YmxpYyBjb25zdHJ1Y3Rvcih2ZXJpZmllZDogYm9vbGVhbiwgbGluZT86IG51bWJlciwgY29sdW1uPzogbnVtYmVyLCBzb3VyY2U/OiBTb3VyY2UpIHtcblx0XHR0aGlzLnZlcmlmaWVkID0gdmVyaWZpZWQ7XG5cdFx0Y29uc3QgZTogRGVidWdQcm90b2NvbC5CcmVha3BvaW50ID0gdGhpcztcblx0XHRpZiAodHlwZW9mIGxpbmUgPT09ICdudW1iZXInKSB7XG5cdFx0XHRlLmxpbmUgPSBsaW5lO1xuXHRcdH1cblx0XHRpZiAodHlwZW9mIGNvbHVtbiA9PT0gJ251bWJlcicpIHtcblx0XHRcdGUuY29sdW1uID0gY29sdW1uO1xuXHRcdH1cblx0XHRpZiAoc291cmNlKSB7XG5cdFx0XHRlLnNvdXJjZSA9IHNvdXJjZTtcblx0XHR9XG5cdH1cbn1cblxuZXhwb3J0IGNsYXNzIE1vZHVsZSBpbXBsZW1lbnRzIERlYnVnUHJvdG9jb2wuTW9kdWxlIHtcblx0aWQ6IG51bWJlciB8IHN0cmluZztcblx0bmFtZTogc3RyaW5nO1xuXG5cdHB1YmxpYyBjb25zdHJ1Y3RvcihpZDogbnVtYmVyIHwgc3RyaW5nLCBuYW1lOiBzdHJpbmcpIHtcblx0XHR0aGlzLmlkID0gaWQ7XG5cdFx0dGhpcy5uYW1lID0gbmFtZTtcblx0fVxufVxuXG5leHBvcnQgY2xhc3MgQ29tcGxldGlvbkl0ZW0gaW1wbGVtZW50cyBEZWJ1Z1Byb3RvY29sLkNvbXBsZXRpb25JdGVtIHtcblx0bGFiZWw6IHN0cmluZztcblx0c3RhcnQ6IG51bWJlcjtcblx0bGVuZ3RoOiBudW1iZXI7XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKGxhYmVsOiBzdHJpbmcsIHN0YXJ0OiBudW1iZXIsIGxlbmd0aDogbnVtYmVyID0gMCkge1xuXHRcdHRoaXMubGFiZWwgPSBsYWJlbDtcblx0XHR0aGlzLnN0YXJ0ID0gc3RhcnQ7XG5cdFx0dGhpcy5sZW5ndGggPSBsZW5ndGg7XG5cdH1cbn1cblxuZXhwb3J0IGNsYXNzIFN0b3BwZWRFdmVudCBleHRlbmRzIEV2ZW50IGltcGxlbWVudHMgRGVidWdQcm90b2NvbC5TdG9wcGVkRXZlbnQge1xuXHRib2R5OiB7XG5cdFx0cmVhc29uOiBzdHJpbmc7XG5cdH07XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKHJlYXNvbjogc3RyaW5nLCB0aHJlYWRJZD86IG51bWJlciwgZXhjZXB0aW9uVGV4dD86IHN0cmluZykge1xuXHRcdHN1cGVyKCdzdG9wcGVkJyk7XG5cdFx0dGhpcy5ib2R5ID0ge1xuXHRcdFx0cmVhc29uOiByZWFzb25cblx0XHR9O1xuXHRcdGlmICh0eXBlb2YgdGhyZWFkSWQgPT09ICdudW1iZXInKSB7XG5cdFx0XHQodGhpcyBhcyBEZWJ1Z1Byb3RvY29sLlN0b3BwZWRFdmVudCkuYm9keS50aHJlYWRJZCA9IHRocmVhZElkO1xuXHRcdH1cblx0XHRpZiAodHlwZW9mIGV4Y2VwdGlvblRleHQgPT09ICdzdHJpbmcnKSB7XG5cdFx0XHQodGhpcyBhcyBEZWJ1Z1Byb3RvY29sLlN0b3BwZWRFdmVudCkuYm9keS50ZXh0ID0gZXhjZXB0aW9uVGV4dDtcblx0XHR9XG5cdH1cbn1cblxuZXhwb3J0IGNsYXNzIENvbnRpbnVlZEV2ZW50IGV4dGVuZHMgRXZlbnQgaW1wbGVtZW50cyBEZWJ1Z1Byb3RvY29sLkNvbnRpbnVlZEV2ZW50IHtcblx0Ym9keToge1xuXHRcdHRocmVhZElkOiBudW1iZXI7XG5cdH07XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKHRocmVhZElkOiBudW1iZXIsIGFsbFRocmVhZHNDb250aW51ZWQ/OiBib29sZWFuKSB7XG5cdFx0c3VwZXIoJ2NvbnRpbnVlZCcpO1xuXHRcdHRoaXMuYm9keSA9IHtcblx0XHRcdHRocmVhZElkOiB0aHJlYWRJZFxuXHRcdH07XG5cblx0XHRpZiAodHlwZW9mIGFsbFRocmVhZHNDb250aW51ZWQgPT09ICdib29sZWFuJykge1xuXHRcdFx0KDxEZWJ1Z1Byb3RvY29sLkNvbnRpbnVlZEV2ZW50PnRoaXMpLmJvZHkuYWxsVGhyZWFkc0NvbnRpbnVlZCA9IGFsbFRocmVhZHNDb250aW51ZWQ7XG5cdFx0fVxuXHR9XG59XG5cbmV4cG9ydCBjbGFzcyBJbml0aWFsaXplZEV2ZW50IGV4dGVuZHMgRXZlbnQgaW1wbGVtZW50cyBEZWJ1Z1Byb3RvY29sLkluaXRpYWxpemVkRXZlbnQge1xuXHRwdWJsaWMgY29uc3RydWN0b3IoKSB7XG5cdFx0c3VwZXIoJ2luaXRpYWxpemVkJyk7XG5cdH1cbn1cblxuZXhwb3J0IGNsYXNzIFRlcm1pbmF0ZWRFdmVudCBleHRlbmRzIEV2ZW50IGltcGxlbWVudHMgRGVidWdQcm90b2NvbC5UZXJtaW5hdGVkRXZlbnQge1xuXHRwdWJsaWMgY29uc3RydWN0b3IocmVzdGFydD86IGFueSkge1xuXHRcdHN1cGVyKCd0ZXJtaW5hdGVkJyk7XG5cdFx0aWYgKHR5cGVvZiByZXN0YXJ0ID09PSAnYm9vbGVhbicgfHwgcmVzdGFydCkge1xuXHRcdFx0Y29uc3QgZTogRGVidWdQcm90b2NvbC5UZXJtaW5hdGVkRXZlbnQgPSB0aGlzO1xuXHRcdFx0ZS5ib2R5ID0ge1xuXHRcdFx0XHRyZXN0YXJ0OiByZXN0YXJ0XG5cdFx0XHR9O1xuXHRcdH1cblx0fVxufVxuXG5leHBvcnQgY2xhc3MgT3V0cHV0RXZlbnQgZXh0ZW5kcyBFdmVudCBpbXBsZW1lbnRzIERlYnVnUHJvdG9jb2wuT3V0cHV0RXZlbnQge1xuXHRib2R5OiB7XG5cdFx0Y2F0ZWdvcnk6IHN0cmluZyxcblx0XHRvdXRwdXQ6IHN0cmluZyxcblx0XHRkYXRhPzogYW55XG5cdH07XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKG91dHB1dDogc3RyaW5nLCBjYXRlZ29yeTogc3RyaW5nID0gJ2NvbnNvbGUnLCBkYXRhPzogYW55KSB7XG5cdFx0c3VwZXIoJ291dHB1dCcpO1xuXHRcdHRoaXMuYm9keSA9IHtcblx0XHRcdGNhdGVnb3J5OiBjYXRlZ29yeSxcblx0XHRcdG91dHB1dDogb3V0cHV0XG5cdFx0fTtcblx0XHRpZiAoZGF0YSAhPT0gdW5kZWZpbmVkKSB7XG5cdFx0XHR0aGlzLmJvZHkuZGF0YSA9IGRhdGE7XG5cdFx0fVxuXHR9XG59XG5cbmV4cG9ydCBjbGFzcyBUaHJlYWRFdmVudCBleHRlbmRzIEV2ZW50IGltcGxlbWVudHMgRGVidWdQcm90b2NvbC5UaHJlYWRFdmVudCB7XG5cdGJvZHk6IHtcblx0XHRyZWFzb246IHN0cmluZyxcblx0XHR0aHJlYWRJZDogbnVtYmVyXG5cdH07XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKHJlYXNvbjogc3RyaW5nLCB0aHJlYWRJZDogbnVtYmVyKSB7XG5cdFx0c3VwZXIoJ3RocmVhZCcpO1xuXHRcdHRoaXMuYm9keSA9IHtcblx0XHRcdHJlYXNvbjogcmVhc29uLFxuXHRcdFx0dGhyZWFkSWQ6IHRocmVhZElkXG5cdFx0fTtcblx0fVxufVxuXG5leHBvcnQgY2xhc3MgQnJlYWtwb2ludEV2ZW50IGV4dGVuZHMgRXZlbnQgaW1wbGVtZW50cyBEZWJ1Z1Byb3RvY29sLkJyZWFrcG9pbnRFdmVudCB7XG5cdGJvZHk6IHtcblx0XHRyZWFzb246IHN0cmluZyxcblx0XHRicmVha3BvaW50OiBCcmVha3BvaW50XG5cdH07XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKHJlYXNvbjogc3RyaW5nLCBicmVha3BvaW50OiBCcmVha3BvaW50KSB7XG5cdFx0c3VwZXIoJ2JyZWFrcG9pbnQnKTtcblx0XHR0aGlzLmJvZHkgPSB7XG5cdFx0XHRyZWFzb246IHJlYXNvbixcblx0XHRcdGJyZWFrcG9pbnQ6IGJyZWFrcG9pbnRcblx0XHR9O1xuXHR9XG59XG5cbmV4cG9ydCBjbGFzcyBNb2R1bGVFdmVudCBleHRlbmRzIEV2ZW50IGltcGxlbWVudHMgRGVidWdQcm90b2NvbC5Nb2R1bGVFdmVudCB7XG5cdGJvZHk6IHtcblx0XHRyZWFzb246ICduZXcnIHwgJ2NoYW5nZWQnIHwgJ3JlbW92ZWQnLFxuXHRcdG1vZHVsZTogTW9kdWxlXG5cdH07XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKHJlYXNvbjogJ25ldycgfCAnY2hhbmdlZCcgfCAncmVtb3ZlZCcsIG1vZHVsZTogTW9kdWxlKSB7XG5cdFx0c3VwZXIoJ21vZHVsZScpO1xuXHRcdHRoaXMuYm9keSA9IHtcblx0XHRcdHJlYXNvbjogcmVhc29uLFxuXHRcdFx0bW9kdWxlOiBtb2R1bGVcblx0XHR9O1xuXHR9XG59XG5cbmV4cG9ydCBjbGFzcyBMb2FkZWRTb3VyY2VFdmVudCBleHRlbmRzIEV2ZW50IGltcGxlbWVudHMgRGVidWdQcm90b2NvbC5Mb2FkZWRTb3VyY2VFdmVudCB7XG5cdGJvZHk6IHtcblx0XHRyZWFzb246ICduZXcnIHwgJ2NoYW5nZWQnIHwgJ3JlbW92ZWQnLFxuXHRcdHNvdXJjZTogU291cmNlXG5cdH07XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKHJlYXNvbjogJ25ldycgfCAnY2hhbmdlZCcgfCAncmVtb3ZlZCcsIHNvdXJjZTogU291cmNlKSB7XG5cdFx0c3VwZXIoJ2xvYWRlZFNvdXJjZScpO1xuXHRcdHRoaXMuYm9keSA9IHtcblx0XHRcdHJlYXNvbjogcmVhc29uLFxuXHRcdFx0c291cmNlOiBzb3VyY2Vcblx0XHR9O1xuXHR9XG59XG5cbmV4cG9ydCBjbGFzcyBDYXBhYmlsaXRpZXNFdmVudCBleHRlbmRzIEV2ZW50IGltcGxlbWVudHMgRGVidWdQcm90b2NvbC5DYXBhYmlsaXRpZXNFdmVudCB7XG5cdGJvZHk6IHtcblx0XHRjYXBhYmlsaXRpZXM6IERlYnVnUHJvdG9jb2wuQ2FwYWJpbGl0aWVzXG5cdH07XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKGNhcGFiaWxpdGllczogRGVidWdQcm90b2NvbC5DYXBhYmlsaXRpZXMpIHtcblx0XHRzdXBlcignY2FwYWJpbGl0aWVzJyk7XG5cdFx0dGhpcy5ib2R5ID0ge1xuXHRcdFx0Y2FwYWJpbGl0aWVzOiBjYXBhYmlsaXRpZXNcblx0XHR9O1xuXHR9XG59XG5cbmV4cG9ydCBlbnVtIEVycm9yRGVzdGluYXRpb24ge1xuXHRVc2VyID0gMSxcblx0VGVsZW1ldHJ5ID0gMlxufTtcblxuZXhwb3J0IGNsYXNzIERlYnVnU2Vzc2lvbiBleHRlbmRzIFByb3RvY29sU2VydmVyIHtcblxuXHRwcml2YXRlIF9kZWJ1Z2dlckxpbmVzU3RhcnRBdDE6IGJvb2xlYW47XG5cdHByaXZhdGUgX2RlYnVnZ2VyQ29sdW1uc1N0YXJ0QXQxOiBib29sZWFuO1xuXHRwcml2YXRlIF9kZWJ1Z2dlclBhdGhzQXJlVVJJczogYm9vbGVhbjtcblxuXHRwcml2YXRlIF9jbGllbnRMaW5lc1N0YXJ0QXQxOiBib29sZWFuO1xuXHRwcml2YXRlIF9jbGllbnRDb2x1bW5zU3RhcnRBdDE6IGJvb2xlYW47XG5cdHByaXZhdGUgX2NsaWVudFBhdGhzQXJlVVJJczogYm9vbGVhbjtcblxuXHRwcm90ZWN0ZWQgX2lzU2VydmVyOiBib29sZWFuO1xuXG5cdHB1YmxpYyBjb25zdHJ1Y3RvcihvYnNvbGV0ZV9kZWJ1Z2dlckxpbmVzQW5kQ29sdW1uc1N0YXJ0QXQxPzogYm9vbGVhbiwgb2Jzb2xldGVfaXNTZXJ2ZXI/OiBib29sZWFuKSB7XG5cdFx0c3VwZXIoKTtcblxuXHRcdGNvbnN0IGxpbmVzQW5kQ29sdW1uc1N0YXJ0QXQxID0gdHlwZW9mIG9ic29sZXRlX2RlYnVnZ2VyTGluZXNBbmRDb2x1bW5zU3RhcnRBdDEgPT09ICdib29sZWFuJyA/IG9ic29sZXRlX2RlYnVnZ2VyTGluZXNBbmRDb2x1bW5zU3RhcnRBdDEgOiBmYWxzZTtcblx0XHR0aGlzLl9kZWJ1Z2dlckxpbmVzU3RhcnRBdDEgPSBsaW5lc0FuZENvbHVtbnNTdGFydEF0MTtcblx0XHR0aGlzLl9kZWJ1Z2dlckNvbHVtbnNTdGFydEF0MSA9IGxpbmVzQW5kQ29sdW1uc1N0YXJ0QXQxO1xuXHRcdHRoaXMuX2RlYnVnZ2VyUGF0aHNBcmVVUklzID0gZmFsc2U7XG5cblx0XHR0aGlzLl9jbGllbnRMaW5lc1N0YXJ0QXQxID0gdHJ1ZTtcblx0XHR0aGlzLl9jbGllbnRDb2x1bW5zU3RhcnRBdDEgPSB0cnVlO1xuXHRcdHRoaXMuX2NsaWVudFBhdGhzQXJlVVJJcyA9IGZhbHNlO1xuXG5cdFx0dGhpcy5faXNTZXJ2ZXIgPSB0eXBlb2Ygb2Jzb2xldGVfaXNTZXJ2ZXIgPT09ICdib29sZWFuJyA/IG9ic29sZXRlX2lzU2VydmVyIDogZmFsc2U7XG5cblx0XHR0aGlzLm9uKCdjbG9zZScsICgpID0+IHtcblx0XHRcdHRoaXMuc2h1dGRvd24oKTtcblx0XHR9KTtcblx0XHR0aGlzLm9uKCdlcnJvcicsIChlcnJvcikgPT4ge1xuXHRcdFx0dGhpcy5zaHV0ZG93bigpO1xuXHRcdH0pO1xuXHR9XG5cblx0cHVibGljIHNldERlYnVnZ2VyUGF0aEZvcm1hdChmb3JtYXQ6IHN0cmluZykge1xuXHRcdHRoaXMuX2RlYnVnZ2VyUGF0aHNBcmVVUklzID0gZm9ybWF0ICE9PSAncGF0aCc7XG5cdH1cblxuXHRwdWJsaWMgc2V0RGVidWdnZXJMaW5lc1N0YXJ0QXQxKGVuYWJsZTogYm9vbGVhbikge1xuXHRcdHRoaXMuX2RlYnVnZ2VyTGluZXNTdGFydEF0MSA9IGVuYWJsZTtcblx0fVxuXG5cdHB1YmxpYyBzZXREZWJ1Z2dlckNvbHVtbnNTdGFydEF0MShlbmFibGU6IGJvb2xlYW4pIHtcblx0XHR0aGlzLl9kZWJ1Z2dlckNvbHVtbnNTdGFydEF0MSA9IGVuYWJsZTtcblx0fVxuXG5cdHB1YmxpYyBzZXRSdW5Bc1NlcnZlcihlbmFibGU6IGJvb2xlYW4pIHtcblx0XHR0aGlzLl9pc1NlcnZlciA9IGVuYWJsZTtcblx0fVxuXG5cdC8qKlxuXHQgKiBBIHZpcnR1YWwgY29uc3RydWN0b3IuLi5cblx0ICovXG5cdHB1YmxpYyBzdGF0aWMgcnVuKGRlYnVnU2Vzc2lvbjogdHlwZW9mIERlYnVnU2Vzc2lvbikge1xuXG5cdFx0Ly8gcGFyc2UgYXJndW1lbnRzXG5cdFx0bGV0IHBvcnQgPSAwO1xuXHRcdGNvbnN0IGFyZ3MgPSBwcm9jZXNzLmFyZ3Yuc2xpY2UoMik7XG5cdFx0YXJncy5mb3JFYWNoKGZ1bmN0aW9uICh2YWwsIGluZGV4LCBhcnJheSkge1xuXHRcdFx0Y29uc3QgcG9ydE1hdGNoID0gL14tLXNlcnZlcj0oXFxkezQsNX0pJC8uZXhlYyh2YWwpO1xuXHRcdFx0aWYgKHBvcnRNYXRjaCkge1xuXHRcdFx0XHRwb3J0ID0gcGFyc2VJbnQocG9ydE1hdGNoWzFdLCAxMCk7XG5cdFx0XHR9XG5cdFx0fSk7XG5cblx0XHRpZiAocG9ydCA+IDApIHtcblx0XHRcdC8vIHN0YXJ0IGFzIGEgc2VydmVyXG5cdFx0XHRjb25zb2xlLmVycm9yKGB3YWl0aW5nIGZvciBkZWJ1ZyBwcm90b2NvbCBvbiBwb3J0ICR7cG9ydH1gKTtcblx0XHRcdE5ldC5jcmVhdGVTZXJ2ZXIoKHNvY2tldCkgPT4ge1xuXHRcdFx0XHRjb25zb2xlLmVycm9yKCc+PiBhY2NlcHRlZCBjb25uZWN0aW9uIGZyb20gY2xpZW50Jyk7XG5cdFx0XHRcdHNvY2tldC5vbignZW5kJywgKCkgPT4ge1xuXHRcdFx0XHRcdGNvbnNvbGUuZXJyb3IoJz4+IGNsaWVudCBjb25uZWN0aW9uIGNsb3NlZFxcbicpO1xuXHRcdFx0XHR9KTtcblx0XHRcdFx0Y29uc3Qgc2Vzc2lvbiA9IG5ldyBkZWJ1Z1Nlc3Npb24oZmFsc2UsIHRydWUpO1xuXHRcdFx0XHRzZXNzaW9uLnNldFJ1bkFzU2VydmVyKHRydWUpO1xuXHRcdFx0XHRzZXNzaW9uLnN0YXJ0KHNvY2tldCwgc29ja2V0KTtcblx0XHRcdH0pLmxpc3Rlbihwb3J0KTtcblx0XHR9IGVsc2Uge1xuXG5cdFx0XHQvLyBzdGFydCBhIHNlc3Npb25cblx0XHRcdC8vY29uc29sZS5lcnJvcignd2FpdGluZyBmb3IgZGVidWcgcHJvdG9jb2wgb24gc3RkaW4vc3Rkb3V0Jyk7XG5cdFx0XHRjb25zdCBzZXNzaW9uID0gbmV3IGRlYnVnU2Vzc2lvbihmYWxzZSk7XG5cdFx0XHRwcm9jZXNzLm9uKCdTSUdURVJNJywgKCkgPT4ge1xuXHRcdFx0XHRzZXNzaW9uLnNodXRkb3duKCk7XG5cdFx0XHR9KTtcblx0XHRcdHNlc3Npb24uc3RhcnQocHJvY2Vzcy5zdGRpbiwgcHJvY2Vzcy5zdGRvdXQpO1xuXHRcdH1cblx0fVxuXG5cdHB1YmxpYyBzaHV0ZG93bigpOiB2b2lkIHtcblx0XHRpZiAodGhpcy5faXNTZXJ2ZXIpIHtcblx0XHRcdC8vIHNodXRkb3duIGlnbm9yZWQgaW4gc2VydmVyIG1vZGVcblx0XHR9IGVsc2Uge1xuXHRcdFx0Ly8gd2FpdCBhIGJpdCBiZWZvcmUgc2h1dHRpbmcgZG93blxuXHRcdFx0c2V0VGltZW91dCgoKSA9PiB7XG5cdFx0XHRcdHByb2Nlc3MuZXhpdCgwKTtcblx0XHRcdH0sIDEwMCk7XG5cdFx0fVxuXHR9XG5cblx0cHJvdGVjdGVkIHNlbmRFcnJvclJlc3BvbnNlKHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLlJlc3BvbnNlLCBjb2RlT3JNZXNzYWdlOiBudW1iZXIgfCBEZWJ1Z1Byb3RvY29sLk1lc3NhZ2UsIGZvcm1hdD86IHN0cmluZywgdmFyaWFibGVzPzogYW55LCBkZXN0OiBFcnJvckRlc3RpbmF0aW9uID0gRXJyb3JEZXN0aW5hdGlvbi5Vc2VyKTogdm9pZCB7XG5cblx0XHRsZXQgbXNnIDogRGVidWdQcm90b2NvbC5NZXNzYWdlO1xuXHRcdGlmICh0eXBlb2YgY29kZU9yTWVzc2FnZSA9PT0gJ251bWJlcicpIHtcblx0XHRcdG1zZyA9IDxEZWJ1Z1Byb3RvY29sLk1lc3NhZ2U+IHtcblx0XHRcdFx0aWQ6IDxudW1iZXI+IGNvZGVPck1lc3NhZ2UsXG5cdFx0XHRcdGZvcm1hdDogZm9ybWF0XG5cdFx0XHR9O1xuXHRcdFx0aWYgKHZhcmlhYmxlcykge1xuXHRcdFx0XHRtc2cudmFyaWFibGVzID0gdmFyaWFibGVzO1xuXHRcdFx0fVxuXHRcdFx0aWYgKGRlc3QgJiBFcnJvckRlc3RpbmF0aW9uLlVzZXIpIHtcblx0XHRcdFx0bXNnLnNob3dVc2VyID0gdHJ1ZTtcblx0XHRcdH1cblx0XHRcdGlmIChkZXN0ICYgRXJyb3JEZXN0aW5hdGlvbi5UZWxlbWV0cnkpIHtcblx0XHRcdFx0bXNnLnNlbmRUZWxlbWV0cnkgPSB0cnVlO1xuXHRcdFx0fVxuXHRcdH0gZWxzZSB7XG5cdFx0XHRtc2cgPSBjb2RlT3JNZXNzYWdlO1xuXHRcdH1cblxuXHRcdHJlc3BvbnNlLnN1Y2Nlc3MgPSBmYWxzZTtcblx0XHRyZXNwb25zZS5tZXNzYWdlID0gRGVidWdTZXNzaW9uLmZvcm1hdFBJSShtc2cuZm9ybWF0LCB0cnVlLCBtc2cudmFyaWFibGVzKTtcblx0XHRpZiAoIXJlc3BvbnNlLmJvZHkpIHtcblx0XHRcdHJlc3BvbnNlLmJvZHkgPSB7IH07XG5cdFx0fVxuXHRcdHJlc3BvbnNlLmJvZHkuZXJyb3IgPSBtc2c7XG5cblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwdWJsaWMgcnVuSW5UZXJtaW5hbFJlcXVlc3QoYXJnczogRGVidWdQcm90b2NvbC5SdW5JblRlcm1pbmFsUmVxdWVzdEFyZ3VtZW50cywgdGltZW91dDogbnVtYmVyLCBjYjogKHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLlJ1bkluVGVybWluYWxSZXNwb25zZSkgPT4gdm9pZCkge1xuXHRcdHRoaXMuc2VuZFJlcXVlc3QoJ3J1bkluVGVybWluYWwnLCBhcmdzLCB0aW1lb3V0LCBjYik7XG5cdH1cblxuXHRwcm90ZWN0ZWQgZGlzcGF0Y2hSZXF1ZXN0KHJlcXVlc3Q6IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXG5cdFx0Y29uc3QgcmVzcG9uc2UgPSBuZXcgUmVzcG9uc2UocmVxdWVzdCk7XG5cblx0XHR0cnkge1xuXHRcdFx0aWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ2luaXRpYWxpemUnKSB7XG5cdFx0XHRcdHZhciBhcmdzID0gPERlYnVnUHJvdG9jb2wuSW5pdGlhbGl6ZVJlcXVlc3RBcmd1bWVudHM+IHJlcXVlc3QuYXJndW1lbnRzO1xuXG5cdFx0XHRcdGlmICh0eXBlb2YgYXJncy5saW5lc1N0YXJ0QXQxID09PSAnYm9vbGVhbicpIHtcblx0XHRcdFx0XHR0aGlzLl9jbGllbnRMaW5lc1N0YXJ0QXQxID0gYXJncy5saW5lc1N0YXJ0QXQxO1xuXHRcdFx0XHR9XG5cdFx0XHRcdGlmICh0eXBlb2YgYXJncy5jb2x1bW5zU3RhcnRBdDEgPT09ICdib29sZWFuJykge1xuXHRcdFx0XHRcdHRoaXMuX2NsaWVudENvbHVtbnNTdGFydEF0MSA9IGFyZ3MuY29sdW1uc1N0YXJ0QXQxO1xuXHRcdFx0XHR9XG5cblx0XHRcdFx0aWYgKGFyZ3MucGF0aEZvcm1hdCAhPT0gJ3BhdGgnKSB7XG5cdFx0XHRcdFx0dGhpcy5zZW5kRXJyb3JSZXNwb25zZShyZXNwb25zZSwgMjAxOCwgJ2RlYnVnIGFkYXB0ZXIgb25seSBzdXBwb3J0cyBuYXRpdmUgcGF0aHMnLCBudWxsLCBFcnJvckRlc3RpbmF0aW9uLlRlbGVtZXRyeSk7XG5cdFx0XHRcdH0gZWxzZSB7XG5cdFx0XHRcdFx0Y29uc3QgaW5pdGlhbGl6ZVJlc3BvbnNlID0gPERlYnVnUHJvdG9jb2wuSW5pdGlhbGl6ZVJlc3BvbnNlPiByZXNwb25zZTtcblx0XHRcdFx0XHRpbml0aWFsaXplUmVzcG9uc2UuYm9keSA9IHt9O1xuXHRcdFx0XHRcdHRoaXMuaW5pdGlhbGl6ZVJlcXVlc3QoaW5pdGlhbGl6ZVJlc3BvbnNlLCBhcmdzKTtcblx0XHRcdFx0fVxuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ2xhdW5jaCcpIHtcblx0XHRcdFx0dGhpcy5sYXVuY2hSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLkxhdW5jaFJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ2F0dGFjaCcpIHtcblx0XHRcdFx0dGhpcy5hdHRhY2hSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLkF0dGFjaFJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ2Rpc2Nvbm5lY3QnKSB7XG5cdFx0XHRcdHRoaXMuZGlzY29ubmVjdFJlcXVlc3QoPERlYnVnUHJvdG9jb2wuRGlzY29ubmVjdFJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3Rlcm1pbmF0ZScpIHtcblx0XHRcdFx0dGhpcy50ZXJtaW5hdGVSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLlRlcm1pbmF0ZVJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3Jlc3RhcnQnKSB7XG5cdFx0XHRcdHRoaXMucmVzdGFydFJlcXVlc3QoPERlYnVnUHJvdG9jb2wuUmVzdGFydFJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3NldEJyZWFrcG9pbnRzJykge1xuXHRcdFx0XHR0aGlzLnNldEJyZWFrUG9pbnRzUmVxdWVzdCg8RGVidWdQcm90b2NvbC5TZXRCcmVha3BvaW50c1Jlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3NldEZ1bmN0aW9uQnJlYWtwb2ludHMnKSB7XG5cdFx0XHRcdHRoaXMuc2V0RnVuY3Rpb25CcmVha1BvaW50c1JlcXVlc3QoPERlYnVnUHJvdG9jb2wuU2V0RnVuY3Rpb25CcmVha3BvaW50c1Jlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3NldEV4Y2VwdGlvbkJyZWFrcG9pbnRzJykge1xuXHRcdFx0XHR0aGlzLnNldEV4Y2VwdGlvbkJyZWFrUG9pbnRzUmVxdWVzdCg8RGVidWdQcm90b2NvbC5TZXRFeGNlcHRpb25CcmVha3BvaW50c1Jlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ2NvbmZpZ3VyYXRpb25Eb25lJykge1xuXHRcdFx0XHR0aGlzLmNvbmZpZ3VyYXRpb25Eb25lUmVxdWVzdCg8RGVidWdQcm90b2NvbC5Db25maWd1cmF0aW9uRG9uZVJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ2NvbnRpbnVlJykge1xuXHRcdFx0XHR0aGlzLmNvbnRpbnVlUmVxdWVzdCg8RGVidWdQcm90b2NvbC5Db250aW51ZVJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ25leHQnKSB7XG5cdFx0XHRcdHRoaXMubmV4dFJlcXVlc3QoPERlYnVnUHJvdG9jb2wuTmV4dFJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3N0ZXBJbicpIHtcblx0XHRcdFx0dGhpcy5zdGVwSW5SZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLlN0ZXBJblJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3N0ZXBPdXQnKSB7XG5cdFx0XHRcdHRoaXMuc3RlcE91dFJlcXVlc3QoPERlYnVnUHJvdG9jb2wuU3RlcE91dFJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3N0ZXBCYWNrJykge1xuXHRcdFx0XHR0aGlzLnN0ZXBCYWNrUmVxdWVzdCg8RGVidWdQcm90b2NvbC5TdGVwQmFja1Jlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3JldmVyc2VDb250aW51ZScpIHtcblx0XHRcdFx0dGhpcy5yZXZlcnNlQ29udGludWVSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLlJldmVyc2VDb250aW51ZVJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3Jlc3RhcnRGcmFtZScpIHtcblx0XHRcdFx0dGhpcy5yZXN0YXJ0RnJhbWVSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLlJlc3RhcnRGcmFtZVJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ2dvdG8nKSB7XG5cdFx0XHRcdHRoaXMuZ290b1JlcXVlc3QoPERlYnVnUHJvdG9jb2wuR290b1Jlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3BhdXNlJykge1xuXHRcdFx0XHR0aGlzLnBhdXNlUmVxdWVzdCg8RGVidWdQcm90b2NvbC5QYXVzZVJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3N0YWNrVHJhY2UnKSB7XG5cdFx0XHRcdHRoaXMuc3RhY2tUcmFjZVJlcXVlc3QoPERlYnVnUHJvdG9jb2wuU3RhY2tUcmFjZVJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3Njb3BlcycpIHtcblx0XHRcdFx0dGhpcy5zY29wZXNSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLlNjb3Blc1Jlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3ZhcmlhYmxlcycpIHtcblx0XHRcdFx0dGhpcy52YXJpYWJsZXNSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLlZhcmlhYmxlc1Jlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3NldFZhcmlhYmxlJykge1xuXHRcdFx0XHR0aGlzLnNldFZhcmlhYmxlUmVxdWVzdCg8RGVidWdQcm90b2NvbC5TZXRWYXJpYWJsZVJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3NldEV4cHJlc3Npb24nKSB7XG5cdFx0XHRcdHRoaXMuc2V0RXhwcmVzc2lvblJlcXVlc3QoPERlYnVnUHJvdG9jb2wuU2V0RXhwcmVzc2lvblJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3NvdXJjZScpIHtcblx0XHRcdFx0dGhpcy5zb3VyY2VSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLlNvdXJjZVJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXG5cdFx0XHR9IGVsc2UgaWYgKHJlcXVlc3QuY29tbWFuZCA9PT0gJ3RocmVhZHMnKSB7XG5cdFx0XHRcdHRoaXMudGhyZWFkc1JlcXVlc3QoPERlYnVnUHJvdG9jb2wuVGhyZWFkc1Jlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAndGVybWluYXRlVGhyZWFkcycpIHtcblx0XHRcdFx0dGhpcy50ZXJtaW5hdGVUaHJlYWRzUmVxdWVzdCg8RGVidWdQcm90b2NvbC5UZXJtaW5hdGVUaHJlYWRzUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAnZXZhbHVhdGUnKSB7XG5cdFx0XHRcdHRoaXMuZXZhbHVhdGVSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLkV2YWx1YXRlUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAnc3RlcEluVGFyZ2V0cycpIHtcblx0XHRcdFx0dGhpcy5zdGVwSW5UYXJnZXRzUmVxdWVzdCg8RGVidWdQcm90b2NvbC5TdGVwSW5UYXJnZXRzUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAnZ290b1RhcmdldHMnKSB7XG5cdFx0XHRcdHRoaXMuZ290b1RhcmdldHNSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLkdvdG9UYXJnZXRzUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAnY29tcGxldGlvbnMnKSB7XG5cdFx0XHRcdHRoaXMuY29tcGxldGlvbnNSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLkNvbXBsZXRpb25zUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAnZXhjZXB0aW9uSW5mbycpIHtcblx0XHRcdFx0dGhpcy5leGNlcHRpb25JbmZvUmVxdWVzdCg8RGVidWdQcm90b2NvbC5FeGNlcHRpb25JbmZvUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAnbG9hZGVkU291cmNlcycpIHtcblx0XHRcdFx0dGhpcy5sb2FkZWRTb3VyY2VzUmVxdWVzdCg8RGVidWdQcm90b2NvbC5Mb2FkZWRTb3VyY2VzUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAnZGF0YUJyZWFrcG9pbnRJbmZvJykge1xuXHRcdFx0XHR0aGlzLmRhdGFCcmVha3BvaW50SW5mb1JlcXVlc3QoPERlYnVnUHJvdG9jb2wuRGF0YUJyZWFrcG9pbnRJbmZvUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAnc2V0RGF0YUJyZWFrcG9pbnRzJykge1xuXHRcdFx0XHR0aGlzLnNldERhdGFCcmVha3BvaW50c1JlcXVlc3QoPERlYnVnUHJvdG9jb2wuU2V0RGF0YUJyZWFrcG9pbnRzUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAncmVhZE1lbW9yeScpIHtcblx0XHRcdFx0dGhpcy5yZWFkTWVtb3J5UmVxdWVzdCg8RGVidWdQcm90b2NvbC5SZWFkTWVtb3J5UmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAnZGlzYXNzZW1ibGUnKSB7XG5cdFx0XHRcdHRoaXMuZGlzYXNzZW1ibGVSZXF1ZXN0KDxEZWJ1Z1Byb3RvY29sLkRpc2Fzc2VtYmxlUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAnY2FuY2VsJykge1xuXHRcdFx0XHR0aGlzLmNhbmNlbFJlcXVlc3QoPERlYnVnUHJvdG9jb2wuQ2FuY2VsUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSBpZiAocmVxdWVzdC5jb21tYW5kID09PSAnYnJlYWtwb2ludExvY2F0aW9ucycpIHtcblx0XHRcdFx0dGhpcy5icmVha3BvaW50TG9jYXRpb25zUmVxdWVzdCg8RGVidWdQcm90b2NvbC5CcmVha3BvaW50TG9jYXRpb25zUmVzcG9uc2U+IHJlc3BvbnNlLCByZXF1ZXN0LmFyZ3VtZW50cywgcmVxdWVzdCk7XG5cblx0XHRcdH0gZWxzZSB7XG5cdFx0XHRcdHRoaXMuY3VzdG9tUmVxdWVzdChyZXF1ZXN0LmNvbW1hbmQsIDxEZWJ1Z1Byb3RvY29sLlJlc3BvbnNlPiByZXNwb25zZSwgcmVxdWVzdC5hcmd1bWVudHMsIHJlcXVlc3QpO1xuXHRcdFx0fVxuXHRcdH0gY2F0Y2ggKGUpIHtcblx0XHRcdHRoaXMuc2VuZEVycm9yUmVzcG9uc2UocmVzcG9uc2UsIDExMDQsICd7X3N0YWNrfScsIHsgX2V4Y2VwdGlvbjogZS5tZXNzYWdlLCBfc3RhY2s6IGUuc3RhY2sgfSwgRXJyb3JEZXN0aW5hdGlvbi5UZWxlbWV0cnkpO1xuXHRcdH1cblx0fVxuXG5cdHByb3RlY3RlZCBpbml0aWFsaXplUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5Jbml0aWFsaXplUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuSW5pdGlhbGl6ZVJlcXVlc3RBcmd1bWVudHMpOiB2b2lkIHtcblxuXHRcdC8vIFRoaXMgZGVmYXVsdCBkZWJ1ZyBhZGFwdGVyIGRvZXMgbm90IHN1cHBvcnQgY29uZGl0aW9uYWwgYnJlYWtwb2ludHMuXG5cdFx0cmVzcG9uc2UuYm9keS5zdXBwb3J0c0NvbmRpdGlvbmFsQnJlYWtwb2ludHMgPSBmYWxzZTtcblxuXHRcdC8vIFRoaXMgZGVmYXVsdCBkZWJ1ZyBhZGFwdGVyIGRvZXMgbm90IHN1cHBvcnQgaGl0IGNvbmRpdGlvbmFsIGJyZWFrcG9pbnRzLlxuXHRcdHJlc3BvbnNlLmJvZHkuc3VwcG9ydHNIaXRDb25kaXRpb25hbEJyZWFrcG9pbnRzID0gZmFsc2U7XG5cblx0XHQvLyBUaGlzIGRlZmF1bHQgZGVidWcgYWRhcHRlciBkb2VzIG5vdCBzdXBwb3J0IGZ1bmN0aW9uIGJyZWFrcG9pbnRzLlxuXHRcdHJlc3BvbnNlLmJvZHkuc3VwcG9ydHNGdW5jdGlvbkJyZWFrcG9pbnRzID0gZmFsc2U7XG5cblx0XHQvLyBUaGlzIGRlZmF1bHQgZGVidWcgYWRhcHRlciBpbXBsZW1lbnRzIHRoZSAnY29uZmlndXJhdGlvbkRvbmUnIHJlcXVlc3QuXG5cdFx0cmVzcG9uc2UuYm9keS5zdXBwb3J0c0NvbmZpZ3VyYXRpb25Eb25lUmVxdWVzdCA9IHRydWU7XG5cblx0XHQvLyBUaGlzIGRlZmF1bHQgZGVidWcgYWRhcHRlciBkb2VzIG5vdCBzdXBwb3J0IGhvdmVycyBiYXNlZCBvbiB0aGUgJ2V2YWx1YXRlJyByZXF1ZXN0LlxuXHRcdHJlc3BvbnNlLmJvZHkuc3VwcG9ydHNFdmFsdWF0ZUZvckhvdmVycyA9IGZhbHNlO1xuXG5cdFx0Ly8gVGhpcyBkZWZhdWx0IGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCB0aGUgJ3N0ZXBCYWNrJyByZXF1ZXN0LlxuXHRcdHJlc3BvbnNlLmJvZHkuc3VwcG9ydHNTdGVwQmFjayA9IGZhbHNlO1xuXG5cdFx0Ly8gVGhpcyBkZWZhdWx0IGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCB0aGUgJ3NldFZhcmlhYmxlJyByZXF1ZXN0LlxuXHRcdHJlc3BvbnNlLmJvZHkuc3VwcG9ydHNTZXRWYXJpYWJsZSA9IGZhbHNlO1xuXG5cdFx0Ly8gVGhpcyBkZWZhdWx0IGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCB0aGUgJ3Jlc3RhcnRGcmFtZScgcmVxdWVzdC5cblx0XHRyZXNwb25zZS5ib2R5LnN1cHBvcnRzUmVzdGFydEZyYW1lID0gZmFsc2U7XG5cblx0XHQvLyBUaGlzIGRlZmF1bHQgZGVidWcgYWRhcHRlciBkb2VzIG5vdCBzdXBwb3J0IHRoZSAnc3RlcEluVGFyZ2V0cycgcmVxdWVzdC5cblx0XHRyZXNwb25zZS5ib2R5LnN1cHBvcnRzU3RlcEluVGFyZ2V0c1JlcXVlc3QgPSBmYWxzZTtcblxuXHRcdC8vIFRoaXMgZGVmYXVsdCBkZWJ1ZyBhZGFwdGVyIGRvZXMgbm90IHN1cHBvcnQgdGhlICdnb3RvVGFyZ2V0cycgcmVxdWVzdC5cblx0XHRyZXNwb25zZS5ib2R5LnN1cHBvcnRzR290b1RhcmdldHNSZXF1ZXN0ID0gZmFsc2U7XG5cblx0XHQvLyBUaGlzIGRlZmF1bHQgZGVidWcgYWRhcHRlciBkb2VzIG5vdCBzdXBwb3J0IHRoZSAnY29tcGxldGlvbnMnIHJlcXVlc3QuXG5cdFx0cmVzcG9uc2UuYm9keS5zdXBwb3J0c0NvbXBsZXRpb25zUmVxdWVzdCA9IGZhbHNlO1xuXG5cdFx0Ly8gVGhpcyBkZWZhdWx0IGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCB0aGUgJ3Jlc3RhcnQnIHJlcXVlc3QuXG5cdFx0cmVzcG9uc2UuYm9keS5zdXBwb3J0c1Jlc3RhcnRSZXF1ZXN0ID0gZmFsc2U7XG5cblx0XHQvLyBUaGlzIGRlZmF1bHQgZGVidWcgYWRhcHRlciBkb2VzIG5vdCBzdXBwb3J0IHRoZSAnZXhjZXB0aW9uT3B0aW9ucycgYXR0cmlidXRlIG9uIHRoZSAnc2V0RXhjZXB0aW9uQnJlYWtwb2ludHMnIHJlcXVlc3QuXG5cdFx0cmVzcG9uc2UuYm9keS5zdXBwb3J0c0V4Y2VwdGlvbk9wdGlvbnMgPSBmYWxzZTtcblxuXHRcdC8vIFRoaXMgZGVmYXVsdCBkZWJ1ZyBhZGFwdGVyIGRvZXMgbm90IHN1cHBvcnQgdGhlICdmb3JtYXQnIGF0dHJpYnV0ZSBvbiB0aGUgJ3ZhcmlhYmxlcycsICdldmFsdWF0ZScsIGFuZCAnc3RhY2tUcmFjZScgcmVxdWVzdC5cblx0XHRyZXNwb25zZS5ib2R5LnN1cHBvcnRzVmFsdWVGb3JtYXR0aW5nT3B0aW9ucyA9IGZhbHNlO1xuXG5cdFx0Ly8gVGhpcyBkZWJ1ZyBhZGFwdGVyIGRvZXMgbm90IHN1cHBvcnQgdGhlICdleGNlcHRpb25JbmZvJyByZXF1ZXN0LlxuXHRcdHJlc3BvbnNlLmJvZHkuc3VwcG9ydHNFeGNlcHRpb25JbmZvUmVxdWVzdCA9IGZhbHNlO1xuXG5cdFx0Ly8gVGhpcyBkZWJ1ZyBhZGFwdGVyIGRvZXMgbm90IHN1cHBvcnQgdGhlICdUZXJtaW5hdGVEZWJ1Z2dlZScgYXR0cmlidXRlIG9uIHRoZSAnZGlzY29ubmVjdCcgcmVxdWVzdC5cblx0XHRyZXNwb25zZS5ib2R5LnN1cHBvcnRUZXJtaW5hdGVEZWJ1Z2dlZSA9IGZhbHNlO1xuXG5cdFx0Ly8gVGhpcyBkZWJ1ZyBhZGFwdGVyIGRvZXMgbm90IHN1cHBvcnQgZGVsYXllZCBsb2FkaW5nIG9mIHN0YWNrIGZyYW1lcy5cblx0XHRyZXNwb25zZS5ib2R5LnN1cHBvcnRzRGVsYXllZFN0YWNrVHJhY2VMb2FkaW5nID0gZmFsc2U7XG5cblx0XHQvLyBUaGlzIGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCB0aGUgJ2xvYWRlZFNvdXJjZXMnIHJlcXVlc3QuXG5cdFx0cmVzcG9uc2UuYm9keS5zdXBwb3J0c0xvYWRlZFNvdXJjZXNSZXF1ZXN0ID0gZmFsc2U7XG5cblx0XHQvLyBUaGlzIGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCB0aGUgJ2xvZ01lc3NhZ2UnIGF0dHJpYnV0ZSBvZiB0aGUgU291cmNlQnJlYWtwb2ludC5cblx0XHRyZXNwb25zZS5ib2R5LnN1cHBvcnRzTG9nUG9pbnRzID0gZmFsc2U7XG5cblx0XHQvLyBUaGlzIGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCB0aGUgJ3Rlcm1pbmF0ZVRocmVhZHMnIHJlcXVlc3QuXG5cdFx0cmVzcG9uc2UuYm9keS5zdXBwb3J0c1Rlcm1pbmF0ZVRocmVhZHNSZXF1ZXN0ID0gZmFsc2U7XG5cblx0XHQvLyBUaGlzIGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCB0aGUgJ3NldEV4cHJlc3Npb24nIHJlcXVlc3QuXG5cdFx0cmVzcG9uc2UuYm9keS5zdXBwb3J0c1NldEV4cHJlc3Npb24gPSBmYWxzZTtcblxuXHRcdC8vIFRoaXMgZGVidWcgYWRhcHRlciBkb2VzIG5vdCBzdXBwb3J0IHRoZSAndGVybWluYXRlJyByZXF1ZXN0LlxuXHRcdHJlc3BvbnNlLmJvZHkuc3VwcG9ydHNUZXJtaW5hdGVSZXF1ZXN0ID0gZmFsc2U7XG5cblx0XHQvLyBUaGlzIGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCBkYXRhIGJyZWFrcG9pbnRzLlxuXHRcdHJlc3BvbnNlLmJvZHkuc3VwcG9ydHNEYXRhQnJlYWtwb2ludHMgPSBmYWxzZTtcblxuXHRcdC8qKiBUaGlzIGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCB0aGUgJ3JlYWRNZW1vcnknIHJlcXVlc3QuICovXG5cdFx0cmVzcG9uc2UuYm9keS5zdXBwb3J0c1JlYWRNZW1vcnlSZXF1ZXN0ID0gZmFsc2U7XG5cblx0XHQvKiogVGhlIGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCB0aGUgJ2Rpc2Fzc2VtYmxlJyByZXF1ZXN0LiAqL1xuXHRcdHJlc3BvbnNlLmJvZHkuc3VwcG9ydHNEaXNhc3NlbWJsZVJlcXVlc3QgPSBmYWxzZTtcblxuXHRcdC8qKiBUaGUgZGVidWcgYWRhcHRlciBkb2VzIG5vdCBzdXBwb3J0IHRoZSAnY2FuY2VsJyByZXF1ZXN0LiAqL1xuXHRcdHJlc3BvbnNlLmJvZHkuc3VwcG9ydHNDYW5jZWxSZXF1ZXN0ID0gZmFsc2U7XG5cblx0XHQvKiogVGhlIGRlYnVnIGFkYXB0ZXIgZG9lcyBub3Qgc3VwcG9ydCB0aGUgJ2JyZWFrcG9pbnRMb2NhdGlvbnMnIHJlcXVlc3QuICovXG5cdFx0cmVzcG9uc2UuYm9keS5zdXBwb3J0c0JyZWFrcG9pbnRMb2NhdGlvbnNSZXF1ZXN0ID0gZmFsc2U7XG5cblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwcm90ZWN0ZWQgZGlzY29ubmVjdFJlcXVlc3QocmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuRGlzY29ubmVjdFJlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLkRpc2Nvbm5lY3RBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpOiB2b2lkIHtcblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdFx0dGhpcy5zaHV0ZG93bigpO1xuXHR9XG5cblx0cHJvdGVjdGVkIGxhdW5jaFJlcXVlc3QocmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuTGF1bmNoUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuTGF1bmNoUmVxdWVzdEFyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBhdHRhY2hSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLkF0dGFjaFJlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLkF0dGFjaFJlcXVlc3RBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpOiB2b2lkIHtcblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwcm90ZWN0ZWQgdGVybWluYXRlUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5UZXJtaW5hdGVSZXNwb25zZSwgYXJnczogRGVidWdQcm90b2NvbC5UZXJtaW5hdGVBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpOiB2b2lkIHtcblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwcm90ZWN0ZWQgcmVzdGFydFJlcXVlc3QocmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuUmVzdGFydFJlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLlJlc3RhcnRBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpOiB2b2lkIHtcblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwcm90ZWN0ZWQgc2V0QnJlYWtQb2ludHNSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLlNldEJyZWFrcG9pbnRzUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuU2V0QnJlYWtwb2ludHNBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpOiB2b2lkIHtcblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwcm90ZWN0ZWQgc2V0RnVuY3Rpb25CcmVha1BvaW50c1JlcXVlc3QocmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuU2V0RnVuY3Rpb25CcmVha3BvaW50c1Jlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLlNldEZ1bmN0aW9uQnJlYWtwb2ludHNBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpOiB2b2lkIHtcblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwcm90ZWN0ZWQgc2V0RXhjZXB0aW9uQnJlYWtQb2ludHNSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLlNldEV4Y2VwdGlvbkJyZWFrcG9pbnRzUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuU2V0RXhjZXB0aW9uQnJlYWtwb2ludHNBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpOiB2b2lkIHtcblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwcm90ZWN0ZWQgY29uZmlndXJhdGlvbkRvbmVSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLkNvbmZpZ3VyYXRpb25Eb25lUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuQ29uZmlndXJhdGlvbkRvbmVBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpOiB2b2lkIHtcblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwcm90ZWN0ZWQgY29udGludWVSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLkNvbnRpbnVlUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuQ29udGludWVBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpIDogdm9pZCB7XG5cdFx0dGhpcy5zZW5kUmVzcG9uc2UocmVzcG9uc2UpO1xuXHR9XG5cblx0cHJvdGVjdGVkIG5leHRSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLk5leHRSZXNwb25zZSwgYXJnczogRGVidWdQcm90b2NvbC5OZXh0QXJndW1lbnRzLCByZXF1ZXN0PzogRGVidWdQcm90b2NvbC5SZXF1ZXN0KSA6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBzdGVwSW5SZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLlN0ZXBJblJlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLlN0ZXBJbkFyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCkgOiB2b2lkIHtcblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwcm90ZWN0ZWQgc3RlcE91dFJlcXVlc3QocmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuU3RlcE91dFJlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLlN0ZXBPdXRBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpIDogdm9pZCB7XG5cdFx0dGhpcy5zZW5kUmVzcG9uc2UocmVzcG9uc2UpO1xuXHR9XG5cblx0cHJvdGVjdGVkIHN0ZXBCYWNrUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5TdGVwQmFja1Jlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLlN0ZXBCYWNrQXJndW1lbnRzLCByZXF1ZXN0PzogRGVidWdQcm90b2NvbC5SZXF1ZXN0KSA6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCByZXZlcnNlQ29udGludWVSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLlJldmVyc2VDb250aW51ZVJlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLlJldmVyc2VDb250aW51ZUFyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCkgOiB2b2lkIHtcblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwcm90ZWN0ZWQgcmVzdGFydEZyYW1lUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5SZXN0YXJ0RnJhbWVSZXNwb25zZSwgYXJnczogRGVidWdQcm90b2NvbC5SZXN0YXJ0RnJhbWVBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpIDogdm9pZCB7XG5cdFx0dGhpcy5zZW5kUmVzcG9uc2UocmVzcG9uc2UpO1xuXHR9XG5cblx0cHJvdGVjdGVkIGdvdG9SZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLkdvdG9SZXNwb25zZSwgYXJnczogRGVidWdQcm90b2NvbC5Hb3RvQXJndW1lbnRzLCByZXF1ZXN0PzogRGVidWdQcm90b2NvbC5SZXF1ZXN0KSA6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBwYXVzZVJlcXVlc3QocmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuUGF1c2VSZXNwb25zZSwgYXJnczogRGVidWdQcm90b2NvbC5QYXVzZUFyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCkgOiB2b2lkIHtcblx0XHR0aGlzLnNlbmRSZXNwb25zZShyZXNwb25zZSk7XG5cdH1cblxuXHRwcm90ZWN0ZWQgc291cmNlUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5Tb3VyY2VSZXNwb25zZSwgYXJnczogRGVidWdQcm90b2NvbC5Tb3VyY2VBcmd1bWVudHMsIHJlcXVlc3Q/OiBEZWJ1Z1Byb3RvY29sLlJlcXVlc3QpIDogdm9pZCB7XG5cdFx0dGhpcy5zZW5kUmVzcG9uc2UocmVzcG9uc2UpO1xuXHR9XG5cblx0cHJvdGVjdGVkIHRocmVhZHNSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLlRocmVhZHNSZXNwb25zZSwgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCB0ZXJtaW5hdGVUaHJlYWRzUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5UZXJtaW5hdGVUaHJlYWRzUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuVGVybWluYXRlVGhyZWFkc0FyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBzdGFja1RyYWNlUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5TdGFja1RyYWNlUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuU3RhY2tUcmFjZUFyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBzY29wZXNSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLlNjb3Blc1Jlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLlNjb3Blc0FyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCB2YXJpYWJsZXNSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLlZhcmlhYmxlc1Jlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLlZhcmlhYmxlc0FyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBzZXRWYXJpYWJsZVJlcXVlc3QocmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuU2V0VmFyaWFibGVSZXNwb25zZSwgYXJnczogRGVidWdQcm90b2NvbC5TZXRWYXJpYWJsZUFyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBzZXRFeHByZXNzaW9uUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5TZXRFeHByZXNzaW9uUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuU2V0RXhwcmVzc2lvbkFyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBldmFsdWF0ZVJlcXVlc3QocmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuRXZhbHVhdGVSZXNwb25zZSwgYXJnczogRGVidWdQcm90b2NvbC5FdmFsdWF0ZUFyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBzdGVwSW5UYXJnZXRzUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5TdGVwSW5UYXJnZXRzUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuU3RlcEluVGFyZ2V0c0FyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBnb3RvVGFyZ2V0c1JlcXVlc3QocmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuR290b1RhcmdldHNSZXNwb25zZSwgYXJnczogRGVidWdQcm90b2NvbC5Hb3RvVGFyZ2V0c0FyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBjb21wbGV0aW9uc1JlcXVlc3QocmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuQ29tcGxldGlvbnNSZXNwb25zZSwgYXJnczogRGVidWdQcm90b2NvbC5Db21wbGV0aW9uc0FyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBleGNlcHRpb25JbmZvUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5FeGNlcHRpb25JbmZvUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuRXhjZXB0aW9uSW5mb0FyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBsb2FkZWRTb3VyY2VzUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5Mb2FkZWRTb3VyY2VzUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuTG9hZGVkU291cmNlc0FyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBkYXRhQnJlYWtwb2ludEluZm9SZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLkRhdGFCcmVha3BvaW50SW5mb1Jlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLkRhdGFCcmVha3BvaW50SW5mb0FyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBzZXREYXRhQnJlYWtwb2ludHNSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLlNldERhdGFCcmVha3BvaW50c1Jlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLlNldERhdGFCcmVha3BvaW50c0FyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCByZWFkTWVtb3J5UmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5SZWFkTWVtb3J5UmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuUmVhZE1lbW9yeUFyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBkaXNhc3NlbWJsZVJlcXVlc3QocmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuRGlzYXNzZW1ibGVSZXNwb25zZSwgYXJnczogRGVidWdQcm90b2NvbC5EaXNhc3NlbWJsZUFyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBjYW5jZWxSZXF1ZXN0KHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLkNhbmNlbFJlc3BvbnNlLCBhcmdzOiBEZWJ1Z1Byb3RvY29sLkNhbmNlbEFyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBicmVha3BvaW50TG9jYXRpb25zUmVxdWVzdChyZXNwb25zZTogRGVidWdQcm90b2NvbC5CcmVha3BvaW50TG9jYXRpb25zUmVzcG9uc2UsIGFyZ3M6IERlYnVnUHJvdG9jb2wuQnJlYWtwb2ludExvY2F0aW9uc0FyZ3VtZW50cywgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdC8qKlxuXHQgKiBPdmVycmlkZSB0aGlzIGhvb2sgdG8gaW1wbGVtZW50IGN1c3RvbSByZXF1ZXN0cy5cblx0ICovXG5cdHByb3RlY3RlZCBjdXN0b21SZXF1ZXN0KGNvbW1hbmQ6IHN0cmluZywgcmVzcG9uc2U6IERlYnVnUHJvdG9jb2wuUmVzcG9uc2UsIGFyZ3M6IGFueSwgcmVxdWVzdD86IERlYnVnUHJvdG9jb2wuUmVxdWVzdCk6IHZvaWQge1xuXHRcdHRoaXMuc2VuZEVycm9yUmVzcG9uc2UocmVzcG9uc2UsIDEwMTQsICd1bnJlY29nbml6ZWQgcmVxdWVzdCcsIG51bGwsIEVycm9yRGVzdGluYXRpb24uVGVsZW1ldHJ5KTtcblx0fVxuXG5cdC8vLS0tLSBwcm90ZWN0ZWQgLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLVxuXG5cdHByb3RlY3RlZCBjb252ZXJ0Q2xpZW50TGluZVRvRGVidWdnZXIobGluZTogbnVtYmVyKTogbnVtYmVyIHtcblx0XHRpZiAodGhpcy5fZGVidWdnZXJMaW5lc1N0YXJ0QXQxKSB7XG5cdFx0XHRyZXR1cm4gdGhpcy5fY2xpZW50TGluZXNTdGFydEF0MSA/IGxpbmUgOiBsaW5lICsgMTtcblx0XHR9XG5cdFx0cmV0dXJuIHRoaXMuX2NsaWVudExpbmVzU3RhcnRBdDEgPyBsaW5lIC0gMSA6IGxpbmU7XG5cdH1cblxuXHRwcm90ZWN0ZWQgY29udmVydERlYnVnZ2VyTGluZVRvQ2xpZW50KGxpbmU6IG51bWJlcik6IG51bWJlciB7XG5cdFx0aWYgKHRoaXMuX2RlYnVnZ2VyTGluZXNTdGFydEF0MSkge1xuXHRcdFx0cmV0dXJuIHRoaXMuX2NsaWVudExpbmVzU3RhcnRBdDEgPyBsaW5lIDogbGluZSAtIDE7XG5cdFx0fVxuXHRcdHJldHVybiB0aGlzLl9jbGllbnRMaW5lc1N0YXJ0QXQxID8gbGluZSArIDEgOiBsaW5lO1xuXHR9XG5cblx0cHJvdGVjdGVkIGNvbnZlcnRDbGllbnRDb2x1bW5Ub0RlYnVnZ2VyKGNvbHVtbjogbnVtYmVyKTogbnVtYmVyIHtcblx0XHRpZiAodGhpcy5fZGVidWdnZXJDb2x1bW5zU3RhcnRBdDEpIHtcblx0XHRcdHJldHVybiB0aGlzLl9jbGllbnRDb2x1bW5zU3RhcnRBdDEgPyBjb2x1bW4gOiBjb2x1bW4gKyAxO1xuXHRcdH1cblx0XHRyZXR1cm4gdGhpcy5fY2xpZW50Q29sdW1uc1N0YXJ0QXQxID8gY29sdW1uIC0gMSA6IGNvbHVtbjtcblx0fVxuXG5cdHByb3RlY3RlZCBjb252ZXJ0RGVidWdnZXJDb2x1bW5Ub0NsaWVudChjb2x1bW46IG51bWJlcik6IG51bWJlciB7XG5cdFx0aWYgKHRoaXMuX2RlYnVnZ2VyQ29sdW1uc1N0YXJ0QXQxKSB7XG5cdFx0XHRyZXR1cm4gdGhpcy5fY2xpZW50Q29sdW1uc1N0YXJ0QXQxID8gY29sdW1uIDogY29sdW1uIC0gMTtcblx0XHR9XG5cdFx0cmV0dXJuIHRoaXMuX2NsaWVudENvbHVtbnNTdGFydEF0MSA/IGNvbHVtbiArIDEgOiBjb2x1bW47XG5cdH1cblxuXHRwcm90ZWN0ZWQgY29udmVydENsaWVudFBhdGhUb0RlYnVnZ2VyKGNsaWVudFBhdGg6IHN0cmluZyk6IHN0cmluZyB7XG5cdFx0aWYgKHRoaXMuX2NsaWVudFBhdGhzQXJlVVJJcyAhPT0gdGhpcy5fZGVidWdnZXJQYXRoc0FyZVVSSXMpIHtcblx0XHRcdGlmICh0aGlzLl9jbGllbnRQYXRoc0FyZVVSSXMpIHtcblx0XHRcdFx0cmV0dXJuIERlYnVnU2Vzc2lvbi51cmkycGF0aChjbGllbnRQYXRoKTtcblx0XHRcdH0gZWxzZSB7XG5cdFx0XHRcdHJldHVybiBEZWJ1Z1Nlc3Npb24ucGF0aDJ1cmkoY2xpZW50UGF0aCk7XG5cdFx0XHR9XG5cdFx0fVxuXHRcdHJldHVybiBjbGllbnRQYXRoO1xuXHR9XG5cblx0cHJvdGVjdGVkIGNvbnZlcnREZWJ1Z2dlclBhdGhUb0NsaWVudChkZWJ1Z2dlclBhdGg6IHN0cmluZyk6IHN0cmluZyB7XG5cdFx0aWYgKHRoaXMuX2RlYnVnZ2VyUGF0aHNBcmVVUklzICE9PSB0aGlzLl9jbGllbnRQYXRoc0FyZVVSSXMpIHtcblx0XHRcdGlmICh0aGlzLl9kZWJ1Z2dlclBhdGhzQXJlVVJJcykge1xuXHRcdFx0XHRyZXR1cm4gRGVidWdTZXNzaW9uLnVyaTJwYXRoKGRlYnVnZ2VyUGF0aCk7XG5cdFx0XHR9IGVsc2Uge1xuXHRcdFx0XHRyZXR1cm4gRGVidWdTZXNzaW9uLnBhdGgydXJpKGRlYnVnZ2VyUGF0aCk7XG5cdFx0XHR9XG5cdFx0fVxuXHRcdHJldHVybiBkZWJ1Z2dlclBhdGg7XG5cdH1cblxuXHQvLy0tLS0gcHJpdmF0ZSAtLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tXG5cblx0cHJpdmF0ZSBzdGF0aWMgcGF0aDJ1cmkocGF0aDogc3RyaW5nKTogc3RyaW5nIHtcblxuXHRcdGlmIChwcm9jZXNzLnBsYXRmb3JtID09PSAnd2luMzInKSB7XG5cdFx0XHRpZiAoL15bQS1aXTovLnRlc3QocGF0aCkpIHtcblx0XHRcdFx0cGF0aCA9IHBhdGhbMF0udG9Mb3dlckNhc2UoKSArIHBhdGguc3Vic3RyKDEpO1xuXHRcdFx0fVxuXHRcdFx0cGF0aCA9IHBhdGgucmVwbGFjZSgvXFxcXC9nLCAnLycpO1xuXHRcdH1cblx0XHRwYXRoID0gZW5jb2RlVVJJKHBhdGgpO1xuXG5cdFx0bGV0IHVyaSA9IG5ldyBVUkwoYGZpbGU6YCk7XHQvLyBpZ25vcmUgJ3BhdGgnIGZvciBub3dcblx0XHR1cmkucGF0aG5hbWUgPSBwYXRoO1x0Ly8gbm93IHVzZSAncGF0aCcgdG8gZ2V0IHRoZSBjb3JyZWN0IHBlcmNlbnQgZW5jb2RpbmcgKHNlZSBodHRwczovL3VybC5zcGVjLndoYXR3Zy5vcmcpXG5cdFx0cmV0dXJuIHVyaS50b1N0cmluZygpO1xuXHR9XG5cblx0cHJpdmF0ZSBzdGF0aWMgdXJpMnBhdGgoc291cmNlVXJpOiBzdHJpbmcpOiBzdHJpbmcge1xuXG5cdFx0bGV0IHVyaSA9IG5ldyBVUkwoc291cmNlVXJpKTtcblx0XHRsZXQgcyA9IGRlY29kZVVSSUNvbXBvbmVudCh1cmkucGF0aG5hbWUpO1xuXHRcdGlmIChwcm9jZXNzLnBsYXRmb3JtID09PSAnd2luMzInKSB7XG5cdFx0XHRpZiAoL15cXC9bYS16QS1aXTovLnRlc3QocykpIHtcblx0XHRcdFx0cyA9IHNbMV0udG9Mb3dlckNhc2UoKSArIHMuc3Vic3RyKDIpO1xuXHRcdFx0fVxuXHRcdFx0cyA9IHMucmVwbGFjZSgvXFwvL2csICdcXFxcJyk7XG5cdFx0fVxuXHRcdHJldHVybiBzO1xuXHR9XG5cblx0cHJpdmF0ZSBzdGF0aWMgX2Zvcm1hdFBJSVJlZ2V4cCA9IC97KFtefV0rKX0vZztcblxuXHQvKlxuXHQqIElmIGFyZ3VtZW50IHN0YXJ0cyB3aXRoICdfJyBpdCBpcyBPSyB0byBzZW5kIGl0cyB2YWx1ZSB0byB0ZWxlbWV0cnkuXG5cdCovXG5cdHByaXZhdGUgc3RhdGljIGZvcm1hdFBJSShmb3JtYXQ6c3RyaW5nLCBleGNsdWRlUElJOiBib29sZWFuLCBhcmdzOiB7W2tleTogc3RyaW5nXTogc3RyaW5nfSk6IHN0cmluZyB7XG5cdFx0cmV0dXJuIGZvcm1hdC5yZXBsYWNlKERlYnVnU2Vzc2lvbi5fZm9ybWF0UElJUmVnZXhwLCBmdW5jdGlvbihtYXRjaCwgcGFyYW1OYW1lKSB7XG5cdFx0XHRpZiAoZXhjbHVkZVBJSSAmJiBwYXJhbU5hbWUubGVuZ3RoID4gMCAmJiBwYXJhbU5hbWVbMF0gIT09ICdfJykge1xuXHRcdFx0XHRyZXR1cm4gbWF0Y2g7XG5cdFx0XHR9XG5cdFx0XHRyZXR1cm4gYXJnc1twYXJhbU5hbWVdICYmIGFyZ3MuaGFzT3duUHJvcGVydHkocGFyYW1OYW1lKSA/XG5cdFx0XHRcdGFyZ3NbcGFyYW1OYW1lXSA6XG5cdFx0XHRcdG1hdGNoO1xuXHRcdH0pXG5cdH1cbn1cbiJdfQ==