"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const events_1 = require("events");
class MessageError extends Error {
    constructor(error) {
        super(error.message);
        this._code = error.code || 0;
        this._data = error.data || null;
    }
    get code() {
        return this._code;
    }
    get data() {
        return this._data;
    }
}
exports.MessageError = MessageError;
/**
 * Creates a RPC Client.
 * It is intentional that Client does not create a WebSocket object since we prefer composability
 * The Client can be used to communicate over processes, http or anything that can send and receive strings
 * It just needs to pass in an object that implements LikeSocket interface
 */
class Client extends events_1.EventEmitter {
    constructor(socket, opts) {
        super();
        this._responsePromiseMap = new Map();
        this._nextMessageId = 0;
        this._connected = false;
        this._emitLog = false;
        this._consoleLog = false;
        this._requestQueue = [];
        this.setLogging(opts);
        if (!socket) {
            throw new TypeError('socket cannot be undefined or null');
        }
        this._socket = socket;
        socket.on('open', () => {
            this._connected = true;
            this._sendQueuedRequests();
        });
        socket.on('message', (message) => this.processMessage(message));
    }
    processMessage(messageStr) {
        this._logMessage(messageStr, 'receive');
        let message;
        // Ensure JSON is not malformed
        try {
            message = JSON.parse(messageStr);
        }
        catch (e) {
            return this.emit('error', e);
        }
        // Check that messages is well formed
        if (!message) {
            this.emit('error', new Error(`Message cannot be null, empty or undefined`));
        }
        else if (message.id) {
            if (this._responsePromiseMap.has(message.id)) {
                // Resolve promise from pending message
                const promise = this._responsePromiseMap.get(message.id);
                if (message.result) {
                    promise.resolve(message.result);
                }
                else if (message.error) {
                    promise.reject(new MessageError(message.error));
                }
                else {
                    this.emit('error', new Error(`Response must have result or error: ${messageStr}`));
                }
            }
            else {
                this.emit('error', new Error(`Response with id:${message.id} has no pending request`));
            }
        }
        else if (message.method) {
            // Server has sent a notification
            this.emit(message.method, message.params);
        }
        else {
            this.emit('error', new Error(`Invalid message: ${messageStr}`));
        }
    }
    /** Set logging for all received and sent messages */
    setLogging({ logEmit, logConsole } = {}) {
        this._emitLog = logEmit;
        this._consoleLog = logConsole;
    }
    _send(message) {
        this._requestQueue.push(JSON.stringify(message));
        this._sendQueuedRequests();
    }
    _sendQueuedRequests() {
        if (this._connected) {
            for (let messageStr of this._requestQueue) {
                this._logMessage(messageStr, 'send');
                this._socket.send(messageStr);
            }
            this._requestQueue = [];
        }
    }
    _logMessage(message, direction) {
        if (this._consoleLog) {
            console.log(`Client ${direction === 'send' ? '>' : '<'}`, message);
        }
        if (this._emitLog) {
            this.emit(direction, message);
        }
    }
    call(method, params) {
        const id = ++this._nextMessageId;
        const message = { id, method, params };
        return new Promise((resolve, reject) => {
            try {
                this._send(message);
                this._responsePromiseMap.set(id, { resolve, reject });
            }
            catch (error) {
                return reject(error);
            }
        });
    }
    notify(method, params) {
        this._send({ method, params });
    }
    /**
     * Builds an ES6 Proxy where api.domain.method(params) transates into client.send('{domain}.{method}', params) calls
     * api.domain.on{method} will add event handlers for {method} events
     * api.domain.emit{method} will send {method} notifications to the server
     * The api object leads itself to a very clean interface i.e `await api.Domain.func(params)` calls
     * This allows the consumer to abstract all the internal details of marshalling the message from function call to a string
     * Calling client.api('') will return an unprefixed client. e.g api.hello() is equivalient to client.send('hello')
     */
    api(prefix) {
        if (!Proxy) {
            throw new Error('api() requires ES6 Proxy. Please use an ES6 compatible engine');
        }
        return new Proxy({}, {
            get: (target, prop) => {
                if (target[prop]) {
                    return target[prop];
                }
                // Special handling for prototype so console intellisense works on noice objects
                if (prop === '__proto__' || prop === 'prototype') {
                    return Object.prototype;
                }
                else if (prefix === void 0) { // Prefix is undefined. Create domain prefix
                    target[prop] = this.api(`${prop}.`);
                }
                else if (prop === 'on') {
                    target[prop] = (method, handler) => this.on(`${prefix}${method}`, handler);
                }
                else if (prop === 'emit') {
                    target[prop] = (method, params) => this.notify(`${prefix}${method}`, params);
                }
                else if (prop.substr(0, 2) === 'on' && prop.length > 3) { // TODO: deprecate this
                    const method = prop[2].toLowerCase() + prop.substr(3);
                    target[prop] = (handler) => this.on(`${prefix}${method}`, handler);
                }
                else if (prop.substr(0, 4) === 'emit' && prop.length > 5) { // TODO: deprecate this
                    const method = prop[4].toLowerCase() + prop.substr(5);
                    target[prop] = (params) => this.notify(`${prefix}${method}`, params);
                }
                else {
                    const method = prop;
                    target[prop] = (params) => this.call(`${prefix}${method}`, params);
                }
                return target[prop];
            }
        });
    }
}
exports.Client = Client;
/**
 * Creates a RPC Server.
 * It is intentional that Server does not create a WebSocketServer object since we prefer composability
 * The Server can be used to communicate over processes, http or anything that can send and receive strings
 * It just needs to pass in an object that implements LikeSocketServer interface
 */
class Server extends events_1.EventEmitter {
    constructor(server, opts) {
        super();
        this._exposedMethodsMap = new Map();
        this._emitLog = false;
        this._consoleLog = false;
        this.setLogging(opts);
        if (!server) {
            throw new TypeError('server cannot be undefined or null');
        }
        this._socketServer = server;
        server.on('connection', (socket) => {
            socket.on('message', (message) => this.processMessage(message, socket));
        });
    }
    processMessage(messageStr, socket) {
        this._logMessage(messageStr, 'receive');
        let request;
        // Ensure JSON is not malformed
        try {
            request = JSON.parse(messageStr);
        }
        catch (e) {
            return this._sendError(socket, request, -32700 /* ParseError */);
        }
        // Ensure method is atleast defined
        if (request && request.method && typeof request.method === 'string') {
            if (request.id && typeof request.id === 'number') {
                const handler = this._exposedMethodsMap.get(request.method);
                // Handler is defined so lets call it
                if (handler) {
                    try {
                        const result = handler.call(null, request.params);
                        if (result instanceof Promise) {
                            // Result is a promise, so lets wait for the result and handle accordingly
                            result.then((actualResult) => {
                                this._send(socket, { id: request.id, result: actualResult || {} });
                            }).catch((error) => {
                                this._sendError(socket, request, -32603 /* InternalError */, error);
                            });
                        }
                        else {
                            // Result is not a promise so send immediately
                            this._send(socket, { id: request.id, result: result || {} });
                        }
                    }
                    catch (error) {
                        this._sendError(socket, request, -32603 /* InternalError */, error);
                    }
                }
                else {
                    this._sendError(socket, request, -32601 /* MethodNotFound */);
                }
            }
            else {
                // Message is a notification, so just emit
                this.emit(request.method, request.params);
            }
        }
        else {
            // No method property, send InvalidRequest error
            this._sendError(socket, request, -32600 /* InvalidRequest */);
        }
    }
    /** Set logging for all received and sent messages */
    setLogging({ logEmit, logConsole } = {}) {
        this._emitLog = logEmit;
        this._consoleLog = logConsole;
    }
    _logMessage(messageStr, direction) {
        if (this._consoleLog) {
            console.log(`Server ${direction === 'send' ? '>' : '<'}`, messageStr);
        }
        if (this._emitLog) {
            this.emit(direction, messageStr);
        }
    }
    _send(socket, message) {
        const messageStr = JSON.stringify(message);
        this._logMessage(messageStr, 'send');
        socket.send(messageStr);
    }
    _sendError(socket, request, errorCode, error) {
        try {
            this._send(socket, {
                id: request && request.id || -1,
                error: this._errorFromCode(errorCode, error && error.message || error, request && request.method)
            });
        }
        catch (error) {
            // Since we can't even send errors, do nothing. The connection was probably closed.
        }
    }
    _errorFromCode(code, data, method) {
        let message = '';
        switch (code) {
            case -32603 /* InternalError */:
                message = `InternalError: Internal Error when calling '${method}'`;
                break;
            case -32601 /* MethodNotFound */:
                message = `MethodNotFound: '${method}' wasn't found`;
                break;
            case -32600 /* InvalidRequest */:
                message = 'InvalidRequest: JSON sent is not a valid request object';
                break;
            case -32700 /* ParseError */:
                message = 'ParseError: invalid JSON received';
                break;
        }
        return { code, message, data };
    }
    expose(method, handler) {
        this._exposedMethodsMap.set(method, handler);
    }
    notify(method, params) {
        // Broadcast message to all clients
        if (this._socketServer.clients) {
            for (let ws of this._socketServer.clients) {
                this._send(ws, { method, params });
            }
        }
        else {
            throw new Error('SocketServer does not support broadcasting. No "clients: LikeSocket[]" property found');
        }
    }
    /**
     * Builds an ES6 Proxy where api.domain.expose(module) exposes all the functions in the module over RPC
     * api.domain.emit{method} calls will send {method} notifications to the client
     * The api object leads itself to a very clean interface i.e `await api.Domain.func(params)` calls
     * This allows the consumer to abstract all the internal details of marshalling the message from function call to a string
     */
    api(prefix) {
        if (!Proxy) {
            throw new Error('api() requires ES6 Proxy. Please use an ES6 compatible engine');
        }
        return new Proxy({}, {
            get: (target, prop) => {
                if (target[prop]) {
                    return target[prop];
                }
                if (prop === '__proto__' || prop === 'prototype') {
                    return Object.prototype;
                }
                else if (prefix === void 0) {
                    target[prop] = this.api(`${prop}.`);
                }
                else if (prop === 'on') {
                    target[prop] = (method, handler) => this.on(`${prefix}${method}`, handler);
                }
                else if (prop === 'emit') {
                    target[prop] = (method, params) => this.notify(`${prefix}${method}`, params);
                }
                else if (prop.substr(0, 2) === 'on' && prop.length > 3) { // TODO deprecate this
                    const method = prop[2].toLowerCase() + prop.substr(3);
                    target[prop] = (handler) => this.on(`${prefix}${method}`, handler);
                }
                else if (prop.substr(0, 4) === 'emit' && prop.length > 5) { // TODO deprecate this
                    const method = prop[4].toLowerCase() + prop.substr(5);
                    target[prop] = (params) => this.notify(`${prefix}${method}`, params);
                }
                else if (prop === 'expose') {
                    target[prop] = (module) => {
                        if (!module || typeof module !== 'object') {
                            throw new Error('Expected an iterable object to expose functions');
                        }
                        for (let funcName in module) {
                            if (typeof module[funcName] === 'function') {
                                this.expose(`${prefix}${funcName}`, module[funcName].bind(module));
                            }
                        }
                    };
                }
                else {
                    return undefined;
                }
                return target[prop];
            }
        });
    }
}
exports.Server = Server;
