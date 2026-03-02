/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
/*
 * fosDebug.ts implements the Debug Adapter that "adapts" or translates the Debug Adapter Protocol (DAP) used by the client (e.g. VS Code)
 * into requests and events of the real "execution engine" or "debugger" (here: class FosRuntime).
 * When implementing your own debugger extension for VS Code, most of the work will go into the Debug Adapter.
 * Since the Debug Adapter is independent from VS Code, it can be used in any client (IDE) supporting the Debug Adapter Protocol.
 *
 * The most important class of the Debug Adapter is the FosDebugSession which implements many DAP requests by talking to the FosRuntime.
 */

import {
	Logger, logger,
	LoggingDebugSession,
	InitializedEvent, TerminatedEvent, StoppedEvent, ContinuedEvent, BreakpointEvent, OutputEvent,
	ProgressStartEvent, ProgressUpdateEvent, ProgressEndEvent, InvalidatedEvent,
	Thread, StackFrame, Scope, Source, Handles, Breakpoint, MemoryEvent
} from '@vscode/debugadapter';
import { DebugProtocol } from '@vscode/debugprotocol';
import { basename } from 'path-browserify';
import { FosRuntime, IRuntimeBreakpoint, FileAccessor, RuntimeVariable, timeout, IRuntimeVariableType } from './fosRuntime';
import { Subject } from 'await-notify';
import * as base64 from 'base64-js';
import { resolveAttachTarget } from './attachDiscovery';

/**
 * This interface describes the fos-debug specific launch attributes
 * (which are not part of the Debug Adapter Protocol).
 * The schema for these attributes lives in the package.json of the fos-debug extension.
 * The interface should always match this schema.
 */
interface ILaunchRequestArguments extends DebugProtocol.LaunchRequestArguments {
	/** An absolute path to the "program" to debug. */
	program: string;
	/** Automatically stop target after launch. If not specified, target does not stop. */
	stopOnEntry?: boolean;
	/** enable logging the Debug Adapter Protocol */
	trace?: boolean;
	/** run without debugging */
	noDebug?: boolean;
	/** if specified, results in a simulated compile error in launch. */
	compileError?: 'default' | 'show' | 'hide';
}

interface IAttachRequestArguments extends DebugProtocol.AttachRequestArguments {
	/** Process id of the target runtime. */
	processId?: string;
	/** Optional direct endpoint override (tcp://host:port). */
	endpoint?: string;
	/** UDP discovery port for debugger target lookup. */
	discoveryPort?: number;
	/** UDP discovery timeout in milliseconds. */
	discoveryTimeoutMs?: number;
	/** enable logging the Debug Adapter Protocol */
	trace?: boolean;
}

interface IAttachTransport {
	close: () => void;
	sendRequest: (command: string, payload?: any) => Promise<any>;
	setEventHandler: (handler: ((event: IAttachEventMessage) => void) | undefined) => void;
	setCloseHandler: (handler: ((reason?: string) => void) | undefined) => void;
}

interface IResolvedTransportEndpoint {
	kind: 'pipe' | 'unix' | 'tcp';
	path?: string;
	host?: string;
	port?: number;
}

function getNodeNetModule(): any {
	const localRequire = typeof require === 'function' ? require : undefined;
	if (localRequire) {
		try {
			return localRequire('net');
		}
		catch {
		}
	}

	const globalRequire = (globalThis as any)?.require;
	if (typeof globalRequire === 'function') {
		try {
			return globalRequire('net');
		}
		catch {
		}
	}

	const moduleRequire = (globalThis as any)?.module?.require;
	if (typeof moduleRequire === 'function') {
		try {
			return moduleRequire('net');
		}
		catch {
		}
	}

	try {
		const dynamicRequire = new Function('return require')();
		return dynamicRequire('net');
	}
	catch {
		return undefined;
	}
}

function parseTransportEndpoint(endpoint: string): IResolvedTransportEndpoint {
	if (endpoint.startsWith('tcp://')) {
		const hostPort = endpoint.substring('tcp://'.length);
		const separator = hostPort.lastIndexOf(':');
		if (separator <= 0 || separator >= hostPort.length - 1) {
			throw new Error(`Invalid tcp endpoint '${endpoint}'. Expected tcp://host:port.`);
		}

		const host = hostPort.substring(0, separator);
		const port = Number(hostPort.substring(separator + 1));
		if (!Number.isFinite(port) || port <= 0 || port > 65535) {
			throw new Error(`Invalid tcp endpoint port in '${endpoint}'.`);
		}

		return { kind: 'tcp', host, port };
	}

	if (endpoint.startsWith('pipe://')) {
		const rawPath = endpoint.substring('pipe://'.length);
		const normalized = rawPath.replace(/^\/+/, '');

		if (normalized.startsWith('\\\\.\\pipe\\')) {
			return { kind: 'pipe', path: normalized };
		}

		if (normalized.startsWith('./pipe/')) {
			return { kind: 'pipe', path: `\\\\.\\pipe\\${normalized.substring('./pipe/'.length)}` };
		}

		if (normalized.startsWith('pipe/')) {
			return { kind: 'pipe', path: `\\\\.\\pipe\\${normalized.substring('pipe/'.length)}` };
		}

		return { kind: 'pipe', path: `\\\\.\\pipe\\${normalized.replace(/\//g, '\\\\')}` };
	}

	if (endpoint.startsWith('unix://')) {
		const socketPath = endpoint.substring('unix://'.length);
		if (!socketPath) {
			throw new Error('Invalid unix endpoint: empty socket path.');
		}
		return { kind: 'unix', path: socketPath };
	}

	throw new Error(`Unsupported attach endpoint format '${endpoint}'.`);
}

interface IAttachHandshakeMessage {
	type: 'attachAccepted';
	protocolVersion?: number;
	enginePid?: number;
}

interface IAttachResponseMessage {
	type: 'response';
	requestId: number;
	success?: boolean;
	command?: string;
	body?: any;
}

interface IAttachEventMessage {
	type: 'event';
	event?: string;
	body?: any;
}

interface IAttachStoppedLocation {
	sourcePath?: string;
	line?: number;
	functionName?: string;
}

interface IAttachStoppedState {
	reason: string;
	text?: string;
	location?: IAttachStoppedLocation;
}

interface IAttachStackTraceFrame {
	id?: number;
	name?: string;
	source?: string;
	line?: number;
	column?: number;
}

interface IAttachStackTraceResponse {
	stackFrames?: IAttachStackTraceFrame[];
	totalFrames?: number;
}

interface IAttachVariable {
	name?: string;
	type?: string;
	value?: number | boolean | string;
}

interface IAttachVariablesResponse {
	variables?: IAttachVariable[];
}

interface IAttachScopeRef {
	scope: 'locals' | 'globals';
	frameId?: number;
}

const ATTACH_STOP_HISTORY_LIMIT = 8;

async function connectAttachTransport(endpoint: string): Promise<{ transport: IAttachTransport; handshake?: IAttachHandshakeMessage }> {
	const net = getNodeNetModule();
	if (!net) {
		throw new Error('Attach transport is only available in Node.js debug adapter runtime.');
	}

	const resolved = parseTransportEndpoint(endpoint);

	return await new Promise((resolve, reject) => {
		const socket = resolved.kind === 'tcp'
			? net.createConnection({ host: resolved.host, port: resolved.port })
			: net.createConnection({ path: resolved.path });
		let settled = false;
		let nextRequestId = 1;
		let handshakePayload: IAttachHandshakeMessage | undefined;
		let receiveBuffer = '';
		let eventHandler: ((event: IAttachEventMessage) => void) | undefined;
		let closeHandler: ((reason?: string) => void) | undefined;
		let transportClosed = false;
		const pendingRequests = new Map<number, { resolve: (value: any) => void; reject: (error: Error) => void; timeoutId: any }>();

		const notifyTransportClosed = (reason?: string) => {
			if (transportClosed) {
				return;
			}
			transportClosed = true;
			if (closeHandler) {
				closeHandler(reason);
			}
		};

		const finishResolve = () => {
			if (settled) {
				return;
			}
			settled = true;
			socket.setTimeout(0);
			resolve({
				transport: {
					close: () => {
						notifyTransportClosed('closed by client');
						for (const pending of pendingRequests.values()) {
							clearTimeout(pending.timeoutId);
							pending.reject(new Error('Attach transport closed.'));
						}
						pendingRequests.clear();
						socket.end();
						socket.destroy();
					},
					sendRequest: async (command: string, payload?: any) => {
						const requestId = nextRequestId++;
						const request = {
							type: 'request',
							id: requestId,
							command,
							payload: payload ?? {}
						};

						socket.write(`${JSON.stringify(request)}\n`);

						return await new Promise<any>((requestResolve, requestReject) => {
							const timeoutId = setTimeout(() => {
								pendingRequests.delete(requestId);
								requestReject(new Error(`Attach request '${command}' timed out.`));
							}, 2000);

							pendingRequests.set(requestId, {
								resolve: requestResolve,
								reject: requestReject,
								timeoutId
							});
						});
					}
					,
					setEventHandler: (handler: ((event: IAttachEventMessage) => void) | undefined) => {
						eventHandler = handler;
					},
					setCloseHandler: (handler: ((reason?: string) => void) | undefined) => {
						closeHandler = handler;
					}
				},
				handshake: handshakePayload
			});
		};

		const finishReject = (error: Error) => {
			if (settled) {
				return;
			}
			settled = true;
			reject(error);
		};

		socket.setTimeout(2000);

		socket.once('connect', () => {
			const hello = JSON.stringify({ type: 'attach', protocolVersion: 1, client: 'fos-vscode-debugger' });
			socket.write(`${hello}\n`);
		});

		socket.once('timeout', () => {
			finishReject(new Error(`Attach endpoint timeout (${endpoint}).`));
			socket.destroy();
		});

		socket.once('error', (error: Error) => {
			notifyTransportClosed(`socket error: ${error.message}`);
			for (const pending of pendingRequests.values()) {
				clearTimeout(pending.timeoutId);
				pending.reject(error);
			}
			pendingRequests.clear();
			finishReject(new Error(`Attach transport connect failed (${endpoint}): ${error.message}`));
		});

		socket.on('data', (data: Buffer) => {
			receiveBuffer += data.toString('utf8');

			for (;;) {
				const lineEnd = receiveBuffer.indexOf('\n');
				if (lineEnd < 0) {
					break;
				}

				const line = receiveBuffer.slice(0, lineEnd).trim();
				receiveBuffer = receiveBuffer.slice(lineEnd + 1);

				if (!line) {
					continue;
				}

				let message: any;
				try {
					message = JSON.parse(line);
				}
				catch {
					continue;
				}

				if (!settled && message.type === 'attachAccepted') {
					handshakePayload = message as IAttachHandshakeMessage;
					finishResolve();
					continue;
				}

				if (message.type === 'response') {
					const responseMessage = message as IAttachResponseMessage;
					const pending = pendingRequests.get(responseMessage.requestId);
					if (pending) {
						clearTimeout(pending.timeoutId);
						pendingRequests.delete(responseMessage.requestId);
						if (responseMessage.success === false) {
							pending.reject(new Error(`Engine command '${responseMessage.command ?? 'unknown'}' failed.`));
						}
						else {
							pending.resolve(responseMessage.body ?? {});
						}
					}
					continue;
				}

				if (message.type === 'event') {
					if (eventHandler) {
						eventHandler(message as IAttachEventMessage);
					}
				}
			}
		});

		socket.once('end', () => {
			notifyTransportClosed('closed by engine');
			for (const pending of pendingRequests.values()) {
				clearTimeout(pending.timeoutId);
				pending.reject(new Error('Attach transport closed by engine.'));
			}
			pendingRequests.clear();

			if (!settled) {
				finishReject(new Error(`Attach endpoint closed before handshake (${endpoint}).`));
			}
		});

		socket.once('close', () => {
			notifyTransportClosed('socket closed');
		});
	});
}


export class FosDebugSession extends LoggingDebugSession {

	// we don't support multiple threads, so we can use a hardcoded ID for the default thread
	private static threadID = 1;

	// a FOS runtime (or debugger)
	private _runtime: FosRuntime;
	private _fileAccessor: FileAccessor;
	private _attachTransport: IAttachTransport | undefined;
	private _isAttachedTransport = false;
	private _attachStoppedLocation: IAttachStoppedLocation | undefined;
	private _attachStoppedState: IAttachStoppedState | undefined;
	private _attachStopHistory: IAttachStoppedState[] = [];
	private _pendingAttachBreakpoints = new Map<string, number[]>();

	private _variableHandles = new Handles<'locals' | 'globals' | RuntimeVariable>();
	private _attachScopeRefs = new Map<number, IAttachScopeRef>();

	private _configurationDone = new Subject();

	private _cancellationTokens = new Map<number, boolean>();

	private _reportProgress = false;
	private _progressId = 10000;
	private _cancelledProgressId: string | undefined = undefined;
	private _isProgressCancellable = true;

	private _valuesInHex = false;
	private _useInvalidatedEvent = false;

	private _addressesInHex = true;

	private mapAttachStopReason(event: IAttachEventMessage): { reason: string; text?: string } {
		const rawReason = typeof event.body?.reason === 'string' ? event.body.reason : '';
		const rawText = typeof event.body?.text === 'string' ? event.body.text : undefined;
		const normalized = rawReason.toLowerCase();

		switch (normalized) {
			case 'pause':
			case 'step':
			case 'breakpoint':
			case 'exception':
			case 'entry':
				return { reason: normalized, text: rawText };
			case 'aborted':
				return { reason: 'exception', text: rawText ?? 'Execution of script aborted' };
			case 'error':
				return { reason: 'exception', text: rawText ?? 'Script execution error' };
			default:
				if (normalized.length > 0) {
					return { reason: 'exception', text: rawText ?? `Engine stop reason: ${rawReason}` };
				}
				return { reason: 'pause', text: rawText };
		}
	}

	private updateAttachStoppedLocation(event: IAttachEventMessage): void {
		const sourcePathRaw = event.body?.source;
		const lineRaw = event.body?.line;
		const functionRaw = event.body?.function;

		const sourcePath = typeof sourcePathRaw === 'string' && sourcePathRaw.length > 0 ? sourcePathRaw : undefined;
		const line = typeof lineRaw === 'number' ? lineRaw : undefined;
		const functionName = typeof functionRaw === 'string' && functionRaw.length > 0 ? functionRaw : undefined;

		if (sourcePath || typeof line === 'number' || functionName) {
			this._attachStoppedLocation = {
				sourcePath,
				line,
				functionName
			};
		}
	}

	private updateAttachStoppedState(event: IAttachEventMessage): void {
		const mapped = this.mapAttachStopReason(event);
		this.updateAttachStoppedLocation(event);
		this._attachStoppedState = {
			reason: mapped.reason,
			text: mapped.text,
			location: this._attachStoppedLocation
		};

		this._attachStopHistory.unshift(this._attachStoppedState);
		if (this._attachStopHistory.length > ATTACH_STOP_HISTORY_LIMIT) {
			this._attachStopHistory.length = ATTACH_STOP_HISTORY_LIMIT;
		}
	}

	private async flushPendingAttachBreakpoints(): Promise<void> {
		if (!this._isAttachedTransport || !this._attachTransport) {
			return;
		}

		for (const [path, clientLines] of this._pendingAttachBreakpoints) {
			try {
				const debuggerLines = clientLines.map(line => this.convertClientLineToDebugger(line));
				await this._attachTransport.sendRequest('setBreakpoints', {
					source: path,
					lines: debuggerLines,
				});
			}
			catch (error) {
				this.sendEvent(new OutputEvent(`Set breakpoints sync failed for ${path}: ${(error as Error).message}\n`, 'console'));
			}
		}
	}

	private getAttachStopForUi(): IAttachStoppedState | undefined {
		return this._attachStoppedState ?? (this._attachStopHistory.length > 0 ? this._attachStopHistory[0] : undefined);
	}

	private buildAttachScopeVariables(scope: 'locals' | 'globals'): RuntimeVariable[] {
		const stop = this.getAttachStopForUi();
		const location = stop?.location;

		if (scope === 'locals') {
			const vars: RuntimeVariable[] = [
				new RuntimeVariable('reason', stop?.reason ?? 'running'),
				new RuntimeVariable('function', location?.functionName ?? ''),
				new RuntimeVariable('line', typeof location?.line === 'number' ? this.convertDebuggerLineToClient(location.line) : 0),
			];

			if (stop?.text) {
				vars.push(new RuntimeVariable('message', stop.text));
			}

			return vars;
		}

		return [
			new RuntimeVariable('attached', true),
			new RuntimeVariable('threadId', FosDebugSession.threadID),
			new RuntimeVariable('source', location?.sourcePath ?? ''),
			new RuntimeVariable('historySize', this._attachStopHistory.length),
		];
	}

	/**
	 * Creates a new debug adapter that is used for one debug session.
	 * We configure the default implementation of a debug adapter here.
	 */
	public constructor(fileAccessor: FileAccessor) {
		super("fos-debug.txt");
		this._fileAccessor = fileAccessor;

		// this debugger uses zero-based lines and columns
		this.setDebuggerLinesStartAt1(false);
		this.setDebuggerColumnsStartAt1(false);

		this._runtime = new FosRuntime(fileAccessor);

		// setup event handlers
		this._runtime.on('stopOnEntry', () => {
			this.sendEvent(new StoppedEvent('entry', FosDebugSession.threadID));
		});
		this._runtime.on('stopOnStep', () => {
			this.sendEvent(new StoppedEvent('step', FosDebugSession.threadID));
		});
		this._runtime.on('stopOnBreakpoint', () => {
			this.sendEvent(new StoppedEvent('breakpoint', FosDebugSession.threadID));
		});
		this._runtime.on('stopOnDataBreakpoint', () => {
			this.sendEvent(new StoppedEvent('data breakpoint', FosDebugSession.threadID));
		});
		this._runtime.on('stopOnInstructionBreakpoint', () => {
			this.sendEvent(new StoppedEvent('instruction breakpoint', FosDebugSession.threadID));
		});
		this._runtime.on('stopOnException', (exception) => {
			if (exception) {
				this.sendEvent(new StoppedEvent(`exception(${exception})`, FosDebugSession.threadID));
			} else {
				this.sendEvent(new StoppedEvent('exception', FosDebugSession.threadID));
			}
		});
		this._runtime.on('breakpointValidated', (bp: IRuntimeBreakpoint) => {
			this.sendEvent(new BreakpointEvent('changed', { verified: bp.verified, id: bp.id } as DebugProtocol.Breakpoint));
		});
		this._runtime.on('output', (type, text, filePath, line, column) => {

			let category: string;
			switch(type) {
				case 'prio': category = 'important'; break;
				case 'out': category = 'stdout'; break;
				case 'err': category = 'stderr'; break;
				default: category = 'console'; break;
			}
			const e: DebugProtocol.OutputEvent = new OutputEvent(`${text}\n`, category);

			if (text === 'start' || text === 'startCollapsed' || text === 'end') {
				e.body.group = text;
				e.body.output = `group-${text}\n`;
			}

			e.body.source = this.createSource(filePath);
			e.body.line = this.convertDebuggerLineToClient(line);
			e.body.column = this.convertDebuggerColumnToClient(column);
			this.sendEvent(e);
		});
		this._runtime.on('end', () => {
			this.sendEvent(new TerminatedEvent());
		});
	}

	/**
	 * The 'initialize' request is the first request called by the frontend
	 * to interrogate the features the debug adapter provides.
	 */
	protected initializeRequest(response: DebugProtocol.InitializeResponse, args: DebugProtocol.InitializeRequestArguments): void {

		if (args.supportsProgressReporting) {
			this._reportProgress = true;
		}
		if (args.supportsInvalidatedEvent) {
			this._useInvalidatedEvent = true;
		}

		// build and return the capabilities of this debug adapter:
		response.body = response.body || {};

		// the adapter implements the configurationDone request.
		response.body.supportsConfigurationDoneRequest = true;

		// make VS Code use 'evaluate' when hovering over source
		response.body.supportsEvaluateForHovers = true;

		// don't show reverse debugging controls
		response.body.supportsStepBack = false;

		// don't expose restart request from adapter
		response.body.supportsRestartRequest = false;

		// make VS Code support data breakpoints
		response.body.supportsDataBreakpoints = true;

		// make VS Code support completion in REPL
		response.body.supportsCompletionsRequest = true;
		response.body.completionTriggerCharacters = [ ".", "[" ];

		// make VS Code send cancel request
		response.body.supportsCancelRequest = true;

		// make VS Code send the breakpointLocations request
		response.body.supportsBreakpointLocationsRequest = true;

		// make VS Code provide "Step in Target" functionality
		response.body.supportsStepInTargetsRequest = true;

		// the adapter defines two exceptions filters, one with support for conditions.
		response.body.supportsExceptionFilterOptions = true;
		response.body.exceptionBreakpointFilters = [
			{
				filter: 'namedException',
				label: "Named Exception",
				description: `Break on named exceptions. Enter the exception's name as the Condition.`,
				default: false,
				supportsCondition: true,
				conditionDescription: `Enter the exception's name`
			},
			{
				filter: 'otherExceptions',
				label: "Other Exceptions",
				description: 'This is a other exception',
				default: true,
				supportsCondition: false
			}
		];

		// make VS Code send exceptionInfo request
		response.body.supportsExceptionInfoRequest = true;

		// make VS Code send setVariable request
		response.body.supportsSetVariable = true;

		// make VS Code send setExpression request
		response.body.supportsSetExpression = true;

		// make VS Code send disassemble request
		response.body.supportsDisassembleRequest = true;
		response.body.supportsSteppingGranularity = true;
		response.body.supportsInstructionBreakpoints = true;

		// make VS Code able to read and write variable memory
		response.body.supportsReadMemoryRequest = true;
		response.body.supportsWriteMemoryRequest = true;

		response.body.supportSuspendDebuggee = true;
		response.body.supportTerminateDebuggee = true;
		response.body.supportsFunctionBreakpoints = true;
		response.body.supportsDelayedStackTraceLoading = true;

		this.sendResponse(response);

		// since this debug adapter can accept configuration requests like 'setBreakpoint' at any time,
		// we request them early by sending an 'initializeRequest' to the frontend.
		// The frontend will end the configuration sequence by calling 'configurationDone' request.
		this.sendEvent(new InitializedEvent());
	}

	/**
	 * Called at the end of the configuration sequence.
	 * Indicates that all breakpoints etc. have been sent to the DA and that the 'launch' can start.
	 */
	protected configurationDoneRequest(response: DebugProtocol.ConfigurationDoneResponse, args: DebugProtocol.ConfigurationDoneArguments): void {
		super.configurationDoneRequest(response, args);

		// notify the launchRequest that configuration has finished
		this._configurationDone.notify();
	}

	protected disconnectRequest(response: DebugProtocol.DisconnectResponse, args: DebugProtocol.DisconnectArguments, request?: DebugProtocol.Request): void {
		console.log(`disconnectRequest suspend: ${args.suspendDebuggee}, terminate: ${args.terminateDebuggee}`);
		if (this._attachTransport) {
			this._attachTransport.setCloseHandler(undefined);
			this._attachTransport.setEventHandler(undefined);
			this._attachTransport.sendRequest('disconnect').catch(() => undefined);
			this._attachTransport.close();
			this._attachTransport = undefined;
		}
		this._isAttachedTransport = false;
		this._attachStoppedLocation = undefined;
		this._attachStoppedState = undefined;
		this._attachStopHistory = [];
		this._pendingAttachBreakpoints.clear();
		this._attachScopeRefs.clear();
		this.sendResponse(response);
	}

	protected async attachRequest(response: DebugProtocol.AttachResponse, args: IAttachRequestArguments) {
		logger.setup(args.trace ? Logger.LogLevel.Verbose : Logger.LogLevel.Stop, false);

		try {
			const target = await resolveAttachTarget(args, async (path: string) => {
				const data = await this._fileAccessor.readFile(path);
				return new TextDecoder().decode(data);
			});

			const { transport, handshake } = await connectAttachTransport(target.endpoint);
			this._attachTransport = transport;
			this._attachStoppedLocation = undefined;
			this._attachStoppedState = undefined;
			this._attachStopHistory = [];
			this._attachTransport.setCloseHandler((reason?: string) => {
				if (!this._isAttachedTransport) {
					return;
				}

				this._isAttachedTransport = false;
				this._attachStoppedLocation = undefined;
				this._attachStoppedState = undefined;
				this._attachStopHistory = [];
				this._attachTransport = undefined;

				if (reason) {
					this.sendEvent(new OutputEvent(`Attach transport closed: ${reason}\n`, 'console'));
				}

				this.sendEvent(new TerminatedEvent());
			});
			this._attachTransport.setEventHandler((event: IAttachEventMessage) => {
				if (event.event === 'stopped') {
					this.updateAttachStoppedState(event);
					const mapped = this._attachStoppedState;
					if (!mapped) {
						this.sendEvent(new StoppedEvent('pause', FosDebugSession.threadID));
						return;
					}
					this.sendEvent(new StoppedEvent(mapped.reason, FosDebugSession.threadID, mapped.text));
					return;
				}

				if (event.event === 'paused') {
					this.sendEvent(new StoppedEvent('pause', FosDebugSession.threadID));
					return;
				}

				if (event.event === 'resumed') {
					this._attachStoppedState = undefined;
					this.sendEvent(new ContinuedEvent(FosDebugSession.threadID, true));
				}
			});
			const capabilities = await this._attachTransport.sendRequest('capabilities');
			this._isAttachedTransport = true;
			await this.flushPendingAttachBreakpoints();

			this.sendResponse(response);
			this.sendEvent(new OutputEvent(`Attached to ${target.targetName ?? 'AngelScript runtime'} (pid=${target.processId}) via ${target.endpoint}\n`, 'console'));
			if (handshake) {
				this.sendEvent(new OutputEvent(`Attach handshake: ${JSON.stringify(handshake)}\n`, 'console'));
			}
			this.sendEvent(new OutputEvent(`Attach capabilities: ${JSON.stringify(capabilities)}\n`, 'console'));
			if (capabilities?.isPaused === true) {
				this.sendEvent(new StoppedEvent('pause', FosDebugSession.threadID));
			}
		}
		catch (error) {
			this.sendErrorResponse(response, {
				id: 2000,
				format: (error as Error).message
			});
		}
	}

	protected async launchRequest(response: DebugProtocol.LaunchResponse, args: ILaunchRequestArguments) {

		// make sure to 'Stop' the buffered logging if 'trace' is not set
		logger.setup(args.trace ? Logger.LogLevel.Verbose : Logger.LogLevel.Stop, false);

		// wait 1 second until configuration has finished (and configurationDoneRequest has been called)
		await this._configurationDone.wait(1000);

		// start the program in the runtime
		await this._runtime.start(args.program, !!args.stopOnEntry, !args.noDebug);

		if (args.compileError) {
			// simulate a compile/build error in "launch" request:
			// the error should not result in a modal dialog since 'showUser' is set to false.
			// A missing 'showUser' should result in a modal dialog.
			this.sendErrorResponse(response, {
				id: 1001,
				format: `compile error: some fake error.`,
				showUser: args.compileError === 'show' ? true : (args.compileError === 'hide' ? false : undefined)
			});
		} else {
			this.sendResponse(response);
		}
	}

	protected setFunctionBreakPointsRequest(response: DebugProtocol.SetFunctionBreakpointsResponse, args: DebugProtocol.SetFunctionBreakpointsArguments, request?: DebugProtocol.Request): void {
		this.sendResponse(response);
	}

	protected async setBreakPointsRequest(response: DebugProtocol.SetBreakpointsResponse, args: DebugProtocol.SetBreakpointsArguments): Promise<void> {

		const path = args.source.path as string;
		const clientLines = args.lines || [];

		if (path) {
			this._pendingAttachBreakpoints.set(path, [...clientLines]);
		}

		if (this._isAttachedTransport && this._attachTransport) {
			try {
				const debuggerLines = clientLines.map(l => this.convertClientLineToDebugger(l));
				const engineResponse = await this._attachTransport.sendRequest('setBreakpoints', {
					source: path,
					lines: debuggerLines
				});

				const responseBreakpoints = Array.isArray(engineResponse?.breakpoints) ? engineResponse.breakpoints : [];
				response.body = {
					breakpoints: responseBreakpoints.map((bp: any, index: number) => {
						const debuggerLine = typeof bp?.line === 'number' ? bp.line : debuggerLines[index] ?? 0;
						const verified = bp?.verified !== false;
						const dapBp = new Breakpoint(verified, this.convertDebuggerLineToClient(debuggerLine)) as DebugProtocol.Breakpoint;
						dapBp.id = index + 1;
						return dapBp;
					})
				};
				this.sendResponse(response);
			}
			catch (error) {
				this.sendErrorResponse(response, {
					id: 2007,
					format: `Set breakpoints failed: ${(error as Error).message}`
				});
			}
			return;
		}

		// clear all breakpoints for this file
		this._runtime.clearBreakpoints(path);

		// set and verify breakpoint locations
		const actualBreakpoints0 = clientLines.map(async l => {
			const { verified, line, id } = await this._runtime.setBreakPoint(path, this.convertClientLineToDebugger(l));
			const bp = new Breakpoint(verified, this.convertDebuggerLineToClient(line)) as DebugProtocol.Breakpoint;
			bp.id = id;
			return bp;
		});
		const actualBreakpoints = await Promise.all<DebugProtocol.Breakpoint>(actualBreakpoints0);

		// send back the actual breakpoint positions
		response.body = {
			breakpoints: actualBreakpoints
		};
		this.sendResponse(response);
	}

	protected breakpointLocationsRequest(response: DebugProtocol.BreakpointLocationsResponse, args: DebugProtocol.BreakpointLocationsArguments, request?: DebugProtocol.Request): void {

		if (args.source.path) {
			const bps = this._runtime.getBreakpoints(args.source.path, this.convertClientLineToDebugger(args.line));
			response.body = {
				breakpoints: bps.map(col => {
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

	protected async setExceptionBreakPointsRequest(response: DebugProtocol.SetExceptionBreakpointsResponse, args: DebugProtocol.SetExceptionBreakpointsArguments): Promise<void> {

		let namedException: string | undefined = undefined;
		let otherExceptions = false;

		if (args.filterOptions) {
			for (const filterOption of args.filterOptions) {
				switch (filterOption.filterId) {
					case 'namedException':
						namedException = args.filterOptions[0].condition;
						break;
					case 'otherExceptions':
						otherExceptions = true;
						break;
				}
			}
		}

		if (args.filters) {
			if (args.filters.indexOf('otherExceptions') >= 0) {
				otherExceptions = true;
			}
		}

		this._runtime.setExceptionsFilters(namedException, otherExceptions);

		this.sendResponse(response);
	}

	protected exceptionInfoRequest(response: DebugProtocol.ExceptionInfoResponse, args: DebugProtocol.ExceptionInfoArguments) {
		if (this._isAttachedTransport) {
			const stop = this._attachStoppedState ?? this._attachStopHistory.find(s => s.reason === 'exception');
			if (stop?.reason === 'exception') {
				const location = stop.location;
				const sourcePart = location?.sourcePath ? `${location.sourcePath}` : 'unknown-source';
				const linePart = typeof location?.line === 'number' ? `${this.convertDebuggerLineToClient(location.line)}` : '?';
				const functionPart = location?.functionName ?? 'unknown-function';

				response.body = {
					exceptionId: 'AngelScript.Exception',
					description: stop.text ?? 'AngelScript runtime exception.',
					breakMode: 'always',
					details: {
						message: stop.text ?? 'AngelScript runtime exception.',
						typeName: 'AngelScriptException',
						stackTrace: `${functionPart} at ${sourcePart}:${linePart}`,
					},
				};
				this.sendResponse(response);
				return;
			}

			response.body = {
				exceptionId: 'NoException',
				description: 'Current stop is not an exception.',
				breakMode: 'never',
			};
			this.sendResponse(response);
			return;
		}

		response.body = {
			exceptionId: 'Exception ID',
			description: 'This is a descriptive description of the exception.',
			breakMode: 'always',
			details: {
				message: 'Message contained in the exception.',
				typeName: 'Short type name of the exception object',
				stackTrace: 'stack frame 1\nstack frame 2',
			}
		};
		this.sendResponse(response);
	}

	protected threadsRequest(response: DebugProtocol.ThreadsResponse): void {
		if (this._isAttachedTransport) {
			response.body = {
				threads: [new Thread(FosDebugSession.threadID, 'attach-thread')]
			};
			this.sendResponse(response);
			return;
		}

		// runtime supports no threads so just return a default thread.
		response.body = {
			threads: [
				new Thread(FosDebugSession.threadID, "thread 1"),
				new Thread(FosDebugSession.threadID + 1, "thread 2"),
			]
		};
		this.sendResponse(response);
	}

	protected async stackTraceRequest(response: DebugProtocol.StackTraceResponse, args: DebugProtocol.StackTraceArguments): Promise<void> {
		if (this._isAttachedTransport) {
			const startFrame = typeof args.startFrame === 'number' ? args.startFrame : 0;
			const maxLevels = typeof args.levels === 'number' ? args.levels : 1000;

			if (this._attachTransport) {
				try {
					const payload = await this._attachTransport.sendRequest('stackTrace', {
						startFrame,
						levels: maxLevels,
					}) as IAttachStackTraceResponse;

					const rawFrames = Array.isArray(payload?.stackFrames) ? payload.stackFrames : [];
					const stackFrames = rawFrames.map((frame, ix) => {
						const frameId = typeof frame.id === 'number' ? frame.id : (startFrame + ix + 1);
						const frameName = typeof frame.name === 'string' && frame.name.length > 0 ? frame.name : 'attach-frame';
						const source = typeof frame.source === 'string' && frame.source.length > 0 ? this.createSource(frame.source) : undefined;
						const line = typeof frame.line === 'number' ? this.convertDebuggerLineToClient(frame.line) : 1;
						const sf = new StackFrame(frameId, frameName, source, line);

						if (typeof frame.column === 'number') {
							sf.column = this.convertDebuggerColumnToClient(frame.column);
						}

						return sf;
					});

					response.body = {
						stackFrames,
						totalFrames: typeof payload?.totalFrames === 'number' ? payload.totalFrames : stackFrames.length,
					};
					this.sendResponse(response);
					return;
				}
				catch {
					// fallback to single stopped-location frame below
				}
			}

			const historyLocation = this._attachStopHistory.length > 0 ? this._attachStopHistory[0].location : undefined;
			const location = this._attachStoppedLocation ?? historyLocation;
			const hasFrame = !!location;
			const source = location?.sourcePath ? this.createSource(location.sourcePath) : undefined;
			const line = typeof location?.line === 'number' ? this.convertDebuggerLineToClient(location.line) : 1;
			const frameName = location?.functionName ?? 'attach-frame';

			const frames = hasFrame ? [new StackFrame(1, frameName, source, line)] : [];
			const sliced = frames.slice(startFrame, startFrame + maxLevels);

			response.body = {
				stackFrames: sliced,
				totalFrames: frames.length
			};
			this.sendResponse(response);
			return;
		}

		const startFrame = typeof args.startFrame === 'number' ? args.startFrame : 0;
		const maxLevels = typeof args.levels === 'number' ? args.levels : 1000;
		const endFrame = startFrame + maxLevels;

		const stk = this._runtime.stack(startFrame, endFrame);

		response.body = {
			stackFrames: stk.frames.map((f, ix) => {
				const sf: DebugProtocol.StackFrame = new StackFrame(f.index, f.name, this.createSource(f.file), this.convertDebuggerLineToClient(f.line));
				if (typeof f.column === 'number') {
					sf.column = this.convertDebuggerColumnToClient(f.column);
				}
				if (typeof f.instruction === 'number') {
					const address = this.formatAddress(f.instruction);
					sf.name = `${f.name} ${address}`;
					sf.instructionPointerReference = address;
				}

				return sf;
			}),
			// 4 options for 'totalFrames':
			//omit totalFrames property: 	// VS Code has to probe/guess. Should result in a max. of two requests
			totalFrames: stk.count			// stk.count is the correct size, should result in a max. of two requests
			//totalFrames: 1000000 			// not the correct size, should result in a max. of two requests
			//totalFrames: endFrame + 20 	// dynamically increases the size with every requested chunk, results in paging
		};
		this.sendResponse(response);
	}

	protected scopesRequest(response: DebugProtocol.ScopesResponse, args: DebugProtocol.ScopesArguments): void {
		if (this._isAttachedTransport) {
			const localsRef = this._variableHandles.create('locals');
			this._attachScopeRefs.set(localsRef, { scope: 'locals', frameId: args.frameId });
			const globalsRef = this._variableHandles.create('globals');
			this._attachScopeRefs.set(globalsRef, { scope: 'globals' });

			response.body = {
				scopes: [
					new Scope('Locals', localsRef, false),
					new Scope('Globals', globalsRef, true)
				]
			};
			this.sendResponse(response);
			return;
		}

		response.body = {
			scopes: [
				new Scope("Locals", this._variableHandles.create('locals'), false),
				new Scope("Globals", this._variableHandles.create('globals'), true)
			]
		};
		this.sendResponse(response);
	}

	protected async writeMemoryRequest(response: DebugProtocol.WriteMemoryResponse, { data, memoryReference, offset = 0 }: DebugProtocol.WriteMemoryArguments) {
		if (this._isAttachedTransport) {
			this.sendErrorResponse(response, {
				id: 2009,
				format: 'Writing memory is not supported in attach transport mode yet.'
			});
			return;
		}

		const variable = this._variableHandles.get(Number(memoryReference));
		if (typeof variable === 'object') {
			const decoded = base64.toByteArray(data);
			variable.setMemory(decoded, offset);
			response.body = { bytesWritten: decoded.length };
		} else {
			response.body = { bytesWritten: 0 };
		}

		this.sendResponse(response);
		this.sendEvent(new InvalidatedEvent(['variables']));
	}

	protected async readMemoryRequest(response: DebugProtocol.ReadMemoryResponse, { offset = 0, count, memoryReference }: DebugProtocol.ReadMemoryArguments) {
		const variable = this._variableHandles.get(Number(memoryReference));
		if (typeof variable === 'object' && variable.memory) {
			const memory = variable.memory.subarray(
				Math.min(offset, variable.memory.length),
				Math.min(offset + count, variable.memory.length),
			);

			response.body = {
				address: offset.toString(),
				data: base64.fromByteArray(memory),
				unreadableBytes: count - memory.length
			};
		} else {
			response.body = {
				address: offset.toString(),
				data: '',
				unreadableBytes: count
			};
		}

		this.sendResponse(response);
	}

	protected async variablesRequest(response: DebugProtocol.VariablesResponse, args: DebugProtocol.VariablesArguments, request?: DebugProtocol.Request): Promise<void> {

		let vs: RuntimeVariable[] = [];
		const attachScopeRef = this._attachScopeRefs.get(args.variablesReference);

		const v = this._variableHandles.get(args.variablesReference);
		if (this._isAttachedTransport && this._attachTransport && attachScopeRef) {
			if (attachScopeRef.scope === 'locals') {
				try {
					const payload = await this._attachTransport.sendRequest('variables', {
						frameId: typeof attachScopeRef.frameId === 'number' ? attachScopeRef.frameId : 1,
					}) as IAttachVariablesResponse;

					const rawVars = Array.isArray(payload?.variables) ? payload.variables : [];
					vs = rawVars.map(item => {
						const name = typeof item?.name === 'string' && item.name.length > 0 ? item.name : 'var';
						const value = item?.value;

						if (typeof value === 'number' || typeof value === 'boolean' || typeof value === 'string') {
							return new RuntimeVariable(name, value);
						}

						return new RuntimeVariable(name, '');
					});
				}
				catch {
					vs = this.buildAttachScopeVariables('locals');
				}
			}
			else {
				vs = this.buildAttachScopeVariables('globals');
			}
		} else if (v === 'locals') {
			vs = this._isAttachedTransport ? this.buildAttachScopeVariables('locals') : this._runtime.getLocalVariables();
		} else if (v === 'globals') {
			if (this._isAttachedTransport) {
				vs = this.buildAttachScopeVariables('globals');
			}
			else
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
			variables: vs.map(v => this.convertFromRuntime(v))
		};
		this.sendResponse(response);
	}

	protected setVariableRequest(response: DebugProtocol.SetVariableResponse, args: DebugProtocol.SetVariableArguments): void {
		if (this._isAttachedTransport) {
			this.sendErrorResponse(response, {
				id: 2008,
				format: 'Setting variables is not supported in attach transport mode yet.'
			});
			return;
		}

		const container = this._variableHandles.get(args.variablesReference);
		const rv = container === 'locals'
			? this._runtime.getLocalVariable(args.name)
			: container instanceof RuntimeVariable && container.value instanceof Array
			? container.value.find(v => v.name === args.name)
			: undefined;

		if (rv) {
			rv.value = this.convertToRuntime(args.value);
			response.body = this.convertFromRuntime(rv);

			if (rv.memory && rv.reference) {
				this.sendEvent(new MemoryEvent(String(rv.reference), 0, rv.memory.length));
			}
		}

		this.sendResponse(response);
	}

	protected continueRequest(response: DebugProtocol.ContinueResponse, args: DebugProtocol.ContinueArguments): void {
		if (this._isAttachedTransport && this._attachTransport) {
			this._attachTransport.sendRequest('continue')
				.then(() => this.sendResponse(response))
				.catch((error: Error) => {
					this.sendErrorResponse(response, {
						id: 2002,
						format: `Continue failed: ${error.message}`
					});
				});
			return;
		}

		this._runtime.continue(false);
		this.sendResponse(response);
	}

	protected pauseRequest(response: DebugProtocol.PauseResponse, args: DebugProtocol.PauseArguments): void {
		if (this._isAttachedTransport && this._attachTransport) {
			this._attachTransport.sendRequest('pause')
				.then(() => this.sendResponse(response))
				.catch((error: Error) => {
					this.sendErrorResponse(response, {
						id: 2003,
						format: `Pause failed: ${error.message}`
					});
				});
			return;
		}

		this.sendResponse(response);
	}

	protected reverseContinueRequest(response: DebugProtocol.ReverseContinueResponse, args: DebugProtocol.ReverseContinueArguments): void {
		this._runtime.continue(true);
		this.sendResponse(response);
 	}

	protected nextRequest(response: DebugProtocol.NextResponse, args: DebugProtocol.NextArguments): void {
		if (this._isAttachedTransport && this._attachTransport) {
			this._attachTransport.sendRequest('next')
				.then(() => this.sendResponse(response))
				.catch((error: Error) => {
					this.sendErrorResponse(response, {
						id: 2004,
						format: `Next failed: ${error.message}`
					});
				});
			return;
		}

		this._runtime.step(args.granularity === 'instruction', false);
		this.sendResponse(response);
	}

	protected stepBackRequest(response: DebugProtocol.StepBackResponse, args: DebugProtocol.StepBackArguments): void {
		this._runtime.step(args.granularity === 'instruction', true);
		this.sendResponse(response);
	}

	protected stepInTargetsRequest(response: DebugProtocol.StepInTargetsResponse, args: DebugProtocol.StepInTargetsArguments) {
		const targets = this._runtime.getStepInTargets(args.frameId);
		response.body = {
			targets: targets.map(t => {
				return { id: t.id, label: t.label };
			})
		};
		this.sendResponse(response);
	}

	protected stepInRequest(response: DebugProtocol.StepInResponse, args: DebugProtocol.StepInArguments): void {
		if (this._isAttachedTransport && this._attachTransport) {
			this._attachTransport.sendRequest('stepIn')
				.then(() => this.sendResponse(response))
				.catch((error: Error) => {
					this.sendErrorResponse(response, {
						id: 2005,
						format: `Step in failed: ${error.message}`
					});
				});
			return;
		}

		this._runtime.stepIn(args.targetId);
		this.sendResponse(response);
	}

	protected stepOutRequest(response: DebugProtocol.StepOutResponse, args: DebugProtocol.StepOutArguments): void {
		if (this._isAttachedTransport && this._attachTransport) {
			this._attachTransport.sendRequest('stepOut')
				.then(() => this.sendResponse(response))
				.catch((error: Error) => {
					this.sendErrorResponse(response, {
						id: 2006,
						format: `Step out failed: ${error.message}`
					});
				});
			return;
		}

		this._runtime.stepOut();
		this.sendResponse(response);
	}

	protected async evaluateRequest(response: DebugProtocol.EvaluateResponse, args: DebugProtocol.EvaluateArguments): Promise<void> {
		if (this._isAttachedTransport && args.context === 'repl') {
			const isMutatingReplCommand = /^(new|del)\s+[0-9]+$/.test(args.expression.trim());
			if (isMutatingReplCommand) {
				this.sendErrorResponse(response, {
					id: 2010,
					format: 'REPL breakpoint mutation is not supported in attach transport mode yet.'
				});
				return;
			}
		}

		let reply: string | undefined;
		let rv: RuntimeVariable | undefined;

		switch (args.context) {
			case 'repl':
				// handle some REPL commands:
				// 'evaluate' supports to create and delete breakpoints from the 'repl':
				const matches = /new +([0-9]+)/.exec(args.expression);
				if (matches && matches.length === 2) {
					const mbp = await this._runtime.setBreakPoint(this._runtime.sourceFile, this.convertClientLineToDebugger(parseInt(matches[1])));
					const bp = new Breakpoint(mbp.verified, this.convertDebuggerLineToClient(mbp.line), undefined, this.createSource(this._runtime.sourceFile)) as DebugProtocol.Breakpoint;
					bp.id= mbp.id;
					this.sendEvent(new BreakpointEvent('new', bp));
					reply = `breakpoint created`;
				} else {
					const matches = /del +([0-9]+)/.exec(args.expression);
					if (matches && matches.length === 2) {
						const mbp = this._runtime.clearBreakPoint(this._runtime.sourceFile, this.convertClientLineToDebugger(parseInt(matches[1])));
						if (mbp) {
							const bp = new Breakpoint(false) as DebugProtocol.Breakpoint;
							bp.id= mbp.id;
							this.sendEvent(new BreakpointEvent('removed', bp));
							reply = `breakpoint deleted`;
						}
					} else {
						const matches = /progress/.exec(args.expression);
						if (matches && matches.length === 1) {
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
				if (args.expression.startsWith('$')) {
					rv = this._runtime.getLocalVariable(args.expression.substr(1));
				} else {
					rv = new RuntimeVariable('eval', this.convertToRuntime(args.expression));
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

	protected setExpressionRequest(response: DebugProtocol.SetExpressionResponse, args: DebugProtocol.SetExpressionArguments): void {
		if (this._isAttachedTransport) {
			this.sendErrorResponse(response, {
				id: 2011,
				format: 'Setting expressions is not supported in attach transport mode yet.'
			});
			return;
		}

		if (args.expression.startsWith('$')) {
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

	private async progressSequence() {

		const ID = '' + this._progressId++;

		await timeout(100);

		const title = this._isProgressCancellable ? 'Cancellable operation' : 'Long running operation';
		const startEvent: DebugProtocol.ProgressStartEvent = new ProgressStartEvent(ID, title);
		startEvent.body.cancellable = this._isProgressCancellable;
		this._isProgressCancellable = !this._isProgressCancellable;
		this.sendEvent(startEvent);
		this.sendEvent(new OutputEvent(`start progress: ${ID}\n`));

		let endMessage = 'progress ended';

		for (let i = 0; i < 100; i++) {
			await timeout(500);
			this.sendEvent(new ProgressUpdateEvent(ID, `progress: ${i}`));
			if (this._cancelledProgressId === ID) {
				endMessage = 'progress cancelled';
				this._cancelledProgressId = undefined;
				this.sendEvent(new OutputEvent(`cancel progress: ${ID}\n`));
				break;
			}
		}
		this.sendEvent(new ProgressEndEvent(ID, endMessage));
		this.sendEvent(new OutputEvent(`end progress: ${ID}\n`));

		this._cancelledProgressId = undefined;
	}

	protected dataBreakpointInfoRequest(response: DebugProtocol.DataBreakpointInfoResponse, args: DebugProtocol.DataBreakpointInfoArguments): void {

		response.body = {
            dataId: null,
            description: "cannot break on data access",
            accessTypes: undefined,
            canPersist: false
        };

		if (args.variablesReference && args.name) {
			const v = this._variableHandles.get(args.variablesReference);
			if (v === 'globals') {
				response.body.dataId = args.name;
				response.body.description = args.name;
				response.body.accessTypes = [ "write" ];
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

	protected setDataBreakpointsRequest(response: DebugProtocol.SetDataBreakpointsResponse, args: DebugProtocol.SetDataBreakpointsArguments): void {

		// clear all data breakpoints
		this._runtime.clearAllDataBreakpoints();

		response.body = {
			breakpoints: []
		};

		for (const dbp of args.breakpoints) {
			const ok = this._runtime.setDataBreakpoint(dbp.dataId, dbp.accessType || 'write');
			response.body.breakpoints.push({
				verified: ok
			});
		}

		this.sendResponse(response);
	}

	protected completionsRequest(response: DebugProtocol.CompletionsResponse, args: DebugProtocol.CompletionsArguments): void {

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

	protected cancelRequest(response: DebugProtocol.CancelResponse, args: DebugProtocol.CancelArguments) {
		if (args.requestId) {
			this._cancellationTokens.set(args.requestId, true);
		}
		if (args.progressId) {
			this._cancelledProgressId= args.progressId;
		}
	}

	protected disassembleRequest(response: DebugProtocol.DisassembleResponse, args: DebugProtocol.DisassembleArguments) {
		const memoryInt = args.memoryReference.slice(3);
		const baseAddress = parseInt(memoryInt);
		const offset = args.instructionOffset || 0;
		const count = args.instructionCount;

		const isHex = memoryInt.startsWith('0x');
		const pad = isHex ? memoryInt.length-2 : memoryInt.length;

		const loc = this.createSource(this._runtime.sourceFile);

		let lastLine = -1;

		const instructions = this._runtime.disassemble(baseAddress+offset, count).map(instruction => {
			let address = Math.abs(instruction.address).toString(isHex ? 16 : 10).padStart(pad, '0');
			const sign = instruction.address < 0 ? '-' : '';
			const instr : DebugProtocol.DisassembledInstruction = {
				address: sign + (isHex ? `0x${address}` : `${address}`),
				instruction: instruction.instruction
			};
			// if instruction's source starts on a new line add the source to instruction
			if (instruction.line !== undefined && lastLine !== instruction.line) {
				lastLine = instruction.line;
				instr.location = loc;
				instr.line = this.convertDebuggerLineToClient(instruction.line);
			}
			return instr;
		});

		response.body = {
			instructions: instructions
		};
		this.sendResponse(response);
	}

	protected setInstructionBreakpointsRequest(response: DebugProtocol.SetInstructionBreakpointsResponse, args: DebugProtocol.SetInstructionBreakpointsArguments) {

		// clear all instruction breakpoints
		this._runtime.clearInstructionBreakpoints();

		// set instruction breakpoints
		const breakpoints = args.breakpoints.map(ibp => {
			const address = parseInt(ibp.instructionReference.slice(3));
			const offset = ibp.offset || 0;
			return <DebugProtocol.Breakpoint>{
				verified: this._runtime.setInstructionBreakpoint(address + offset)
			};
		});

		response.body = {
			breakpoints: breakpoints
		};
		this.sendResponse(response);
	}

	protected customRequest(command: string, response: DebugProtocol.Response, args: any) {
		if (command === 'toggleFormatting') {
			this._valuesInHex = ! this._valuesInHex;
			if (this._useInvalidatedEvent) {
				this.sendEvent(new InvalidatedEvent( ['variables'] ));
			}
			this.sendResponse(response);
		} else {
			super.customRequest(command, response, args);
		}
	}

	//---- helpers

	private convertToRuntime(value: string): IRuntimeVariableType {

		value= value.trim();

		if (value === 'true') {
			return true;
		}
		if (value === 'false') {
			return false;
		}
		if (value[0] === '\'' || value[0] === '"') {
			return value.substr(1, value.length-2);
		}
		const n = parseFloat(value);
		if (!isNaN(n)) {
			return n;
		}
		return value;
	}

	private convertFromRuntime(v: RuntimeVariable): DebugProtocol.Variable {

		let dapVariable: DebugProtocol.Variable = {
			name: v.name,
			value: '???',
			type: typeof v.value,
			variablesReference: 0,
			evaluateName: '$' + v.name
		};

		if (v.name.indexOf('lazy') >= 0) {
			// a "lazy" variable needs an additional click to retrieve its value

			dapVariable.value = 'lazy var';		// placeholder value
			v.reference ??= this._variableHandles.create(new RuntimeVariable('', [ new RuntimeVariable('', v.value) ]));
			dapVariable.variablesReference = v.reference;
			dapVariable.presentationHint = { lazy: true };
		} else {

			if (Array.isArray(v.value)) {
				dapVariable.value = 'Object';
				v.reference ??= this._variableHandles.create(v);
				dapVariable.variablesReference = v.reference;
			} else {

				switch (typeof v.value) {
					case 'number':
						if (Math.round(v.value) === v.value) {
							dapVariable.value = this.formatNumber(v.value);
							(<any>dapVariable).__vscodeVariableMenuContext = 'simple';	// enable context menu contribution
							dapVariable.type = 'integer';
						} else {
							dapVariable.value = v.value.toString();
							dapVariable.type = 'float';
						}
						break;
					case 'string':
						dapVariable.value = `"${v.value}"`;
						break;
					case 'boolean':
						dapVariable.value = v.value ? 'true' : 'false';
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

	private formatAddress(x: number, pad = 8) {
		return 'mem' + (this._addressesInHex ? '0x' + x.toString(16).padStart(8, '0') : x.toString(10));
	}

	private formatNumber(x: number) {
		return this._valuesInHex ? '0x' + x.toString(16) : x.toString(10);
	}

	private createSource(filePath: string): Source {
		return new Source(basename(filePath), this.convertDebuggerPathToClient(filePath), undefined, undefined, 'fos-adapter-data');
	}
}

