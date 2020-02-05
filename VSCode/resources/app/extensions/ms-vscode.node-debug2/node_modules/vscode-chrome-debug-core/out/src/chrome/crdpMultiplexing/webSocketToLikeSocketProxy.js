"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const vscode_debugadapter_1 = require("vscode-debugadapter");
const WebSocket = require("ws");
class WebSocketToLikeSocketProxy {
    constructor(_port, _socket) {
        this._port = _port;
        this._socket = _socket;
        this._currentlyOpenedWebSocket = null;
    }
    start() {
        this._server = new WebSocket.Server({ port: this._port }, () => {
            vscode_debugadapter_1.logger.log(`CRDP Proxy listening on: ${this._port}`);
        });
        this._socket.on('close', () => {
            vscode_debugadapter_1.logger.log('CRDP Proxy shutting down');
            this._server.close(() => {
                if (this._currentlyOpenedWebSocket !== null) {
                    this._currentlyOpenedWebSocket.close();
                    vscode_debugadapter_1.logger.log('CRDP Proxy succesfully shut down');
                }
                return {};
            });
        });
        this._server.on('connection', openedWebSocket => {
            if (this._currentlyOpenedWebSocket !== null) {
                openedWebSocket.close();
                throw Error(`CRDP Proxy: Only one websocket is supported by the server on port ${this._port}`);
            }
            else {
                this._currentlyOpenedWebSocket = openedWebSocket;
                vscode_debugadapter_1.logger.log(`CRDP Proxy accepted a new connection`);
            }
            openedWebSocket.on('message', data => {
                vscode_debugadapter_1.logger.log(`CRDP Proxy - Client to Target: ${data}`);
                this._socket.send(data.toString());
            });
            openedWebSocket.on('close', () => {
                vscode_debugadapter_1.logger.log('CRDP Proxy - Client closed the connection');
                this._currentlyOpenedWebSocket = null;
            });
            this._socket.on('message', data => {
                vscode_debugadapter_1.logger.log(`CRDP Proxy - Target to Client: ${data}`);
                openedWebSocket.send(data);
            });
        });
    }
}
exports.WebSocketToLikeSocketProxy = WebSocketToLikeSocketProxy;

//# sourceMappingURL=webSocketToLikeSocketProxy.js.map
