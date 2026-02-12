"use strict";
var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __commonJS = (cb, mod) => function __require() {
  return mod || (0, cb[__getOwnPropNames(cb)[0]])((mod = { exports: {} }).exports, mod), mod.exports;
};
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

// node_modules/@vscode/debugadapter/lib/messages.js
var require_messages = __commonJS({
  "node_modules/@vscode/debugadapter/lib/messages.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    exports2.Event = exports2.Response = exports2.Message = void 0;
    var Message = class {
      constructor(type) {
        this.seq = 0;
        this.type = type;
      }
    };
    exports2.Message = Message;
    var Response = class extends Message {
      constructor(request, message) {
        super("response");
        this.request_seq = request.seq;
        this.command = request.command;
        if (message) {
          this.success = false;
          this.message = message;
        } else {
          this.success = true;
        }
      }
    };
    exports2.Response = Response;
    var Event = class extends Message {
      constructor(event, body) {
        super("event");
        this.event = event;
        if (body) {
          this.body = body;
        }
      }
    };
    exports2.Event = Event;
  }
});

// node_modules/@vscode/debugadapter/lib/protocol.js
var require_protocol = __commonJS({
  "node_modules/@vscode/debugadapter/lib/protocol.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    exports2.ProtocolServer = void 0;
    var ee = require("events");
    var messages_1 = require_messages();
    var Emitter = class {
      get event() {
        if (!this._event) {
          this._event = (listener, thisArg) => {
            this._listener = listener;
            this._this = thisArg;
            let result;
            result = {
              dispose: () => {
                this._listener = void 0;
                this._this = void 0;
              }
            };
            return result;
          };
        }
        return this._event;
      }
      fire(event) {
        if (this._listener) {
          try {
            this._listener.call(this._this, event);
          } catch (e) {
          }
        }
      }
      hasListener() {
        return !!this._listener;
      }
      dispose() {
        this._listener = void 0;
        this._this = void 0;
      }
    };
    var ProtocolServer = class _ProtocolServer extends ee.EventEmitter {
      constructor() {
        super();
        this._sendMessage = new Emitter();
        this._sequence = 1;
        this._pendingRequests = /* @__PURE__ */ new Map();
        this.onDidSendMessage = this._sendMessage.event;
      }
      // ---- implements vscode.Debugadapter interface ---------------------------
      dispose() {
      }
      handleMessage(msg) {
        if (msg.type === "request") {
          this.dispatchRequest(msg);
        } else if (msg.type === "response") {
          const response = msg;
          const clb = this._pendingRequests.get(response.request_seq);
          if (clb) {
            this._pendingRequests.delete(response.request_seq);
            clb(response);
          }
        }
      }
      _isRunningInline() {
        return this._sendMessage && this._sendMessage.hasListener();
      }
      //--------------------------------------------------------------------------
      start(inStream, outStream) {
        this._writableStream = outStream;
        this._rawData = Buffer.alloc(0);
        inStream.on("data", (data) => this._handleData(data));
        inStream.on("close", () => {
          this._emitEvent(new messages_1.Event("close"));
        });
        inStream.on("error", (error) => {
          this._emitEvent(new messages_1.Event("error", "inStream error: " + (error && error.message)));
        });
        outStream.on("error", (error) => {
          this._emitEvent(new messages_1.Event("error", "outStream error: " + (error && error.message)));
        });
        inStream.resume();
      }
      stop() {
        if (this._writableStream) {
          this._writableStream.end();
        }
      }
      sendEvent(event) {
        this._send("event", event);
      }
      sendResponse(response) {
        if (response.seq > 0) {
          console.error(`attempt to send more than one response for command ${response.command}`);
        } else {
          this._send("response", response);
        }
      }
      sendRequest(command, args, timeout2, cb) {
        const request = {
          command
        };
        if (args && Object.keys(args).length > 0) {
          request.arguments = args;
        }
        this._send("request", request);
        if (cb) {
          this._pendingRequests.set(request.seq, cb);
          const timer = setTimeout(() => {
            clearTimeout(timer);
            const clb = this._pendingRequests.get(request.seq);
            if (clb) {
              this._pendingRequests.delete(request.seq);
              clb(new messages_1.Response(request, "timeout"));
            }
          }, timeout2);
        }
      }
      // ---- protected ----------------------------------------------------------
      dispatchRequest(request) {
      }
      // ---- private ------------------------------------------------------------
      _emitEvent(event) {
        this.emit(event.event, event);
      }
      _send(typ, message) {
        message.type = typ;
        message.seq = this._sequence++;
        if (this._writableStream) {
          const json = JSON.stringify(message);
          this._writableStream.write(`Content-Length: ${Buffer.byteLength(json, "utf8")}\r
\r
${json}`, "utf8");
        }
        this._sendMessage.fire(message);
      }
      _handleData(data) {
        this._rawData = Buffer.concat([this._rawData, data]);
        while (true) {
          if (this._contentLength >= 0) {
            if (this._rawData.length >= this._contentLength) {
              const message = this._rawData.toString("utf8", 0, this._contentLength);
              this._rawData = this._rawData.slice(this._contentLength);
              this._contentLength = -1;
              if (message.length > 0) {
                try {
                  let msg = JSON.parse(message);
                  this.handleMessage(msg);
                } catch (e) {
                  this._emitEvent(new messages_1.Event("error", "Error handling data: " + (e && e.message)));
                }
              }
              continue;
            }
          } else {
            const idx = this._rawData.indexOf(_ProtocolServer.TWO_CRLF);
            if (idx !== -1) {
              const header = this._rawData.toString("utf8", 0, idx);
              const lines = header.split("\r\n");
              for (let i = 0; i < lines.length; i++) {
                const pair = lines[i].split(/: +/);
                if (pair[0] == "Content-Length") {
                  this._contentLength = +pair[1];
                }
              }
              this._rawData = this._rawData.slice(idx + _ProtocolServer.TWO_CRLF.length);
              continue;
            }
          }
          break;
        }
      }
    };
    exports2.ProtocolServer = ProtocolServer;
    ProtocolServer.TWO_CRLF = "\r\n\r\n";
  }
});

// node_modules/@vscode/debugadapter/lib/runDebugAdapter.js
var require_runDebugAdapter = __commonJS({
  "node_modules/@vscode/debugadapter/lib/runDebugAdapter.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    exports2.runDebugAdapter = void 0;
    var Net2 = require("net");
    function runDebugAdapter(debugSession) {
      let port = 0;
      const args = process.argv.slice(2);
      args.forEach(function(val, index, array) {
        const portMatch = /^--server=(\d{4,5})$/.exec(val);
        if (portMatch) {
          port = parseInt(portMatch[1], 10);
        }
      });
      if (port > 0) {
        console.error(`waiting for debug protocol on port ${port}`);
        Net2.createServer((socket) => {
          console.error(">> accepted connection from client");
          socket.on("end", () => {
            console.error(">> client connection closed\n");
          });
          const session = new debugSession(false, true);
          session.setRunAsServer(true);
          session.start(socket, socket);
        }).listen(port);
      } else {
        const session = new debugSession(false);
        process.on("SIGTERM", () => {
          session.shutdown();
        });
        session.start(process.stdin, process.stdout);
      }
    }
    exports2.runDebugAdapter = runDebugAdapter;
  }
});

// node_modules/@vscode/debugadapter/lib/debugSession.js
var require_debugSession = __commonJS({
  "node_modules/@vscode/debugadapter/lib/debugSession.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    exports2.DebugSession = exports2.ErrorDestination = exports2.MemoryEvent = exports2.InvalidatedEvent = exports2.ProgressEndEvent = exports2.ProgressUpdateEvent = exports2.ProgressStartEvent = exports2.CapabilitiesEvent = exports2.LoadedSourceEvent = exports2.ModuleEvent = exports2.BreakpointEvent = exports2.ThreadEvent = exports2.OutputEvent = exports2.ExitedEvent = exports2.TerminatedEvent = exports2.InitializedEvent = exports2.ContinuedEvent = exports2.StoppedEvent = exports2.CompletionItem = exports2.Module = exports2.Breakpoint = exports2.Variable = exports2.Thread = exports2.StackFrame = exports2.Scope = exports2.Source = void 0;
    var protocol_1 = require_protocol();
    var messages_1 = require_messages();
    var runDebugAdapter_1 = require_runDebugAdapter();
    var url_1 = require("url");
    var Source2 = class {
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
    };
    exports2.Source = Source2;
    var Scope2 = class {
      constructor(name, reference, expensive = false) {
        this.name = name;
        this.variablesReference = reference;
        this.expensive = expensive;
      }
    };
    exports2.Scope = Scope2;
    var StackFrame2 = class {
      constructor(i, nm, src, ln = 0, col = 0) {
        this.id = i;
        this.source = src;
        this.line = ln;
        this.column = col;
        this.name = nm;
      }
    };
    exports2.StackFrame = StackFrame2;
    var Thread2 = class {
      constructor(id, name) {
        this.id = id;
        if (name) {
          this.name = name;
        } else {
          this.name = "Thread #" + id;
        }
      }
    };
    exports2.Thread = Thread2;
    var Variable = class {
      constructor(name, value, ref = 0, indexedVariables, namedVariables) {
        this.name = name;
        this.value = value;
        this.variablesReference = ref;
        if (typeof namedVariables === "number") {
          this.namedVariables = namedVariables;
        }
        if (typeof indexedVariables === "number") {
          this.indexedVariables = indexedVariables;
        }
      }
    };
    exports2.Variable = Variable;
    var Breakpoint2 = class {
      constructor(verified, line, column, source) {
        this.verified = verified;
        const e = this;
        if (typeof line === "number") {
          e.line = line;
        }
        if (typeof column === "number") {
          e.column = column;
        }
        if (source) {
          e.source = source;
        }
      }
      setId(id) {
        this.id = id;
      }
    };
    exports2.Breakpoint = Breakpoint2;
    var Module = class {
      constructor(id, name) {
        this.id = id;
        this.name = name;
      }
    };
    exports2.Module = Module;
    var CompletionItem = class {
      constructor(label, start, length = 0) {
        this.label = label;
        this.start = start;
        this.length = length;
      }
    };
    exports2.CompletionItem = CompletionItem;
    var StoppedEvent2 = class extends messages_1.Event {
      constructor(reason, threadId, exceptionText) {
        super("stopped");
        this.body = {
          reason
        };
        if (typeof threadId === "number") {
          this.body.threadId = threadId;
        }
        if (typeof exceptionText === "string") {
          this.body.text = exceptionText;
        }
      }
    };
    exports2.StoppedEvent = StoppedEvent2;
    var ContinuedEvent = class extends messages_1.Event {
      constructor(threadId, allThreadsContinued) {
        super("continued");
        this.body = {
          threadId
        };
        if (typeof allThreadsContinued === "boolean") {
          this.body.allThreadsContinued = allThreadsContinued;
        }
      }
    };
    exports2.ContinuedEvent = ContinuedEvent;
    var InitializedEvent2 = class extends messages_1.Event {
      constructor() {
        super("initialized");
      }
    };
    exports2.InitializedEvent = InitializedEvent2;
    var TerminatedEvent2 = class extends messages_1.Event {
      constructor(restart) {
        super("terminated");
        if (typeof restart === "boolean" || restart) {
          const e = this;
          e.body = {
            restart
          };
        }
      }
    };
    exports2.TerminatedEvent = TerminatedEvent2;
    var ExitedEvent = class extends messages_1.Event {
      constructor(exitCode) {
        super("exited");
        this.body = {
          exitCode
        };
      }
    };
    exports2.ExitedEvent = ExitedEvent;
    var OutputEvent2 = class extends messages_1.Event {
      constructor(output, category = "console", data) {
        super("output");
        this.body = {
          category,
          output
        };
        if (data !== void 0) {
          this.body.data = data;
        }
      }
    };
    exports2.OutputEvent = OutputEvent2;
    var ThreadEvent = class extends messages_1.Event {
      constructor(reason, threadId) {
        super("thread");
        this.body = {
          reason,
          threadId
        };
      }
    };
    exports2.ThreadEvent = ThreadEvent;
    var BreakpointEvent2 = class extends messages_1.Event {
      constructor(reason, breakpoint) {
        super("breakpoint");
        this.body = {
          reason,
          breakpoint
        };
      }
    };
    exports2.BreakpointEvent = BreakpointEvent2;
    var ModuleEvent = class extends messages_1.Event {
      constructor(reason, module3) {
        super("module");
        this.body = {
          reason,
          module: module3
        };
      }
    };
    exports2.ModuleEvent = ModuleEvent;
    var LoadedSourceEvent = class extends messages_1.Event {
      constructor(reason, source) {
        super("loadedSource");
        this.body = {
          reason,
          source
        };
      }
    };
    exports2.LoadedSourceEvent = LoadedSourceEvent;
    var CapabilitiesEvent = class extends messages_1.Event {
      constructor(capabilities) {
        super("capabilities");
        this.body = {
          capabilities
        };
      }
    };
    exports2.CapabilitiesEvent = CapabilitiesEvent;
    var ProgressStartEvent2 = class extends messages_1.Event {
      constructor(progressId, title, message) {
        super("progressStart");
        this.body = {
          progressId,
          title
        };
        if (typeof message === "string") {
          this.body.message = message;
        }
      }
    };
    exports2.ProgressStartEvent = ProgressStartEvent2;
    var ProgressUpdateEvent2 = class extends messages_1.Event {
      constructor(progressId, message) {
        super("progressUpdate");
        this.body = {
          progressId
        };
        if (typeof message === "string") {
          this.body.message = message;
        }
      }
    };
    exports2.ProgressUpdateEvent = ProgressUpdateEvent2;
    var ProgressEndEvent2 = class extends messages_1.Event {
      constructor(progressId, message) {
        super("progressEnd");
        this.body = {
          progressId
        };
        if (typeof message === "string") {
          this.body.message = message;
        }
      }
    };
    exports2.ProgressEndEvent = ProgressEndEvent2;
    var InvalidatedEvent2 = class extends messages_1.Event {
      constructor(areas, threadId, stackFrameId) {
        super("invalidated");
        this.body = {};
        if (areas) {
          this.body.areas = areas;
        }
        if (threadId) {
          this.body.threadId = threadId;
        }
        if (stackFrameId) {
          this.body.stackFrameId = stackFrameId;
        }
      }
    };
    exports2.InvalidatedEvent = InvalidatedEvent2;
    var MemoryEvent2 = class extends messages_1.Event {
      constructor(memoryReference, offset, count) {
        super("memory");
        this.body = { memoryReference, offset, count };
      }
    };
    exports2.MemoryEvent = MemoryEvent2;
    var ErrorDestination;
    (function(ErrorDestination2) {
      ErrorDestination2[ErrorDestination2["User"] = 1] = "User";
      ErrorDestination2[ErrorDestination2["Telemetry"] = 2] = "Telemetry";
    })(ErrorDestination = exports2.ErrorDestination || (exports2.ErrorDestination = {}));
    var DebugSession = class _DebugSession extends protocol_1.ProtocolServer {
      constructor(obsolete_debuggerLinesAndColumnsStartAt1, obsolete_isServer) {
        super();
        const linesAndColumnsStartAt1 = typeof obsolete_debuggerLinesAndColumnsStartAt1 === "boolean" ? obsolete_debuggerLinesAndColumnsStartAt1 : false;
        this._debuggerLinesStartAt1 = linesAndColumnsStartAt1;
        this._debuggerColumnsStartAt1 = linesAndColumnsStartAt1;
        this._debuggerPathsAreURIs = false;
        this._clientLinesStartAt1 = true;
        this._clientColumnsStartAt1 = true;
        this._clientPathsAreURIs = false;
        this._isServer = typeof obsolete_isServer === "boolean" ? obsolete_isServer : false;
        this.on("close", () => {
          this.shutdown();
        });
        this.on("error", (error) => {
          this.shutdown();
        });
      }
      setDebuggerPathFormat(format) {
        this._debuggerPathsAreURIs = format !== "path";
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
        (0, runDebugAdapter_1.runDebugAdapter)(debugSession);
      }
      shutdown() {
        if (this._isServer || this._isRunningInline()) {
        } else {
          setTimeout(() => {
            process.exit(0);
          }, 100);
        }
      }
      sendErrorResponse(response, codeOrMessage, format, variables, dest = ErrorDestination.User) {
        let msg;
        if (typeof codeOrMessage === "number") {
          msg = {
            id: codeOrMessage,
            format
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
        } else {
          msg = codeOrMessage;
        }
        response.success = false;
        response.message = _DebugSession.formatPII(msg.format, true, msg.variables);
        if (!response.body) {
          response.body = {};
        }
        response.body.error = msg;
        this.sendResponse(response);
      }
      runInTerminalRequest(args, timeout2, cb) {
        this.sendRequest("runInTerminal", args, timeout2, cb);
      }
      dispatchRequest(request) {
        const response = new messages_1.Response(request);
        try {
          if (request.command === "initialize") {
            var args = request.arguments;
            if (typeof args.linesStartAt1 === "boolean") {
              this._clientLinesStartAt1 = args.linesStartAt1;
            }
            if (typeof args.columnsStartAt1 === "boolean") {
              this._clientColumnsStartAt1 = args.columnsStartAt1;
            }
            if (args.pathFormat !== "path") {
              this.sendErrorResponse(response, 2018, "debug adapter only supports native paths", null, ErrorDestination.Telemetry);
            } else {
              const initializeResponse = response;
              initializeResponse.body = {};
              this.initializeRequest(initializeResponse, args);
            }
          } else if (request.command === "launch") {
            this.launchRequest(response, request.arguments, request);
          } else if (request.command === "attach") {
            this.attachRequest(response, request.arguments, request);
          } else if (request.command === "disconnect") {
            this.disconnectRequest(response, request.arguments, request);
          } else if (request.command === "terminate") {
            this.terminateRequest(response, request.arguments, request);
          } else if (request.command === "restart") {
            this.restartRequest(response, request.arguments, request);
          } else if (request.command === "setBreakpoints") {
            this.setBreakPointsRequest(response, request.arguments, request);
          } else if (request.command === "setFunctionBreakpoints") {
            this.setFunctionBreakPointsRequest(response, request.arguments, request);
          } else if (request.command === "setExceptionBreakpoints") {
            this.setExceptionBreakPointsRequest(response, request.arguments, request);
          } else if (request.command === "configurationDone") {
            this.configurationDoneRequest(response, request.arguments, request);
          } else if (request.command === "continue") {
            this.continueRequest(response, request.arguments, request);
          } else if (request.command === "next") {
            this.nextRequest(response, request.arguments, request);
          } else if (request.command === "stepIn") {
            this.stepInRequest(response, request.arguments, request);
          } else if (request.command === "stepOut") {
            this.stepOutRequest(response, request.arguments, request);
          } else if (request.command === "stepBack") {
            this.stepBackRequest(response, request.arguments, request);
          } else if (request.command === "reverseContinue") {
            this.reverseContinueRequest(response, request.arguments, request);
          } else if (request.command === "restartFrame") {
            this.restartFrameRequest(response, request.arguments, request);
          } else if (request.command === "goto") {
            this.gotoRequest(response, request.arguments, request);
          } else if (request.command === "pause") {
            this.pauseRequest(response, request.arguments, request);
          } else if (request.command === "stackTrace") {
            this.stackTraceRequest(response, request.arguments, request);
          } else if (request.command === "scopes") {
            this.scopesRequest(response, request.arguments, request);
          } else if (request.command === "variables") {
            this.variablesRequest(response, request.arguments, request);
          } else if (request.command === "setVariable") {
            this.setVariableRequest(response, request.arguments, request);
          } else if (request.command === "setExpression") {
            this.setExpressionRequest(response, request.arguments, request);
          } else if (request.command === "source") {
            this.sourceRequest(response, request.arguments, request);
          } else if (request.command === "threads") {
            this.threadsRequest(response, request);
          } else if (request.command === "terminateThreads") {
            this.terminateThreadsRequest(response, request.arguments, request);
          } else if (request.command === "evaluate") {
            this.evaluateRequest(response, request.arguments, request);
          } else if (request.command === "stepInTargets") {
            this.stepInTargetsRequest(response, request.arguments, request);
          } else if (request.command === "gotoTargets") {
            this.gotoTargetsRequest(response, request.arguments, request);
          } else if (request.command === "completions") {
            this.completionsRequest(response, request.arguments, request);
          } else if (request.command === "exceptionInfo") {
            this.exceptionInfoRequest(response, request.arguments, request);
          } else if (request.command === "loadedSources") {
            this.loadedSourcesRequest(response, request.arguments, request);
          } else if (request.command === "dataBreakpointInfo") {
            this.dataBreakpointInfoRequest(response, request.arguments, request);
          } else if (request.command === "setDataBreakpoints") {
            this.setDataBreakpointsRequest(response, request.arguments, request);
          } else if (request.command === "readMemory") {
            this.readMemoryRequest(response, request.arguments, request);
          } else if (request.command === "writeMemory") {
            this.writeMemoryRequest(response, request.arguments, request);
          } else if (request.command === "disassemble") {
            this.disassembleRequest(response, request.arguments, request);
          } else if (request.command === "cancel") {
            this.cancelRequest(response, request.arguments, request);
          } else if (request.command === "breakpointLocations") {
            this.breakpointLocationsRequest(response, request.arguments, request);
          } else if (request.command === "setInstructionBreakpoints") {
            this.setInstructionBreakpointsRequest(response, request.arguments, request);
          } else {
            this.customRequest(request.command, response, request.arguments, request);
          }
        } catch (e) {
          this.sendErrorResponse(response, 1104, "{_stack}", { _exception: e.message, _stack: e.stack }, ErrorDestination.Telemetry);
        }
      }
      initializeRequest(response, args) {
        response.body.supportsConditionalBreakpoints = false;
        response.body.supportsHitConditionalBreakpoints = false;
        response.body.supportsFunctionBreakpoints = false;
        response.body.supportsConfigurationDoneRequest = true;
        response.body.supportsEvaluateForHovers = false;
        response.body.supportsStepBack = false;
        response.body.supportsSetVariable = false;
        response.body.supportsRestartFrame = false;
        response.body.supportsStepInTargetsRequest = false;
        response.body.supportsGotoTargetsRequest = false;
        response.body.supportsCompletionsRequest = false;
        response.body.supportsRestartRequest = false;
        response.body.supportsExceptionOptions = false;
        response.body.supportsValueFormattingOptions = false;
        response.body.supportsExceptionInfoRequest = false;
        response.body.supportTerminateDebuggee = false;
        response.body.supportsDelayedStackTraceLoading = false;
        response.body.supportsLoadedSourcesRequest = false;
        response.body.supportsLogPoints = false;
        response.body.supportsTerminateThreadsRequest = false;
        response.body.supportsSetExpression = false;
        response.body.supportsTerminateRequest = false;
        response.body.supportsDataBreakpoints = false;
        response.body.supportsReadMemoryRequest = false;
        response.body.supportsDisassembleRequest = false;
        response.body.supportsCancelRequest = false;
        response.body.supportsBreakpointLocationsRequest = false;
        response.body.supportsClipboardContext = false;
        response.body.supportsSteppingGranularity = false;
        response.body.supportsInstructionBreakpoints = false;
        response.body.supportsExceptionFilterOptions = false;
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
      writeMemoryRequest(response, args, request) {
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
      setInstructionBreakpointsRequest(response, args, request) {
        this.sendResponse(response);
      }
      /**
       * Override this hook to implement custom requests.
       */
      customRequest(command, response, args, request) {
        this.sendErrorResponse(response, 1014, "unrecognized request", null, ErrorDestination.Telemetry);
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
            return _DebugSession.uri2path(clientPath);
          } else {
            return _DebugSession.path2uri(clientPath);
          }
        }
        return clientPath;
      }
      convertDebuggerPathToClient(debuggerPath) {
        if (this._debuggerPathsAreURIs !== this._clientPathsAreURIs) {
          if (this._debuggerPathsAreURIs) {
            return _DebugSession.uri2path(debuggerPath);
          } else {
            return _DebugSession.path2uri(debuggerPath);
          }
        }
        return debuggerPath;
      }
      //---- private -------------------------------------------------------------------------------
      static path2uri(path) {
        if (process.platform === "win32") {
          if (/^[A-Z]:/.test(path)) {
            path = path[0].toLowerCase() + path.substr(1);
          }
          path = path.replace(/\\/g, "/");
        }
        path = encodeURI(path);
        let uri = new url_1.URL(`file:`);
        uri.pathname = path;
        return uri.toString();
      }
      static uri2path(sourceUri) {
        let uri = new url_1.URL(sourceUri);
        let s = decodeURIComponent(uri.pathname);
        if (process.platform === "win32") {
          if (/^\/[a-zA-Z]:/.test(s)) {
            s = s[1].toLowerCase() + s.substr(2);
          }
          s = s.replace(/\//g, "\\");
        }
        return s;
      }
      /*
      * If argument starts with '_' it is OK to send its value to telemetry.
      */
      static formatPII(format, excludePII, args) {
        return format.replace(_DebugSession._formatPIIRegexp, function(match, paramName) {
          if (excludePII && paramName.length > 0 && paramName[0] !== "_") {
            return match;
          }
          return args[paramName] && args.hasOwnProperty(paramName) ? args[paramName] : match;
        });
      }
    };
    exports2.DebugSession = DebugSession;
    DebugSession._formatPIIRegexp = /{([^}]+)}/g;
  }
});

// node_modules/@vscode/debugadapter/lib/internalLogger.js
var require_internalLogger = __commonJS({
  "node_modules/@vscode/debugadapter/lib/internalLogger.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    exports2.InternalLogger = void 0;
    var fs = require("fs");
    var path = require("path");
    var logger_1 = require_logger();
    var InternalLogger = class {
      constructor(logCallback, isServer) {
        this.beforeExitCallback = () => this.dispose();
        this._logCallback = logCallback;
        this._logToConsole = isServer;
        this._minLogLevel = logger_1.LogLevel.Warn;
        this.disposeCallback = (signal, code) => {
          this.dispose();
          code = code || 2;
          code += 128;
          process.exit(code);
        };
      }
      async setup(options) {
        this._minLogLevel = options.consoleMinLogLevel;
        this._prependTimestamp = options.prependTimestamp;
        if (options.logFilePath) {
          if (!path.isAbsolute(options.logFilePath)) {
            this.log(`logFilePath must be an absolute path: ${options.logFilePath}`, logger_1.LogLevel.Error);
          } else {
            const handleError = (err) => this.sendLog(`Error creating log file at path: ${options.logFilePath}. Error: ${err.toString()}
`, logger_1.LogLevel.Error);
            try {
              await fs.promises.mkdir(path.dirname(options.logFilePath), { recursive: true });
              this.log(`Verbose logs are written to:
`, logger_1.LogLevel.Warn);
              this.log(options.logFilePath + "\n", logger_1.LogLevel.Warn);
              this._logFileStream = fs.createWriteStream(options.logFilePath);
              this.logDateTime();
              this.setupShutdownListeners();
              this._logFileStream.on("error", (err) => {
                handleError(err);
              });
            } catch (err) {
              handleError(err);
            }
          }
        }
      }
      logDateTime() {
        let d = /* @__PURE__ */ new Date();
        let dateString = d.getUTCFullYear() + `-${d.getUTCMonth() + 1}-` + d.getUTCDate();
        const timeAndDateStamp = dateString + ", " + getFormattedTimeString();
        this.log(timeAndDateStamp + "\n", logger_1.LogLevel.Verbose, false);
      }
      setupShutdownListeners() {
        process.on("beforeExit", this.beforeExitCallback);
        process.on("SIGTERM", this.disposeCallback);
        process.on("SIGINT", this.disposeCallback);
      }
      removeShutdownListeners() {
        process.removeListener("beforeExit", this.beforeExitCallback);
        process.removeListener("SIGTERM", this.disposeCallback);
        process.removeListener("SIGINT", this.disposeCallback);
      }
      dispose() {
        return new Promise((resolve) => {
          this.removeShutdownListeners();
          if (this._logFileStream) {
            this._logFileStream.end(resolve);
            this._logFileStream = null;
          } else {
            resolve();
          }
        });
      }
      log(msg, level, prependTimestamp = true) {
        if (this._minLogLevel === logger_1.LogLevel.Stop) {
          return;
        }
        if (level >= this._minLogLevel) {
          this.sendLog(msg, level);
        }
        if (this._logToConsole) {
          const logFn = level === logger_1.LogLevel.Error ? console.error : level === logger_1.LogLevel.Warn ? console.warn : null;
          if (logFn) {
            logFn((0, logger_1.trimLastNewline)(msg));
          }
        }
        if (level === logger_1.LogLevel.Error) {
          msg = `[${logger_1.LogLevel[level]}] ${msg}`;
        }
        if (this._prependTimestamp && prependTimestamp) {
          msg = "[" + getFormattedTimeString() + "] " + msg;
        }
        if (this._logFileStream) {
          this._logFileStream.write(msg);
        }
      }
      sendLog(msg, level) {
        if (msg.length > 1500) {
          const endsInNewline = !!msg.match(/(\n|\r\n)$/);
          msg = msg.substr(0, 1500) + "[...]";
          if (endsInNewline) {
            msg = msg + "\n";
          }
        }
        if (this._logCallback) {
          const event = new logger_1.LogOutputEvent(msg, level);
          this._logCallback(event);
        }
      }
    };
    exports2.InternalLogger = InternalLogger;
    function getFormattedTimeString() {
      let d = /* @__PURE__ */ new Date();
      let hourString = _padZeroes(2, String(d.getUTCHours()));
      let minuteString = _padZeroes(2, String(d.getUTCMinutes()));
      let secondString = _padZeroes(2, String(d.getUTCSeconds()));
      let millisecondString = _padZeroes(3, String(d.getUTCMilliseconds()));
      return hourString + ":" + minuteString + ":" + secondString + "." + millisecondString + " UTC";
    }
    function _padZeroes(minDesiredLength, numberToPad) {
      if (numberToPad.length >= minDesiredLength) {
        return numberToPad;
      } else {
        return String("0".repeat(minDesiredLength) + numberToPad).slice(-minDesiredLength);
      }
    }
  }
});

// node_modules/@vscode/debugadapter/lib/logger.js
var require_logger = __commonJS({
  "node_modules/@vscode/debugadapter/lib/logger.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    exports2.trimLastNewline = exports2.LogOutputEvent = exports2.logger = exports2.Logger = exports2.LogLevel = void 0;
    var internalLogger_1 = require_internalLogger();
    var debugSession_1 = require_debugSession();
    var LogLevel;
    (function(LogLevel2) {
      LogLevel2[LogLevel2["Verbose"] = 0] = "Verbose";
      LogLevel2[LogLevel2["Log"] = 1] = "Log";
      LogLevel2[LogLevel2["Warn"] = 2] = "Warn";
      LogLevel2[LogLevel2["Error"] = 3] = "Error";
      LogLevel2[LogLevel2["Stop"] = 4] = "Stop";
    })(LogLevel = exports2.LogLevel || (exports2.LogLevel = {}));
    var Logger2 = class {
      constructor() {
        this._pendingLogQ = [];
      }
      log(msg, level = LogLevel.Log) {
        msg = msg + "\n";
        this._write(msg, level);
      }
      verbose(msg) {
        this.log(msg, LogLevel.Verbose);
      }
      warn(msg) {
        this.log(msg, LogLevel.Warn);
      }
      error(msg) {
        this.log(msg, LogLevel.Error);
      }
      dispose() {
        if (this._currentLogger) {
          const disposeP = this._currentLogger.dispose();
          this._currentLogger = null;
          return disposeP;
        } else {
          return Promise.resolve();
        }
      }
      /**
       * `log` adds a newline, `write` doesn't
       */
      _write(msg, level = LogLevel.Log) {
        msg = msg + "";
        if (this._pendingLogQ) {
          this._pendingLogQ.push({ msg, level });
        } else if (this._currentLogger) {
          this._currentLogger.log(msg, level);
        }
      }
      /**
       * Set the logger's minimum level to log in the console, and whether to log to the file. Log messages are queued before this is
       * called the first time, because minLogLevel defaults to Warn.
       */
      setup(consoleMinLogLevel, _logFilePath, prependTimestamp = true) {
        const logFilePath = typeof _logFilePath === "string" ? _logFilePath : _logFilePath && this._logFilePathFromInit;
        if (this._currentLogger) {
          const options = {
            consoleMinLogLevel,
            logFilePath,
            prependTimestamp
          };
          this._currentLogger.setup(options).then(() => {
            if (this._pendingLogQ) {
              const logQ = this._pendingLogQ;
              this._pendingLogQ = null;
              logQ.forEach((item) => this._write(item.msg, item.level));
            }
          });
        }
      }
      init(logCallback, logFilePath, logToConsole) {
        this._pendingLogQ = this._pendingLogQ || [];
        this._currentLogger = new internalLogger_1.InternalLogger(logCallback, logToConsole);
        this._logFilePathFromInit = logFilePath;
      }
    };
    exports2.Logger = Logger2;
    exports2.logger = new Logger2();
    var LogOutputEvent = class extends debugSession_1.OutputEvent {
      constructor(msg, level) {
        const category = level === LogLevel.Error ? "stderr" : level === LogLevel.Warn ? "console" : "stdout";
        super(msg, category);
      }
    };
    exports2.LogOutputEvent = LogOutputEvent;
    function trimLastNewline(str) {
      return str.replace(/(\n|\r\n)$/, "");
    }
    exports2.trimLastNewline = trimLastNewline;
  }
});

// node_modules/@vscode/debugadapter/lib/loggingDebugSession.js
var require_loggingDebugSession = __commonJS({
  "node_modules/@vscode/debugadapter/lib/loggingDebugSession.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    exports2.LoggingDebugSession = void 0;
    var Logger2 = require_logger();
    var logger2 = Logger2.logger;
    var debugSession_1 = require_debugSession();
    var LoggingDebugSession2 = class extends debugSession_1.DebugSession {
      constructor(obsolete_logFilePath, obsolete_debuggerLinesAndColumnsStartAt1, obsolete_isServer) {
        super(obsolete_debuggerLinesAndColumnsStartAt1, obsolete_isServer);
        this.obsolete_logFilePath = obsolete_logFilePath;
        this.on("error", (event) => {
          logger2.error(event.body);
        });
      }
      start(inStream, outStream) {
        super.start(inStream, outStream);
        logger2.init((e) => this.sendEvent(e), this.obsolete_logFilePath, this._isServer);
      }
      /**
       * Overload sendEvent to log
       */
      sendEvent(event) {
        if (!(event instanceof Logger2.LogOutputEvent)) {
          let objectToLog = event;
          if (event instanceof debugSession_1.OutputEvent && event.body && event.body.data && event.body.data.doNotLogOutput) {
            delete event.body.data.doNotLogOutput;
            objectToLog = { ...event };
            objectToLog.body = { ...event.body, output: "<output not logged>" };
          }
          logger2.verbose(`To client: ${JSON.stringify(objectToLog)}`);
        }
        super.sendEvent(event);
      }
      /**
       * Overload sendRequest to log
       */
      sendRequest(command, args, timeout2, cb) {
        logger2.verbose(`To client: ${JSON.stringify(command)}(${JSON.stringify(args)}), timeout: ${timeout2}`);
        super.sendRequest(command, args, timeout2, cb);
      }
      /**
       * Overload sendResponse to log
       */
      sendResponse(response) {
        logger2.verbose(`To client: ${JSON.stringify(response)}`);
        super.sendResponse(response);
      }
      dispatchRequest(request) {
        logger2.verbose(`From client: ${request.command}(${JSON.stringify(request.arguments)})`);
        super.dispatchRequest(request);
      }
    };
    exports2.LoggingDebugSession = LoggingDebugSession2;
  }
});

// node_modules/@vscode/debugadapter/lib/handles.js
var require_handles = __commonJS({
  "node_modules/@vscode/debugadapter/lib/handles.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    exports2.Handles = void 0;
    var Handles2 = class {
      constructor(startHandle) {
        this.START_HANDLE = 1e3;
        this._handleMap = /* @__PURE__ */ new Map();
        this._nextHandle = typeof startHandle === "number" ? startHandle : this.START_HANDLE;
      }
      reset() {
        this._nextHandle = this.START_HANDLE;
        this._handleMap = /* @__PURE__ */ new Map();
      }
      create(value) {
        var handle = this._nextHandle++;
        this._handleMap.set(handle, value);
        return handle;
      }
      get(handle, dflt) {
        return this._handleMap.get(handle) || dflt;
      }
    };
    exports2.Handles = Handles2;
  }
});

// node_modules/@vscode/debugadapter/lib/main.js
var require_main = __commonJS({
  "node_modules/@vscode/debugadapter/lib/main.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    exports2.Handles = exports2.Response = exports2.Event = exports2.ErrorDestination = exports2.CompletionItem = exports2.Module = exports2.Source = exports2.Breakpoint = exports2.Variable = exports2.Scope = exports2.StackFrame = exports2.Thread = exports2.MemoryEvent = exports2.InvalidatedEvent = exports2.ProgressEndEvent = exports2.ProgressUpdateEvent = exports2.ProgressStartEvent = exports2.CapabilitiesEvent = exports2.LoadedSourceEvent = exports2.ModuleEvent = exports2.BreakpointEvent = exports2.ThreadEvent = exports2.OutputEvent = exports2.ContinuedEvent = exports2.StoppedEvent = exports2.ExitedEvent = exports2.TerminatedEvent = exports2.InitializedEvent = exports2.logger = exports2.Logger = exports2.LoggingDebugSession = exports2.DebugSession = void 0;
    var debugSession_1 = require_debugSession();
    Object.defineProperty(exports2, "DebugSession", { enumerable: true, get: function() {
      return debugSession_1.DebugSession;
    } });
    Object.defineProperty(exports2, "InitializedEvent", { enumerable: true, get: function() {
      return debugSession_1.InitializedEvent;
    } });
    Object.defineProperty(exports2, "TerminatedEvent", { enumerable: true, get: function() {
      return debugSession_1.TerminatedEvent;
    } });
    Object.defineProperty(exports2, "ExitedEvent", { enumerable: true, get: function() {
      return debugSession_1.ExitedEvent;
    } });
    Object.defineProperty(exports2, "StoppedEvent", { enumerable: true, get: function() {
      return debugSession_1.StoppedEvent;
    } });
    Object.defineProperty(exports2, "ContinuedEvent", { enumerable: true, get: function() {
      return debugSession_1.ContinuedEvent;
    } });
    Object.defineProperty(exports2, "OutputEvent", { enumerable: true, get: function() {
      return debugSession_1.OutputEvent;
    } });
    Object.defineProperty(exports2, "ThreadEvent", { enumerable: true, get: function() {
      return debugSession_1.ThreadEvent;
    } });
    Object.defineProperty(exports2, "BreakpointEvent", { enumerable: true, get: function() {
      return debugSession_1.BreakpointEvent;
    } });
    Object.defineProperty(exports2, "ModuleEvent", { enumerable: true, get: function() {
      return debugSession_1.ModuleEvent;
    } });
    Object.defineProperty(exports2, "LoadedSourceEvent", { enumerable: true, get: function() {
      return debugSession_1.LoadedSourceEvent;
    } });
    Object.defineProperty(exports2, "CapabilitiesEvent", { enumerable: true, get: function() {
      return debugSession_1.CapabilitiesEvent;
    } });
    Object.defineProperty(exports2, "ProgressStartEvent", { enumerable: true, get: function() {
      return debugSession_1.ProgressStartEvent;
    } });
    Object.defineProperty(exports2, "ProgressUpdateEvent", { enumerable: true, get: function() {
      return debugSession_1.ProgressUpdateEvent;
    } });
    Object.defineProperty(exports2, "ProgressEndEvent", { enumerable: true, get: function() {
      return debugSession_1.ProgressEndEvent;
    } });
    Object.defineProperty(exports2, "InvalidatedEvent", { enumerable: true, get: function() {
      return debugSession_1.InvalidatedEvent;
    } });
    Object.defineProperty(exports2, "MemoryEvent", { enumerable: true, get: function() {
      return debugSession_1.MemoryEvent;
    } });
    Object.defineProperty(exports2, "Thread", { enumerable: true, get: function() {
      return debugSession_1.Thread;
    } });
    Object.defineProperty(exports2, "StackFrame", { enumerable: true, get: function() {
      return debugSession_1.StackFrame;
    } });
    Object.defineProperty(exports2, "Scope", { enumerable: true, get: function() {
      return debugSession_1.Scope;
    } });
    Object.defineProperty(exports2, "Variable", { enumerable: true, get: function() {
      return debugSession_1.Variable;
    } });
    Object.defineProperty(exports2, "Breakpoint", { enumerable: true, get: function() {
      return debugSession_1.Breakpoint;
    } });
    Object.defineProperty(exports2, "Source", { enumerable: true, get: function() {
      return debugSession_1.Source;
    } });
    Object.defineProperty(exports2, "Module", { enumerable: true, get: function() {
      return debugSession_1.Module;
    } });
    Object.defineProperty(exports2, "CompletionItem", { enumerable: true, get: function() {
      return debugSession_1.CompletionItem;
    } });
    Object.defineProperty(exports2, "ErrorDestination", { enumerable: true, get: function() {
      return debugSession_1.ErrorDestination;
    } });
    var loggingDebugSession_1 = require_loggingDebugSession();
    Object.defineProperty(exports2, "LoggingDebugSession", { enumerable: true, get: function() {
      return loggingDebugSession_1.LoggingDebugSession;
    } });
    var Logger2 = require_logger();
    exports2.Logger = Logger2;
    var messages_1 = require_messages();
    Object.defineProperty(exports2, "Event", { enumerable: true, get: function() {
      return messages_1.Event;
    } });
    Object.defineProperty(exports2, "Response", { enumerable: true, get: function() {
      return messages_1.Response;
    } });
    var handles_1 = require_handles();
    Object.defineProperty(exports2, "Handles", { enumerable: true, get: function() {
      return handles_1.Handles;
    } });
    var logger2 = Logger2.logger;
    exports2.logger = logger2;
  }
});

// node_modules/path-browserify/index.js
var require_path_browserify = __commonJS({
  "node_modules/path-browserify/index.js"(exports2, module2) {
    "use strict";
    function assertPath(path) {
      if (typeof path !== "string") {
        throw new TypeError("Path must be a string. Received " + JSON.stringify(path));
      }
    }
    function normalizeStringPosix(path, allowAboveRoot) {
      var res = "";
      var lastSegmentLength = 0;
      var lastSlash = -1;
      var dots = 0;
      var code;
      for (var i = 0; i <= path.length; ++i) {
        if (i < path.length)
          code = path.charCodeAt(i);
        else if (code === 47)
          break;
        else
          code = 47;
        if (code === 47) {
          if (lastSlash === i - 1 || dots === 1) {
          } else if (lastSlash !== i - 1 && dots === 2) {
            if (res.length < 2 || lastSegmentLength !== 2 || res.charCodeAt(res.length - 1) !== 46 || res.charCodeAt(res.length - 2) !== 46) {
              if (res.length > 2) {
                var lastSlashIndex = res.lastIndexOf("/");
                if (lastSlashIndex !== res.length - 1) {
                  if (lastSlashIndex === -1) {
                    res = "";
                    lastSegmentLength = 0;
                  } else {
                    res = res.slice(0, lastSlashIndex);
                    lastSegmentLength = res.length - 1 - res.lastIndexOf("/");
                  }
                  lastSlash = i;
                  dots = 0;
                  continue;
                }
              } else if (res.length === 2 || res.length === 1) {
                res = "";
                lastSegmentLength = 0;
                lastSlash = i;
                dots = 0;
                continue;
              }
            }
            if (allowAboveRoot) {
              if (res.length > 0)
                res += "/..";
              else
                res = "..";
              lastSegmentLength = 2;
            }
          } else {
            if (res.length > 0)
              res += "/" + path.slice(lastSlash + 1, i);
            else
              res = path.slice(lastSlash + 1, i);
            lastSegmentLength = i - lastSlash - 1;
          }
          lastSlash = i;
          dots = 0;
        } else if (code === 46 && dots !== -1) {
          ++dots;
        } else {
          dots = -1;
        }
      }
      return res;
    }
    function _format(sep, pathObject) {
      var dir = pathObject.dir || pathObject.root;
      var base = pathObject.base || (pathObject.name || "") + (pathObject.ext || "");
      if (!dir) {
        return base;
      }
      if (dir === pathObject.root) {
        return dir + base;
      }
      return dir + sep + base;
    }
    var posix = {
      // path.resolve([from ...], to)
      resolve: function resolve() {
        var resolvedPath = "";
        var resolvedAbsolute = false;
        var cwd;
        for (var i = arguments.length - 1; i >= -1 && !resolvedAbsolute; i--) {
          var path;
          if (i >= 0)
            path = arguments[i];
          else {
            if (cwd === void 0)
              cwd = process.cwd();
            path = cwd;
          }
          assertPath(path);
          if (path.length === 0) {
            continue;
          }
          resolvedPath = path + "/" + resolvedPath;
          resolvedAbsolute = path.charCodeAt(0) === 47;
        }
        resolvedPath = normalizeStringPosix(resolvedPath, !resolvedAbsolute);
        if (resolvedAbsolute) {
          if (resolvedPath.length > 0)
            return "/" + resolvedPath;
          else
            return "/";
        } else if (resolvedPath.length > 0) {
          return resolvedPath;
        } else {
          return ".";
        }
      },
      normalize: function normalize(path) {
        assertPath(path);
        if (path.length === 0) return ".";
        var isAbsolute = path.charCodeAt(0) === 47;
        var trailingSeparator = path.charCodeAt(path.length - 1) === 47;
        path = normalizeStringPosix(path, !isAbsolute);
        if (path.length === 0 && !isAbsolute) path = ".";
        if (path.length > 0 && trailingSeparator) path += "/";
        if (isAbsolute) return "/" + path;
        return path;
      },
      isAbsolute: function isAbsolute(path) {
        assertPath(path);
        return path.length > 0 && path.charCodeAt(0) === 47;
      },
      join: function join2() {
        if (arguments.length === 0)
          return ".";
        var joined;
        for (var i = 0; i < arguments.length; ++i) {
          var arg = arguments[i];
          assertPath(arg);
          if (arg.length > 0) {
            if (joined === void 0)
              joined = arg;
            else
              joined += "/" + arg;
          }
        }
        if (joined === void 0)
          return ".";
        return posix.normalize(joined);
      },
      relative: function relative(from, to) {
        assertPath(from);
        assertPath(to);
        if (from === to) return "";
        from = posix.resolve(from);
        to = posix.resolve(to);
        if (from === to) return "";
        var fromStart = 1;
        for (; fromStart < from.length; ++fromStart) {
          if (from.charCodeAt(fromStart) !== 47)
            break;
        }
        var fromEnd = from.length;
        var fromLen = fromEnd - fromStart;
        var toStart = 1;
        for (; toStart < to.length; ++toStart) {
          if (to.charCodeAt(toStart) !== 47)
            break;
        }
        var toEnd = to.length;
        var toLen = toEnd - toStart;
        var length = fromLen < toLen ? fromLen : toLen;
        var lastCommonSep = -1;
        var i = 0;
        for (; i <= length; ++i) {
          if (i === length) {
            if (toLen > length) {
              if (to.charCodeAt(toStart + i) === 47) {
                return to.slice(toStart + i + 1);
              } else if (i === 0) {
                return to.slice(toStart + i);
              }
            } else if (fromLen > length) {
              if (from.charCodeAt(fromStart + i) === 47) {
                lastCommonSep = i;
              } else if (i === 0) {
                lastCommonSep = 0;
              }
            }
            break;
          }
          var fromCode = from.charCodeAt(fromStart + i);
          var toCode = to.charCodeAt(toStart + i);
          if (fromCode !== toCode)
            break;
          else if (fromCode === 47)
            lastCommonSep = i;
        }
        var out = "";
        for (i = fromStart + lastCommonSep + 1; i <= fromEnd; ++i) {
          if (i === fromEnd || from.charCodeAt(i) === 47) {
            if (out.length === 0)
              out += "..";
            else
              out += "/..";
          }
        }
        if (out.length > 0)
          return out + to.slice(toStart + lastCommonSep);
        else {
          toStart += lastCommonSep;
          if (to.charCodeAt(toStart) === 47)
            ++toStart;
          return to.slice(toStart);
        }
      },
      _makeLong: function _makeLong(path) {
        return path;
      },
      dirname: function dirname(path) {
        assertPath(path);
        if (path.length === 0) return ".";
        var code = path.charCodeAt(0);
        var hasRoot = code === 47;
        var end = -1;
        var matchedSlash = true;
        for (var i = path.length - 1; i >= 1; --i) {
          code = path.charCodeAt(i);
          if (code === 47) {
            if (!matchedSlash) {
              end = i;
              break;
            }
          } else {
            matchedSlash = false;
          }
        }
        if (end === -1) return hasRoot ? "/" : ".";
        if (hasRoot && end === 1) return "//";
        return path.slice(0, end);
      },
      basename: function basename2(path, ext) {
        if (ext !== void 0 && typeof ext !== "string") throw new TypeError('"ext" argument must be a string');
        assertPath(path);
        var start = 0;
        var end = -1;
        var matchedSlash = true;
        var i;
        if (ext !== void 0 && ext.length > 0 && ext.length <= path.length) {
          if (ext.length === path.length && ext === path) return "";
          var extIdx = ext.length - 1;
          var firstNonSlashEnd = -1;
          for (i = path.length - 1; i >= 0; --i) {
            var code = path.charCodeAt(i);
            if (code === 47) {
              if (!matchedSlash) {
                start = i + 1;
                break;
              }
            } else {
              if (firstNonSlashEnd === -1) {
                matchedSlash = false;
                firstNonSlashEnd = i + 1;
              }
              if (extIdx >= 0) {
                if (code === ext.charCodeAt(extIdx)) {
                  if (--extIdx === -1) {
                    end = i;
                  }
                } else {
                  extIdx = -1;
                  end = firstNonSlashEnd;
                }
              }
            }
          }
          if (start === end) end = firstNonSlashEnd;
          else if (end === -1) end = path.length;
          return path.slice(start, end);
        } else {
          for (i = path.length - 1; i >= 0; --i) {
            if (path.charCodeAt(i) === 47) {
              if (!matchedSlash) {
                start = i + 1;
                break;
              }
            } else if (end === -1) {
              matchedSlash = false;
              end = i + 1;
            }
          }
          if (end === -1) return "";
          return path.slice(start, end);
        }
      },
      extname: function extname(path) {
        assertPath(path);
        var startDot = -1;
        var startPart = 0;
        var end = -1;
        var matchedSlash = true;
        var preDotState = 0;
        for (var i = path.length - 1; i >= 0; --i) {
          var code = path.charCodeAt(i);
          if (code === 47) {
            if (!matchedSlash) {
              startPart = i + 1;
              break;
            }
            continue;
          }
          if (end === -1) {
            matchedSlash = false;
            end = i + 1;
          }
          if (code === 46) {
            if (startDot === -1)
              startDot = i;
            else if (preDotState !== 1)
              preDotState = 1;
          } else if (startDot !== -1) {
            preDotState = -1;
          }
        }
        if (startDot === -1 || end === -1 || // We saw a non-dot character immediately before the dot
        preDotState === 0 || // The (right-most) trimmed path component is exactly '..'
        preDotState === 1 && startDot === end - 1 && startDot === startPart + 1) {
          return "";
        }
        return path.slice(startDot, end);
      },
      format: function format(pathObject) {
        if (pathObject === null || typeof pathObject !== "object") {
          throw new TypeError('The "pathObject" argument must be of type Object. Received type ' + typeof pathObject);
        }
        return _format("/", pathObject);
      },
      parse: function parse(path) {
        assertPath(path);
        var ret = { root: "", dir: "", base: "", ext: "", name: "" };
        if (path.length === 0) return ret;
        var code = path.charCodeAt(0);
        var isAbsolute = code === 47;
        var start;
        if (isAbsolute) {
          ret.root = "/";
          start = 1;
        } else {
          start = 0;
        }
        var startDot = -1;
        var startPart = 0;
        var end = -1;
        var matchedSlash = true;
        var i = path.length - 1;
        var preDotState = 0;
        for (; i >= start; --i) {
          code = path.charCodeAt(i);
          if (code === 47) {
            if (!matchedSlash) {
              startPart = i + 1;
              break;
            }
            continue;
          }
          if (end === -1) {
            matchedSlash = false;
            end = i + 1;
          }
          if (code === 46) {
            if (startDot === -1) startDot = i;
            else if (preDotState !== 1) preDotState = 1;
          } else if (startDot !== -1) {
            preDotState = -1;
          }
        }
        if (startDot === -1 || end === -1 || // We saw a non-dot character immediately before the dot
        preDotState === 0 || // The (right-most) trimmed path component is exactly '..'
        preDotState === 1 && startDot === end - 1 && startDot === startPart + 1) {
          if (end !== -1) {
            if (startPart === 0 && isAbsolute) ret.base = ret.name = path.slice(1, end);
            else ret.base = ret.name = path.slice(startPart, end);
          }
        } else {
          if (startPart === 0 && isAbsolute) {
            ret.name = path.slice(1, startDot);
            ret.base = path.slice(1, end);
          } else {
            ret.name = path.slice(startPart, startDot);
            ret.base = path.slice(startPart, end);
          }
          ret.ext = path.slice(startDot, end);
        }
        if (startPart > 0) ret.dir = path.slice(0, startPart - 1);
        else if (isAbsolute) ret.dir = "/";
        return ret;
      },
      sep: "/",
      delimiter: ":",
      win32: null,
      posix: null
    };
    posix.posix = posix;
    module2.exports = posix;
  }
});

// node_modules/await-notify/index.js
var require_await_notify = __commonJS({
  "node_modules/await-notify/index.js"(exports2) {
    "use strict";
    function Subject2() {
      this.waiters = [];
    }
    Subject2.prototype.wait = function(timeout2) {
      var self = this;
      var waiter = {};
      this.waiters.push(waiter);
      var promise = new Promise(function(resolve) {
        var resolved = false;
        waiter.resolve = function(noRemove) {
          if (resolved) {
            return;
          }
          resolved = true;
          if (waiter.timeout) {
            clearTimeout(waiter.timeout);
            waiter.timeout = null;
          }
          if (!noRemove) {
            var pos = self.waiters.indexOf(waiter);
            if (pos > -1) {
              self.waiters.splice(pos, 1);
            }
          }
          resolve();
        };
      });
      if (timeout2 > 0 && isFinite(timeout2)) {
        waiter.timeout = setTimeout(function() {
          waiter.timeout = null;
          waiter.resolve();
        }, timeout2);
      }
      return promise;
    };
    Subject2.prototype.notify = function() {
      if (this.waiters.length > 0) {
        this.waiters.pop().resolve(true);
      }
    };
    Subject2.prototype.notifyAll = function() {
      for (var i = this.waiters.length - 1; i >= 0; i--) {
        this.waiters[i].resolve(true);
      }
      this.waiters = [];
    };
    exports2.Subject = Subject2;
  }
});

// node_modules/base64-js/index.js
var require_base64_js = __commonJS({
  "node_modules/base64-js/index.js"(exports2) {
    "use strict";
    exports2.byteLength = byteLength;
    exports2.toByteArray = toByteArray2;
    exports2.fromByteArray = fromByteArray2;
    var lookup = [];
    var revLookup = [];
    var Arr = typeof Uint8Array !== "undefined" ? Uint8Array : Array;
    var code = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (i = 0, len = code.length; i < len; ++i) {
      lookup[i] = code[i];
      revLookup[code.charCodeAt(i)] = i;
    }
    var i;
    var len;
    revLookup["-".charCodeAt(0)] = 62;
    revLookup["_".charCodeAt(0)] = 63;
    function getLens(b64) {
      var len2 = b64.length;
      if (len2 % 4 > 0) {
        throw new Error("Invalid string. Length must be a multiple of 4");
      }
      var validLen = b64.indexOf("=");
      if (validLen === -1) validLen = len2;
      var placeHoldersLen = validLen === len2 ? 0 : 4 - validLen % 4;
      return [validLen, placeHoldersLen];
    }
    function byteLength(b64) {
      var lens = getLens(b64);
      var validLen = lens[0];
      var placeHoldersLen = lens[1];
      return (validLen + placeHoldersLen) * 3 / 4 - placeHoldersLen;
    }
    function _byteLength(b64, validLen, placeHoldersLen) {
      return (validLen + placeHoldersLen) * 3 / 4 - placeHoldersLen;
    }
    function toByteArray2(b64) {
      var tmp;
      var lens = getLens(b64);
      var validLen = lens[0];
      var placeHoldersLen = lens[1];
      var arr = new Arr(_byteLength(b64, validLen, placeHoldersLen));
      var curByte = 0;
      var len2 = placeHoldersLen > 0 ? validLen - 4 : validLen;
      var i2;
      for (i2 = 0; i2 < len2; i2 += 4) {
        tmp = revLookup[b64.charCodeAt(i2)] << 18 | revLookup[b64.charCodeAt(i2 + 1)] << 12 | revLookup[b64.charCodeAt(i2 + 2)] << 6 | revLookup[b64.charCodeAt(i2 + 3)];
        arr[curByte++] = tmp >> 16 & 255;
        arr[curByte++] = tmp >> 8 & 255;
        arr[curByte++] = tmp & 255;
      }
      if (placeHoldersLen === 2) {
        tmp = revLookup[b64.charCodeAt(i2)] << 2 | revLookup[b64.charCodeAt(i2 + 1)] >> 4;
        arr[curByte++] = tmp & 255;
      }
      if (placeHoldersLen === 1) {
        tmp = revLookup[b64.charCodeAt(i2)] << 10 | revLookup[b64.charCodeAt(i2 + 1)] << 4 | revLookup[b64.charCodeAt(i2 + 2)] >> 2;
        arr[curByte++] = tmp >> 8 & 255;
        arr[curByte++] = tmp & 255;
      }
      return arr;
    }
    function tripletToBase64(num) {
      return lookup[num >> 18 & 63] + lookup[num >> 12 & 63] + lookup[num >> 6 & 63] + lookup[num & 63];
    }
    function encodeChunk(uint8, start, end) {
      var tmp;
      var output = [];
      for (var i2 = start; i2 < end; i2 += 3) {
        tmp = (uint8[i2] << 16 & 16711680) + (uint8[i2 + 1] << 8 & 65280) + (uint8[i2 + 2] & 255);
        output.push(tripletToBase64(tmp));
      }
      return output.join("");
    }
    function fromByteArray2(uint8) {
      var tmp;
      var len2 = uint8.length;
      var extraBytes = len2 % 3;
      var parts = [];
      var maxChunkLength = 16383;
      for (var i2 = 0, len22 = len2 - extraBytes; i2 < len22; i2 += maxChunkLength) {
        parts.push(encodeChunk(uint8, i2, i2 + maxChunkLength > len22 ? len22 : i2 + maxChunkLength));
      }
      if (extraBytes === 1) {
        tmp = uint8[len2 - 1];
        parts.push(
          lookup[tmp >> 2] + lookup[tmp << 4 & 63] + "=="
        );
      } else if (extraBytes === 2) {
        tmp = (uint8[len2 - 2] << 8) + uint8[len2 - 1];
        parts.push(
          lookup[tmp >> 10] + lookup[tmp >> 4 & 63] + lookup[tmp << 2 & 63] + "="
        );
      }
      return parts.join("");
    }
  }
});

// src/extension.ts
var extension_exports = {};
__export(extension_exports, {
  activate: () => activate,
  deactivate: () => deactivate
});
module.exports = __toCommonJS(extension_exports);
var Net = __toESM(require("net"));
var vscode2 = __toESM(require("vscode"));
var import_crypto = require("crypto");
var import_os = require("os");
var import_path = require("path");
var import_process = require("process");

// src/mockDebug.ts
var import_debugadapter = __toESM(require_main());
var import_path_browserify = __toESM(require_path_browserify());

// src/mockRuntime.ts
var import_events = require("events");
var RuntimeVariable = class {
  constructor(name, _value) {
    this.name = name;
    this._value = _value;
  }
  get value() {
    return this._value;
  }
  set value(value) {
    this._value = value;
    this._memory = void 0;
  }
  get memory() {
    if (this._memory === void 0 && typeof this._value === "string") {
      this._memory = new TextEncoder().encode(this._value);
    }
    return this._memory;
  }
  setMemory(data, offset = 0) {
    const memory = this.memory;
    if (!memory) {
      return;
    }
    memory.set(data, offset);
    this._memory = memory;
    this._value = new TextDecoder().decode(memory);
  }
};
function timeout(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
var MockRuntime = class extends import_events.EventEmitter {
  constructor(fileAccessor) {
    super();
    this.fileAccessor = fileAccessor;
    // the initial (and one and only) file we are 'debugging'
    this._sourceFile = "";
    this.variables = /* @__PURE__ */ new Map();
    // the contents (= lines) of the one and only file
    this.sourceLines = [];
    this.instructions = [];
    this.starts = [];
    this.ends = [];
    // This is the next line that will be 'executed'
    this._currentLine = 0;
    // This is the next instruction that will be 'executed'
    this.instruction = 0;
    // maps from sourceFile to array of IRuntimeBreakpoint
    this.breakPoints = /* @__PURE__ */ new Map();
    // all instruction breakpoint addresses
    this.instructionBreakpoints = /* @__PURE__ */ new Set();
    // since we want to send breakpoint events, we will assign an id to every event
    // so that the frontend can match events with breakpoints.
    this.breakpointId = 1;
    this.breakAddresses = /* @__PURE__ */ new Map();
    this.otherExceptions = false;
  }
  get sourceFile() {
    return this._sourceFile;
  }
  get currentLine() {
    return this._currentLine;
  }
  set currentLine(x) {
    this._currentLine = x;
    this.instruction = this.starts[x];
  }
  /**
   * Start executing the given program.
   */
  async start(program, stopOnEntry, debug2) {
    await this.loadSource(this.normalizePathAndCasing(program));
    if (debug2) {
      await this.verifyBreakpoints(this._sourceFile);
      if (stopOnEntry) {
        this.findNextStatement(false, "stopOnEntry");
      } else {
        this.continue(false);
      }
    } else {
      this.continue(false);
    }
  }
  /**
   * Continue execution to the end/beginning.
   */
  continue(reverse) {
    while (!this.executeLine(this.currentLine, reverse)) {
      if (this.updateCurrentLine(reverse)) {
        break;
      }
      if (this.findNextStatement(reverse)) {
        break;
      }
    }
  }
  /**
   * Step to the next/previous non empty line.
   */
  step(instruction, reverse) {
    if (instruction) {
      if (reverse) {
        this.instruction--;
      } else {
        this.instruction++;
      }
      this.sendEvent("stopOnStep");
    } else {
      if (!this.executeLine(this.currentLine, reverse)) {
        if (!this.updateCurrentLine(reverse)) {
          this.findNextStatement(reverse, "stopOnStep");
        }
      }
    }
  }
  updateCurrentLine(reverse) {
    if (reverse) {
      if (this.currentLine > 0) {
        this.currentLine--;
      } else {
        this.currentLine = 0;
        this.currentColumn = void 0;
        this.sendEvent("stopOnEntry");
        return true;
      }
    } else {
      if (this.currentLine < this.sourceLines.length - 1) {
        this.currentLine++;
      } else {
        this.currentColumn = void 0;
        this.sendEvent("end");
        return true;
      }
    }
    return false;
  }
  /**
   * "Step into" for Mock debug means: go to next character
   */
  stepIn(targetId) {
    if (typeof targetId === "number") {
      this.currentColumn = targetId;
      this.sendEvent("stopOnStep");
    } else {
      if (typeof this.currentColumn === "number") {
        if (this.currentColumn <= this.sourceLines[this.currentLine].length) {
          this.currentColumn += 1;
        }
      } else {
        this.currentColumn = 1;
      }
      this.sendEvent("stopOnStep");
    }
  }
  /**
   * "Step out" for Mock debug means: go to previous character
   */
  stepOut() {
    if (typeof this.currentColumn === "number") {
      this.currentColumn -= 1;
      if (this.currentColumn === 0) {
        this.currentColumn = void 0;
      }
    }
    this.sendEvent("stopOnStep");
  }
  getStepInTargets(frameId) {
    const line = this.getLine();
    const words = this.getWords(this.currentLine, line);
    if (frameId < 0 || frameId >= words.length) {
      return [];
    }
    const { name, index } = words[frameId];
    return name.split("").map((c, ix) => {
      return {
        id: index + ix,
        label: `target: ${c}`
      };
    });
  }
  /**
   * Returns a fake 'stacktrace' where every 'stackframe' is a word from the current line.
   */
  stack(startFrame, endFrame) {
    const line = this.getLine();
    const words = this.getWords(this.currentLine, line);
    words.push({ name: "BOTTOM", line: -1, index: -1 });
    const instruction = line.indexOf("disassembly") >= 0 ? this.instruction : void 0;
    const column = typeof this.currentColumn === "number" ? this.currentColumn : void 0;
    const frames = [];
    for (let i = startFrame; i < Math.min(endFrame, words.length); i++) {
      const stackFrame = {
        index: i,
        name: `${words[i].name}(${i})`,
        // use a word of the line as the stackframe name
        file: this._sourceFile,
        line: this.currentLine,
        column,
        // words[i].index
        instruction: instruction ? instruction + i : 0
      };
      frames.push(stackFrame);
    }
    return {
      frames,
      count: words.length
    };
  }
  /*
   * Determine possible column breakpoint positions for the given line.
   * Here we return the start location of words with more than 8 characters.
   */
  getBreakpoints(path, line) {
    return this.getWords(line, this.getLine(line)).filter((w) => w.name.length > 8).map((w) => w.index);
  }
  /*
   * Set breakpoint in file with given line.
   */
  async setBreakPoint(path, line) {
    path = this.normalizePathAndCasing(path);
    const bp = { verified: false, line, id: this.breakpointId++ };
    let bps = this.breakPoints.get(path);
    if (!bps) {
      bps = new Array();
      this.breakPoints.set(path, bps);
    }
    bps.push(bp);
    await this.verifyBreakpoints(path);
    return bp;
  }
  /*
   * Clear breakpoint in file with given line.
   */
  clearBreakPoint(path, line) {
    const bps = this.breakPoints.get(this.normalizePathAndCasing(path));
    if (bps) {
      const index = bps.findIndex((bp) => bp.line === line);
      if (index >= 0) {
        const bp = bps[index];
        bps.splice(index, 1);
        return bp;
      }
    }
    return void 0;
  }
  clearBreakpoints(path) {
    this.breakPoints.delete(this.normalizePathAndCasing(path));
  }
  setDataBreakpoint(address, accessType) {
    const x = accessType === "readWrite" ? "read write" : accessType;
    const t = this.breakAddresses.get(address);
    if (t) {
      if (t !== x) {
        this.breakAddresses.set(address, "read write");
      }
    } else {
      this.breakAddresses.set(address, x);
    }
    return true;
  }
  clearAllDataBreakpoints() {
    this.breakAddresses.clear();
  }
  setExceptionsFilters(namedException, otherExceptions) {
    this.namedException = namedException;
    this.otherExceptions = otherExceptions;
  }
  setInstructionBreakpoint(address) {
    this.instructionBreakpoints.add(address);
    return true;
  }
  clearInstructionBreakpoints() {
    this.instructionBreakpoints.clear();
  }
  async getGlobalVariables(cancellationToken) {
    let a = [];
    for (let i = 0; i < 10; i++) {
      a.push(new RuntimeVariable(`global_${i}`, i));
      if (cancellationToken && cancellationToken()) {
        break;
      }
      await timeout(1e3);
    }
    return a;
  }
  getLocalVariables() {
    return Array.from(this.variables, ([name, value]) => value);
  }
  getLocalVariable(name) {
    return this.variables.get(name);
  }
  /**
   * Return words of the given address range as "instructions"
   */
  disassemble(address, instructionCount) {
    const instructions = [];
    for (let a = address; a < address + instructionCount; a++) {
      if (a >= 0 && a < this.instructions.length) {
        instructions.push({
          address: a,
          instruction: this.instructions[a].name,
          line: this.instructions[a].line
        });
      } else {
        instructions.push({
          address: a,
          instruction: "nop"
        });
      }
    }
    return instructions;
  }
  // private methods
  getLine(line) {
    return this.sourceLines[line === void 0 ? this.currentLine : line].trim();
  }
  getWords(l, line) {
    const WORD_REGEXP = /[a-z]+/ig;
    const words = [];
    let match;
    while (match = WORD_REGEXP.exec(line)) {
      words.push({ name: match[0], line: l, index: match.index });
    }
    return words;
  }
  async loadSource(file) {
    if (this._sourceFile !== file) {
      this._sourceFile = this.normalizePathAndCasing(file);
      this.initializeContents(await this.fileAccessor.readFile(file));
    }
  }
  initializeContents(memory) {
    this.sourceLines = new TextDecoder().decode(memory).split(/\r?\n/);
    this.instructions = [];
    this.starts = [];
    this.instructions = [];
    this.ends = [];
    for (let l = 0; l < this.sourceLines.length; l++) {
      this.starts.push(this.instructions.length);
      const words = this.getWords(l, this.sourceLines[l]);
      for (let word of words) {
        this.instructions.push(word);
      }
      this.ends.push(this.instructions.length);
    }
  }
  /**
   * return true on stop
   */
  findNextStatement(reverse, stepEvent) {
    for (let ln = this.currentLine; reverse ? ln >= 0 : ln < this.sourceLines.length; reverse ? ln-- : ln++) {
      const breakpoints = this.breakPoints.get(this._sourceFile);
      if (breakpoints) {
        const bps = breakpoints.filter((bp) => bp.line === ln);
        if (bps.length > 0) {
          this.sendEvent("stopOnBreakpoint");
          if (!bps[0].verified) {
            bps[0].verified = true;
            this.sendEvent("breakpointValidated", bps[0]);
          }
          this.currentLine = ln;
          return true;
        }
      }
      const line = this.getLine(ln);
      if (line.length > 0) {
        this.currentLine = ln;
        break;
      }
    }
    if (stepEvent) {
      this.sendEvent(stepEvent);
      return true;
    }
    return false;
  }
  /**
   * "execute a line" of the readme markdown.
   * Returns true if execution sent out a stopped event and needs to stop.
   */
  executeLine(ln, reverse) {
    while (reverse ? this.instruction >= this.starts[ln] : this.instruction < this.ends[ln]) {
      reverse ? this.instruction-- : this.instruction++;
      if (this.instructionBreakpoints.has(this.instruction)) {
        this.sendEvent("stopOnInstructionBreakpoint");
        return true;
      }
    }
    const line = this.getLine(ln);
    let reg0 = /\$([a-z][a-z0-9]*)(=(false|true|[0-9]+(\.[0-9]+)?|\".*\"|\{.*\}))?/ig;
    let matches0;
    while (matches0 = reg0.exec(line)) {
      if (matches0.length === 5) {
        let access;
        const name = matches0[1];
        const value = matches0[3];
        let v = new RuntimeVariable(name, value);
        if (value && value.length > 0) {
          if (value === "true") {
            v.value = true;
          } else if (value === "false") {
            v.value = false;
          } else if (value[0] === '"') {
            v.value = value.slice(1, -1);
          } else if (value[0] === "{") {
            v.value = [
              new RuntimeVariable("fBool", true),
              new RuntimeVariable("fInteger", 123),
              new RuntimeVariable("fString", "hello"),
              new RuntimeVariable("flazyInteger", 321)
            ];
          } else {
            v.value = parseFloat(value);
          }
          if (this.variables.has(name)) {
            access = "write";
          }
          this.variables.set(name, v);
        } else {
          if (this.variables.has(name)) {
            access = "read";
          }
        }
        const accessType = this.breakAddresses.get(name);
        if (access && accessType && accessType.indexOf(access) >= 0) {
          this.sendEvent("stopOnDataBreakpoint", access);
          return true;
        }
      }
    }
    const reg1 = /(log|prio|out|err)\(([^\)]*)\)/g;
    let matches1;
    while (matches1 = reg1.exec(line)) {
      if (matches1.length === 3) {
        this.sendEvent("output", matches1[1], matches1[2], this._sourceFile, ln, matches1.index);
      }
    }
    const matches2 = /exception\((.*)\)/.exec(line);
    if (matches2 && matches2.length === 2) {
      const exception = matches2[1].trim();
      if (this.namedException === exception) {
        this.sendEvent("stopOnException", exception);
        return true;
      } else {
        if (this.otherExceptions) {
          this.sendEvent("stopOnException", void 0);
          return true;
        }
      }
    } else {
      if (line.indexOf("exception") >= 0) {
        if (this.otherExceptions) {
          this.sendEvent("stopOnException", void 0);
          return true;
        }
      }
    }
    return false;
  }
  async verifyBreakpoints(path) {
    const bps = this.breakPoints.get(path);
    if (bps) {
      await this.loadSource(path);
      bps.forEach((bp) => {
        if (!bp.verified && bp.line < this.sourceLines.length) {
          const srcLine = this.getLine(bp.line);
          if (srcLine.length === 0 || srcLine.indexOf("+") === 0) {
            bp.line++;
          }
          if (srcLine.indexOf("-") === 0) {
            bp.line--;
          }
          if (srcLine.indexOf("lazy") < 0) {
            bp.verified = true;
            this.sendEvent("breakpointValidated", bp);
          }
        }
      });
    }
  }
  sendEvent(event, ...args) {
    setTimeout(() => {
      this.emit(event, ...args);
    }, 0);
  }
  normalizePathAndCasing(path) {
    if (this.fileAccessor.isWindows) {
      return path.replace(/\//g, "\\").toLowerCase();
    } else {
      return path.replace(/\\/g, "/");
    }
  }
};

// src/mockDebug.ts
var import_await_notify = __toESM(require_await_notify());
var base64 = __toESM(require_base64_js());
var MockDebugSession = class _MockDebugSession extends import_debugadapter.LoggingDebugSession {
  /**
   * Creates a new debug adapter that is used for one debug session.
   * We configure the default implementation of a debug adapter here.
   */
  constructor(fileAccessor) {
    super("mock-debug.txt");
    this._variableHandles = new import_debugadapter.Handles();
    this._configurationDone = new import_await_notify.Subject();
    this._cancellationTokens = /* @__PURE__ */ new Map();
    this._reportProgress = false;
    this._progressId = 1e4;
    this._cancelledProgressId = void 0;
    this._isProgressCancellable = true;
    this._valuesInHex = false;
    this._useInvalidatedEvent = false;
    this._addressesInHex = true;
    this.setDebuggerLinesStartAt1(false);
    this.setDebuggerColumnsStartAt1(false);
    this._runtime = new MockRuntime(fileAccessor);
    this._runtime.on("stopOnEntry", () => {
      this.sendEvent(new import_debugadapter.StoppedEvent("entry", _MockDebugSession.threadID));
    });
    this._runtime.on("stopOnStep", () => {
      this.sendEvent(new import_debugadapter.StoppedEvent("step", _MockDebugSession.threadID));
    });
    this._runtime.on("stopOnBreakpoint", () => {
      this.sendEvent(new import_debugadapter.StoppedEvent("breakpoint", _MockDebugSession.threadID));
    });
    this._runtime.on("stopOnDataBreakpoint", () => {
      this.sendEvent(new import_debugadapter.StoppedEvent("data breakpoint", _MockDebugSession.threadID));
    });
    this._runtime.on("stopOnInstructionBreakpoint", () => {
      this.sendEvent(new import_debugadapter.StoppedEvent("instruction breakpoint", _MockDebugSession.threadID));
    });
    this._runtime.on("stopOnException", (exception) => {
      if (exception) {
        this.sendEvent(new import_debugadapter.StoppedEvent(`exception(${exception})`, _MockDebugSession.threadID));
      } else {
        this.sendEvent(new import_debugadapter.StoppedEvent("exception", _MockDebugSession.threadID));
      }
    });
    this._runtime.on("breakpointValidated", (bp) => {
      this.sendEvent(new import_debugadapter.BreakpointEvent("changed", { verified: bp.verified, id: bp.id }));
    });
    this._runtime.on("output", (type, text, filePath, line, column) => {
      let category;
      switch (type) {
        case "prio":
          category = "important";
          break;
        case "out":
          category = "stdout";
          break;
        case "err":
          category = "stderr";
          break;
        default:
          category = "console";
          break;
      }
      const e = new import_debugadapter.OutputEvent(`${text}
`, category);
      if (text === "start" || text === "startCollapsed" || text === "end") {
        e.body.group = text;
        e.body.output = `group-${text}
`;
      }
      e.body.source = this.createSource(filePath);
      e.body.line = this.convertDebuggerLineToClient(line);
      e.body.column = this.convertDebuggerColumnToClient(column);
      this.sendEvent(e);
    });
    this._runtime.on("end", () => {
      this.sendEvent(new import_debugadapter.TerminatedEvent());
    });
  }
  static {
    // we don't support multiple threads, so we can use a hardcoded ID for the default thread
    this.threadID = 1;
  }
  /**
   * The 'initialize' request is the first request called by the frontend
   * to interrogate the features the debug adapter provides.
   */
  initializeRequest(response, args) {
    if (args.supportsProgressReporting) {
      this._reportProgress = true;
    }
    if (args.supportsInvalidatedEvent) {
      this._useInvalidatedEvent = true;
    }
    response.body = response.body || {};
    response.body.supportsConfigurationDoneRequest = true;
    response.body.supportsEvaluateForHovers = true;
    response.body.supportsStepBack = true;
    response.body.supportsDataBreakpoints = true;
    response.body.supportsCompletionsRequest = true;
    response.body.completionTriggerCharacters = [".", "["];
    response.body.supportsCancelRequest = true;
    response.body.supportsBreakpointLocationsRequest = true;
    response.body.supportsStepInTargetsRequest = true;
    response.body.supportsExceptionFilterOptions = true;
    response.body.exceptionBreakpointFilters = [
      {
        filter: "namedException",
        label: "Named Exception",
        description: `Break on named exceptions. Enter the exception's name as the Condition.`,
        default: false,
        supportsCondition: true,
        conditionDescription: `Enter the exception's name`
      },
      {
        filter: "otherExceptions",
        label: "Other Exceptions",
        description: "This is a other exception",
        default: true,
        supportsCondition: false
      }
    ];
    response.body.supportsExceptionInfoRequest = true;
    response.body.supportsSetVariable = true;
    response.body.supportsSetExpression = true;
    response.body.supportsDisassembleRequest = true;
    response.body.supportsSteppingGranularity = true;
    response.body.supportsInstructionBreakpoints = true;
    response.body.supportsReadMemoryRequest = true;
    response.body.supportsWriteMemoryRequest = true;
    response.body.supportSuspendDebuggee = true;
    response.body.supportTerminateDebuggee = true;
    response.body.supportsFunctionBreakpoints = true;
    response.body.supportsDelayedStackTraceLoading = true;
    this.sendResponse(response);
    this.sendEvent(new import_debugadapter.InitializedEvent());
  }
  /**
   * Called at the end of the configuration sequence.
   * Indicates that all breakpoints etc. have been sent to the DA and that the 'launch' can start.
   */
  configurationDoneRequest(response, args) {
    super.configurationDoneRequest(response, args);
    this._configurationDone.notify();
  }
  disconnectRequest(response, args, request) {
    console.log(`disconnectRequest suspend: ${args.suspendDebuggee}, terminate: ${args.terminateDebuggee}`);
  }
  async attachRequest(response, args) {
    return this.launchRequest(response, args);
  }
  async launchRequest(response, args) {
    import_debugadapter.logger.setup(args.trace ? import_debugadapter.Logger.LogLevel.Verbose : import_debugadapter.Logger.LogLevel.Stop, false);
    await this._configurationDone.wait(1e3);
    await this._runtime.start(args.program, !!args.stopOnEntry, !args.noDebug);
    if (args.compileError) {
      this.sendErrorResponse(response, {
        id: 1001,
        format: `compile error: some fake error.`,
        showUser: args.compileError === "show" ? true : args.compileError === "hide" ? false : void 0
      });
    } else {
      this.sendResponse(response);
    }
  }
  setFunctionBreakPointsRequest(response, args, request) {
    this.sendResponse(response);
  }
  async setBreakPointsRequest(response, args) {
    const path = args.source.path;
    const clientLines = args.lines || [];
    this._runtime.clearBreakpoints(path);
    const actualBreakpoints0 = clientLines.map(async (l) => {
      const { verified, line, id } = await this._runtime.setBreakPoint(path, this.convertClientLineToDebugger(l));
      const bp = new import_debugadapter.Breakpoint(verified, this.convertDebuggerLineToClient(line));
      bp.id = id;
      return bp;
    });
    const actualBreakpoints = await Promise.all(actualBreakpoints0);
    response.body = {
      breakpoints: actualBreakpoints
    };
    this.sendResponse(response);
  }
  breakpointLocationsRequest(response, args, request) {
    if (args.source.path) {
      const bps = this._runtime.getBreakpoints(args.source.path, this.convertClientLineToDebugger(args.line));
      response.body = {
        breakpoints: bps.map((col) => {
          return {
            line: args.line,
            column: this.convertDebuggerColumnToClient(col)
          };
        })
      };
    } else {
      response.body = {
        breakpoints: []
      };
    }
    this.sendResponse(response);
  }
  async setExceptionBreakPointsRequest(response, args) {
    let namedException = void 0;
    let otherExceptions = false;
    if (args.filterOptions) {
      for (const filterOption of args.filterOptions) {
        switch (filterOption.filterId) {
          case "namedException":
            namedException = args.filterOptions[0].condition;
            break;
          case "otherExceptions":
            otherExceptions = true;
            break;
        }
      }
    }
    if (args.filters) {
      if (args.filters.indexOf("otherExceptions") >= 0) {
        otherExceptions = true;
      }
    }
    this._runtime.setExceptionsFilters(namedException, otherExceptions);
    this.sendResponse(response);
  }
  exceptionInfoRequest(response, args) {
    response.body = {
      exceptionId: "Exception ID",
      description: "This is a descriptive description of the exception.",
      breakMode: "always",
      details: {
        message: "Message contained in the exception.",
        typeName: "Short type name of the exception object",
        stackTrace: "stack frame 1\nstack frame 2"
      }
    };
    this.sendResponse(response);
  }
  threadsRequest(response) {
    response.body = {
      threads: [
        new import_debugadapter.Thread(_MockDebugSession.threadID, "thread 1"),
        new import_debugadapter.Thread(_MockDebugSession.threadID + 1, "thread 2")
      ]
    };
    this.sendResponse(response);
  }
  stackTraceRequest(response, args) {
    const startFrame = typeof args.startFrame === "number" ? args.startFrame : 0;
    const maxLevels = typeof args.levels === "number" ? args.levels : 1e3;
    const endFrame = startFrame + maxLevels;
    const stk = this._runtime.stack(startFrame, endFrame);
    response.body = {
      stackFrames: stk.frames.map((f, ix) => {
        const sf = new import_debugadapter.StackFrame(f.index, f.name, this.createSource(f.file), this.convertDebuggerLineToClient(f.line));
        if (typeof f.column === "number") {
          sf.column = this.convertDebuggerColumnToClient(f.column);
        }
        if (typeof f.instruction === "number") {
          const address = this.formatAddress(f.instruction);
          sf.name = `${f.name} ${address}`;
          sf.instructionPointerReference = address;
        }
        return sf;
      }),
      // 4 options for 'totalFrames':
      //omit totalFrames property: 	// VS Code has to probe/guess. Should result in a max. of two requests
      totalFrames: stk.count
      // stk.count is the correct size, should result in a max. of two requests
      //totalFrames: 1000000 			// not the correct size, should result in a max. of two requests
      //totalFrames: endFrame + 20 	// dynamically increases the size with every requested chunk, results in paging
    };
    this.sendResponse(response);
  }
  scopesRequest(response, args) {
    response.body = {
      scopes: [
        new import_debugadapter.Scope("Locals", this._variableHandles.create("locals"), false),
        new import_debugadapter.Scope("Globals", this._variableHandles.create("globals"), true)
      ]
    };
    this.sendResponse(response);
  }
  async writeMemoryRequest(response, { data, memoryReference, offset = 0 }) {
    const variable = this._variableHandles.get(Number(memoryReference));
    if (typeof variable === "object") {
      const decoded = base64.toByteArray(data);
      variable.setMemory(decoded, offset);
      response.body = { bytesWritten: decoded.length };
    } else {
      response.body = { bytesWritten: 0 };
    }
    this.sendResponse(response);
    this.sendEvent(new import_debugadapter.InvalidatedEvent(["variables"]));
  }
  async readMemoryRequest(response, { offset = 0, count, memoryReference }) {
    const variable = this._variableHandles.get(Number(memoryReference));
    if (typeof variable === "object" && variable.memory) {
      const memory = variable.memory.subarray(
        Math.min(offset, variable.memory.length),
        Math.min(offset + count, variable.memory.length)
      );
      response.body = {
        address: offset.toString(),
        data: base64.fromByteArray(memory),
        unreadableBytes: count - memory.length
      };
    } else {
      response.body = {
        address: offset.toString(),
        data: "",
        unreadableBytes: count
      };
    }
    this.sendResponse(response);
  }
  async variablesRequest(response, args, request) {
    let vs = [];
    const v = this._variableHandles.get(args.variablesReference);
    if (v === "locals") {
      vs = this._runtime.getLocalVariables();
    } else if (v === "globals") {
      if (request) {
        this._cancellationTokens.set(request.seq, false);
        vs = await this._runtime.getGlobalVariables(() => !!this._cancellationTokens.get(request.seq));
        this._cancellationTokens.delete(request.seq);
      } else {
        vs = await this._runtime.getGlobalVariables();
      }
    } else if (v && Array.isArray(v.value)) {
      vs = v.value;
    }
    response.body = {
      variables: vs.map((v2) => this.convertFromRuntime(v2))
    };
    this.sendResponse(response);
  }
  setVariableRequest(response, args) {
    const container = this._variableHandles.get(args.variablesReference);
    const rv = container === "locals" ? this._runtime.getLocalVariable(args.name) : container instanceof RuntimeVariable && container.value instanceof Array ? container.value.find((v) => v.name === args.name) : void 0;
    if (rv) {
      rv.value = this.convertToRuntime(args.value);
      response.body = this.convertFromRuntime(rv);
      if (rv.memory && rv.reference) {
        this.sendEvent(new import_debugadapter.MemoryEvent(String(rv.reference), 0, rv.memory.length));
      }
    }
    this.sendResponse(response);
  }
  continueRequest(response, args) {
    this._runtime.continue(false);
    this.sendResponse(response);
  }
  reverseContinueRequest(response, args) {
    this._runtime.continue(true);
    this.sendResponse(response);
  }
  nextRequest(response, args) {
    this._runtime.step(args.granularity === "instruction", false);
    this.sendResponse(response);
  }
  stepBackRequest(response, args) {
    this._runtime.step(args.granularity === "instruction", true);
    this.sendResponse(response);
  }
  stepInTargetsRequest(response, args) {
    const targets = this._runtime.getStepInTargets(args.frameId);
    response.body = {
      targets: targets.map((t) => {
        return { id: t.id, label: t.label };
      })
    };
    this.sendResponse(response);
  }
  stepInRequest(response, args) {
    this._runtime.stepIn(args.targetId);
    this.sendResponse(response);
  }
  stepOutRequest(response, args) {
    this._runtime.stepOut();
    this.sendResponse(response);
  }
  async evaluateRequest(response, args) {
    let reply;
    let rv;
    switch (args.context) {
      case "repl":
        const matches = /new +([0-9]+)/.exec(args.expression);
        if (matches && matches.length === 2) {
          const mbp = await this._runtime.setBreakPoint(this._runtime.sourceFile, this.convertClientLineToDebugger(parseInt(matches[1])));
          const bp = new import_debugadapter.Breakpoint(mbp.verified, this.convertDebuggerLineToClient(mbp.line), void 0, this.createSource(this._runtime.sourceFile));
          bp.id = mbp.id;
          this.sendEvent(new import_debugadapter.BreakpointEvent("new", bp));
          reply = `breakpoint created`;
        } else {
          const matches2 = /del +([0-9]+)/.exec(args.expression);
          if (matches2 && matches2.length === 2) {
            const mbp = this._runtime.clearBreakPoint(this._runtime.sourceFile, this.convertClientLineToDebugger(parseInt(matches2[1])));
            if (mbp) {
              const bp = new import_debugadapter.Breakpoint(false);
              bp.id = mbp.id;
              this.sendEvent(new import_debugadapter.BreakpointEvent("removed", bp));
              reply = `breakpoint deleted`;
            }
          } else {
            const matches3 = /progress/.exec(args.expression);
            if (matches3 && matches3.length === 1) {
              if (this._reportProgress) {
                reply = `progress started`;
                this.progressSequence();
              } else {
                reply = `frontend doesn't support progress (capability 'supportsProgressReporting' not set)`;
              }
            }
          }
        }
      // fall through
      default:
        if (args.expression.startsWith("$")) {
          rv = this._runtime.getLocalVariable(args.expression.substr(1));
        } else {
          rv = new RuntimeVariable("eval", this.convertToRuntime(args.expression));
        }
        break;
    }
    if (rv) {
      const v = this.convertFromRuntime(rv);
      response.body = {
        result: v.value,
        type: v.type,
        variablesReference: v.variablesReference,
        presentationHint: v.presentationHint
      };
    } else {
      response.body = {
        result: reply ? reply : `evaluate(context: '${args.context}', '${args.expression}')`,
        variablesReference: 0
      };
    }
    this.sendResponse(response);
  }
  setExpressionRequest(response, args) {
    if (args.expression.startsWith("$")) {
      const rv = this._runtime.getLocalVariable(args.expression.substr(1));
      if (rv) {
        rv.value = this.convertToRuntime(args.value);
        response.body = this.convertFromRuntime(rv);
        this.sendResponse(response);
      } else {
        this.sendErrorResponse(response, {
          id: 1002,
          format: `variable '{lexpr}' not found`,
          variables: { lexpr: args.expression },
          showUser: true
        });
      }
    } else {
      this.sendErrorResponse(response, {
        id: 1003,
        format: `'{lexpr}' not an assignable expression`,
        variables: { lexpr: args.expression },
        showUser: true
      });
    }
  }
  async progressSequence() {
    const ID = "" + this._progressId++;
    await timeout(100);
    const title = this._isProgressCancellable ? "Cancellable operation" : "Long running operation";
    const startEvent = new import_debugadapter.ProgressStartEvent(ID, title);
    startEvent.body.cancellable = this._isProgressCancellable;
    this._isProgressCancellable = !this._isProgressCancellable;
    this.sendEvent(startEvent);
    this.sendEvent(new import_debugadapter.OutputEvent(`start progress: ${ID}
`));
    let endMessage = "progress ended";
    for (let i = 0; i < 100; i++) {
      await timeout(500);
      this.sendEvent(new import_debugadapter.ProgressUpdateEvent(ID, `progress: ${i}`));
      if (this._cancelledProgressId === ID) {
        endMessage = "progress cancelled";
        this._cancelledProgressId = void 0;
        this.sendEvent(new import_debugadapter.OutputEvent(`cancel progress: ${ID}
`));
        break;
      }
    }
    this.sendEvent(new import_debugadapter.ProgressEndEvent(ID, endMessage));
    this.sendEvent(new import_debugadapter.OutputEvent(`end progress: ${ID}
`));
    this._cancelledProgressId = void 0;
  }
  dataBreakpointInfoRequest(response, args) {
    response.body = {
      dataId: null,
      description: "cannot break on data access",
      accessTypes: void 0,
      canPersist: false
    };
    if (args.variablesReference && args.name) {
      const v = this._variableHandles.get(args.variablesReference);
      if (v === "globals") {
        response.body.dataId = args.name;
        response.body.description = args.name;
        response.body.accessTypes = ["write"];
        response.body.canPersist = true;
      } else {
        response.body.dataId = args.name;
        response.body.description = args.name;
        response.body.accessTypes = ["read", "write", "readWrite"];
        response.body.canPersist = true;
      }
    }
    this.sendResponse(response);
  }
  setDataBreakpointsRequest(response, args) {
    this._runtime.clearAllDataBreakpoints();
    response.body = {
      breakpoints: []
    };
    for (const dbp of args.breakpoints) {
      const ok = this._runtime.setDataBreakpoint(dbp.dataId, dbp.accessType || "write");
      response.body.breakpoints.push({
        verified: ok
      });
    }
    this.sendResponse(response);
  }
  completionsRequest(response, args) {
    response.body = {
      targets: [
        {
          label: "item 10",
          sortText: "10"
        },
        {
          label: "item 1",
          sortText: "01",
          detail: "detail 1"
        },
        {
          label: "item 2",
          sortText: "02",
          detail: "detail 2"
        },
        {
          label: "array[]",
          selectionStart: 6,
          sortText: "03"
        },
        {
          label: "func(arg)",
          selectionStart: 5,
          selectionLength: 3,
          sortText: "04"
        }
      ]
    };
    this.sendResponse(response);
  }
  cancelRequest(response, args) {
    if (args.requestId) {
      this._cancellationTokens.set(args.requestId, true);
    }
    if (args.progressId) {
      this._cancelledProgressId = args.progressId;
    }
  }
  disassembleRequest(response, args) {
    const memoryInt = args.memoryReference.slice(3);
    const baseAddress = parseInt(memoryInt);
    const offset = args.instructionOffset || 0;
    const count = args.instructionCount;
    const isHex = memoryInt.startsWith("0x");
    const pad = isHex ? memoryInt.length - 2 : memoryInt.length;
    const loc = this.createSource(this._runtime.sourceFile);
    let lastLine = -1;
    const instructions = this._runtime.disassemble(baseAddress + offset, count).map((instruction) => {
      let address = Math.abs(instruction.address).toString(isHex ? 16 : 10).padStart(pad, "0");
      const sign = instruction.address < 0 ? "-" : "";
      const instr = {
        address: sign + (isHex ? `0x${address}` : `${address}`),
        instruction: instruction.instruction
      };
      if (instruction.line !== void 0 && lastLine !== instruction.line) {
        lastLine = instruction.line;
        instr.location = loc;
        instr.line = this.convertDebuggerLineToClient(instruction.line);
      }
      return instr;
    });
    response.body = {
      instructions
    };
    this.sendResponse(response);
  }
  setInstructionBreakpointsRequest(response, args) {
    this._runtime.clearInstructionBreakpoints();
    const breakpoints = args.breakpoints.map((ibp) => {
      const address = parseInt(ibp.instructionReference.slice(3));
      const offset = ibp.offset || 0;
      return {
        verified: this._runtime.setInstructionBreakpoint(address + offset)
      };
    });
    response.body = {
      breakpoints
    };
    this.sendResponse(response);
  }
  customRequest(command, response, args) {
    if (command === "toggleFormatting") {
      this._valuesInHex = !this._valuesInHex;
      if (this._useInvalidatedEvent) {
        this.sendEvent(new import_debugadapter.InvalidatedEvent(["variables"]));
      }
      this.sendResponse(response);
    } else {
      super.customRequest(command, response, args);
    }
  }
  //---- helpers
  convertToRuntime(value) {
    value = value.trim();
    if (value === "true") {
      return true;
    }
    if (value === "false") {
      return false;
    }
    if (value[0] === "'" || value[0] === '"') {
      return value.substr(1, value.length - 2);
    }
    const n = parseFloat(value);
    if (!isNaN(n)) {
      return n;
    }
    return value;
  }
  convertFromRuntime(v) {
    let dapVariable = {
      name: v.name,
      value: "???",
      type: typeof v.value,
      variablesReference: 0,
      evaluateName: "$" + v.name
    };
    if (v.name.indexOf("lazy") >= 0) {
      dapVariable.value = "lazy var";
      v.reference ??= this._variableHandles.create(new RuntimeVariable("", [new RuntimeVariable("", v.value)]));
      dapVariable.variablesReference = v.reference;
      dapVariable.presentationHint = { lazy: true };
    } else {
      if (Array.isArray(v.value)) {
        dapVariable.value = "Object";
        v.reference ??= this._variableHandles.create(v);
        dapVariable.variablesReference = v.reference;
      } else {
        switch (typeof v.value) {
          case "number":
            if (Math.round(v.value) === v.value) {
              dapVariable.value = this.formatNumber(v.value);
              dapVariable.__vscodeVariableMenuContext = "simple";
              dapVariable.type = "integer";
            } else {
              dapVariable.value = v.value.toString();
              dapVariable.type = "float";
            }
            break;
          case "string":
            dapVariable.value = `"${v.value}"`;
            break;
          case "boolean":
            dapVariable.value = v.value ? "true" : "false";
            break;
          default:
            dapVariable.value = typeof v.value;
            break;
        }
      }
    }
    if (v.memory) {
      v.reference ??= this._variableHandles.create(v);
      dapVariable.memoryReference = String(v.reference);
    }
    return dapVariable;
  }
  formatAddress(x, pad = 8) {
    return "mem" + (this._addressesInHex ? "0x" + x.toString(16).padStart(8, "0") : x.toString(10));
  }
  formatNumber(x) {
    return this._valuesInHex ? "0x" + x.toString(16) : x.toString(10);
  }
  createSource(filePath) {
    return new import_debugadapter.Source((0, import_path_browserify.basename)(filePath), this.convertDebuggerPathToClient(filePath), void 0, void 0, "mock-adapter-data");
  }
};

// src/activateMockDebug.ts
var vscode = __toESM(require("vscode"));
function activateMockDebug(context, factory) {
  context.subscriptions.push(
    vscode.commands.registerCommand("extension.mock-debug.runEditorContents", (resource) => {
      let targetResource = resource;
      if (!targetResource && vscode.window.activeTextEditor) {
        targetResource = vscode.window.activeTextEditor.document.uri;
      }
      if (targetResource) {
        vscode.debug.startDebugging(
          void 0,
          {
            type: "mock",
            name: "Run File",
            request: "launch",
            program: targetResource.fsPath
          },
          { noDebug: true }
        );
      }
    }),
    vscode.commands.registerCommand("extension.mock-debug.debugEditorContents", (resource) => {
      let targetResource = resource;
      if (!targetResource && vscode.window.activeTextEditor) {
        targetResource = vscode.window.activeTextEditor.document.uri;
      }
      if (targetResource) {
        vscode.debug.startDebugging(void 0, {
          type: "mock",
          name: "Debug File",
          request: "launch",
          program: targetResource.fsPath,
          stopOnEntry: true
        });
      }
    }),
    vscode.commands.registerCommand("extension.mock-debug.toggleFormatting", (variable) => {
      const ds = vscode.debug.activeDebugSession;
      if (ds) {
        ds.customRequest("toggleFormatting");
      }
    })
  );
  context.subscriptions.push(vscode.commands.registerCommand("extension.mock-debug.getProgramName", (config) => {
    return vscode.window.showInputBox({
      placeHolder: "Please enter the name of a markdown file in the workspace folder",
      value: "readme.md"
    });
  }));
  const provider = new MockConfigurationProvider();
  context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider("mock", provider));
  context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider("mock", {
    provideDebugConfigurations(folder) {
      return [
        {
          name: "Dynamic Launch",
          request: "launch",
          type: "mock",
          program: "${file}"
        },
        {
          name: "Another Dynamic Launch",
          request: "launch",
          type: "mock",
          program: "${file}"
        },
        {
          name: "Mock Launch",
          request: "launch",
          type: "mock",
          program: "${file}"
        }
      ];
    }
  }, vscode.DebugConfigurationProviderTriggerKind.Dynamic));
  if (!factory) {
    factory = new InlineDebugAdapterFactory();
  }
  context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory("mock", factory));
  if ("dispose" in factory) {
  }
  context.subscriptions.push(vscode.languages.registerEvaluatableExpressionProvider("markdown", {
    provideEvaluatableExpression(document, position) {
      const VARIABLE_REGEXP = /\$[a-z][a-z0-9]*/ig;
      const line = document.lineAt(position.line).text;
      let m;
      while (m = VARIABLE_REGEXP.exec(line)) {
        const varRange = new vscode.Range(position.line, m.index, position.line, m.index + m[0].length);
        if (varRange.contains(position)) {
          return new vscode.EvaluatableExpression(varRange);
        }
      }
      return void 0;
    }
  }));
  context.subscriptions.push(vscode.languages.registerInlineValuesProvider("markdown", {
    provideInlineValues(document, viewport, context2) {
      const allValues = [];
      for (let l = viewport.start.line; l <= context2.stoppedLocation.end.line; l++) {
        const line = document.lineAt(l);
        var regExp = /\$([a-z][a-z0-9]*)/ig;
        do {
          var m = regExp.exec(line.text);
          if (m) {
            const varName = m[1];
            const varRange = new vscode.Range(l, m.index, l, m.index + varName.length);
            allValues.push(new vscode.InlineValueVariableLookup(varRange, varName, false));
          }
        } while (m);
      }
      return allValues;
    }
  }));
}
var MockConfigurationProvider = class {
  /**
   * Massage a debug configuration just before a debug session is being launched,
   * e.g. add all missing attributes to the debug configuration.
   */
  resolveDebugConfiguration(folder, config, token) {
    if (!config.type && !config.request && !config.name) {
      const editor = vscode.window.activeTextEditor;
      if (editor && editor.document.languageId === "markdown") {
        config.type = "mock";
        config.name = "Launch";
        config.request = "launch";
        config.program = "${file}";
        config.stopOnEntry = true;
      }
    }
    if (!config.program) {
      return vscode.window.showInformationMessage("Cannot find a program to debug").then((_) => {
        return void 0;
      });
    }
    return config;
  }
};
var workspaceFileAccessor = {
  isWindows: typeof process !== "undefined" && process.platform === "win32",
  async readFile(path) {
    let uri;
    try {
      uri = pathToUri(path);
    } catch (e) {
      return new TextEncoder().encode(`cannot read '${path}'`);
    }
    return await vscode.workspace.fs.readFile(uri);
  },
  async writeFile(path, contents) {
    await vscode.workspace.fs.writeFile(pathToUri(path), contents);
  }
};
function pathToUri(path) {
  try {
    return vscode.Uri.file(path);
  } catch (e) {
    return vscode.Uri.parse(path);
  }
}
var InlineDebugAdapterFactory = class {
  createDebugAdapterDescriptor(_session) {
    return new vscode.DebugAdapterInlineImplementation(new MockDebugSession(workspaceFileAccessor));
  }
  dispose() {
  }
};

// src/extension.ts
var runMode = "inline";
function activate(context) {
  switch (runMode) {
    case "server":
      activateMockDebug(context, new MockDebugAdapterServerDescriptorFactory());
      break;
    case "namedPipeServer":
      activateMockDebug(context, new MockDebugAdapterNamedPipeServerDescriptorFactory());
      break;
    case "external":
    default:
      activateMockDebug(context, new DebugAdapterExecutableFactory());
      break;
    case "inline":
      activateMockDebug(context);
      break;
  }
}
function deactivate() {
}
var DebugAdapterExecutableFactory = class {
  // The following use of a DebugAdapter factory shows how to control what debug adapter executable is used.
  // Since the code implements the default behavior, it is absolutely not neccessary and we show it here only for educational purpose.
  createDebugAdapterDescriptor(_session, executable) {
    if (!executable) {
      const command = "absolute path to my DA executable";
      const args = [
        "some args",
        "another arg"
      ];
      const options = {
        cwd: "working directory for executable",
        env: { "envVariable": "some value" }
      };
      executable = new vscode2.DebugAdapterExecutable(command, args, options);
    }
    return executable;
  }
};
var MockDebugAdapterServerDescriptorFactory = class {
  createDebugAdapterDescriptor(session, executable) {
    if (!this.server) {
      this.server = Net.createServer((socket) => {
        const session2 = new MockDebugSession(workspaceFileAccessor);
        session2.setRunAsServer(true);
        session2.start(socket, socket);
      }).listen(0);
    }
    return new vscode2.DebugAdapterServer(this.server.address().port);
  }
  dispose() {
    if (this.server) {
      this.server.close();
    }
  }
};
var MockDebugAdapterNamedPipeServerDescriptorFactory = class {
  createDebugAdapterDescriptor(session, executable) {
    if (!this.server) {
      const pipeName = (0, import_crypto.randomBytes)(10).toString("utf8");
      const pipePath = import_process.platform === "win32" ? (0, import_path.join)("\\\\.\\pipe\\", pipeName) : (0, import_path.join)((0, import_os.tmpdir)(), pipeName);
      this.server = Net.createServer((socket) => {
        const session2 = new MockDebugSession(workspaceFileAccessor);
        session2.setRunAsServer(true);
        session2.start(socket, socket);
      }).listen(pipePath);
    }
    return new vscode2.DebugAdapterNamedPipeServer(this.server.address());
  }
  dispose() {
    if (this.server) {
      this.server.close();
    }
  }
};
// Annotate the CommonJS export names for ESM import in node:
0 && (module.exports = {
  activate,
  deactivate
});
//# sourceMappingURL=extension.js.map
