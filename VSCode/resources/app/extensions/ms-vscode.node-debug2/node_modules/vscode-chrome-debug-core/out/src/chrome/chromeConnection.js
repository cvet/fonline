"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const WebSocket = require("ws");
const telemetry_1 = require("../telemetry");
const executionTimingsReporter_1 = require("../executionTimingsReporter");
const errors = require("../errors");
const utils = require("../utils");
const vscode_debugadapter_1 = require("vscode-debugadapter");
const chromeTargetDiscoveryStrategy_1 = require("./chromeTargetDiscoveryStrategy");
const noice_json_rpc_1 = require("noice-json-rpc");
const crdpMultiplexor_1 = require("./crdpMultiplexing/crdpMultiplexor");
const webSocketToLikeSocketProxy_1 = require("./crdpMultiplexing/webSocketToLikeSocketProxy");
/**
 * A subclass of WebSocket that logs all traffic
 */
class LoggingSocket extends WebSocket {
    constructor(address, protocols, options) {
        super(address, protocols, options);
        this.on('error', e => {
            vscode_debugadapter_1.logger.log('Websocket error: ' + e.toString());
        });
        this.on('close', () => {
            vscode_debugadapter_1.logger.log('Websocket closed');
        });
        this.on('message', msgStr => {
            let msgObj;
            try {
                msgObj = JSON.parse(msgStr.toString());
            }
            catch (e) {
                vscode_debugadapter_1.logger.error(`Invalid JSON from target: (${e.message}): ${msgStr}`);
                return;
            }
            if (msgObj && !(msgObj.method && msgObj.method.startsWith('Network.'))) {
                // Not really the right place to examine the content of the message, but don't log annoying Network activity notifications.
                if ((msgObj.result && msgObj.result.scriptSource)) {
                    // If this message contains the source of a script, we log everything but the source
                    msgObj.result.scriptSource = '<removed script source for logs>';
                    vscode_debugadapter_1.logger.verbose('← From target: ' + JSON.stringify(msgObj));
                }
                else {
                    vscode_debugadapter_1.logger.verbose('← From target: ' + msgStr);
                }
            }
        });
    }
    send(data, opts, cb) {
        const msgStr = JSON.stringify(data);
        if (this.readyState !== WebSocket.OPEN) {
            vscode_debugadapter_1.logger.log(`→ Warning: Target not open! Message: ${msgStr}`);
            return;
        }
        super.send.apply(this, arguments);
        vscode_debugadapter_1.logger.verbose('→ To target: ' + msgStr);
    }
}
/**
 * Connects to a target supporting the Chrome Debug Protocol and sends and receives messages
 */
class ChromeConnection {
    constructor(targetDiscovery, targetFilter) {
        this._targetFilter = targetFilter;
        this._targetDiscoveryStrategy = targetDiscovery || new chromeTargetDiscoveryStrategy_1.ChromeTargetDiscovery(vscode_debugadapter_1.logger, telemetry_1.telemetry);
        this.events = new executionTimingsReporter_1.StepProgressEventsEmitter([this._targetDiscoveryStrategy.events]);
    }
    get isAttached() { return !!this._client; }
    get api() {
        return this._client && this._client.api();
    }
    get attachedTarget() {
        return this._attachedTarget;
    }
    setTargetFilter(targetFilter) {
        this._targetFilter = targetFilter;
    }
    /**
     * Attach the websocket to the first available tab in the chrome instance with the given remote debugging port number.
     */
    attach(address = '127.0.0.1', port = 9222, targetUrl, timeout, extraCRDPChannelPort) {
        return this._attach(address, port, targetUrl, timeout, extraCRDPChannelPort)
            .then(() => { });
    }
    attachToWebsocketUrl(wsUrl, extraCRDPChannelPort) {
        /* __GDPR__FRAGMENT__
           "StepNames" : {
              "Attach.AttachToTargetDebuggerWebsocket" : { "classification": "SystemMetaData", "purpose": "FeatureInsight" }
           }
         */
        this.events.emitStepStarted('Attach.AttachToTargetDebuggerWebsocket');
        this._socket = new LoggingSocket(wsUrl, undefined, { headers: { Host: 'localhost' } });
        if (extraCRDPChannelPort) {
            this._crdpSocketMultiplexor = new crdpMultiplexor_1.CRDPMultiplexor(this._socket);
            new webSocketToLikeSocketProxy_1.WebSocketToLikeSocketProxy(extraCRDPChannelPort, this._crdpSocketMultiplexor.addChannel('extraCRDPEndpoint')).start();
            this._client = new noice_json_rpc_1.Client(this._crdpSocketMultiplexor.addChannel('debugger'));
        }
        else {
            this._client = new noice_json_rpc_1.Client(this._socket);
        }
        this._client.on('error', e => vscode_debugadapter_1.logger.error('Error handling message from target: ' + e.message));
    }
    getAllTargets(address = '127.0.0.1', port = 9222, targetFilter, targetUrl) {
        return this._targetDiscoveryStrategy.getAllTargets(address, port, targetFilter, targetUrl);
    }
    _attach(address, port, targetUrl, timeout = ChromeConnection.ATTACH_TIMEOUT, extraCRDPChannelPort) {
        let selectedTarget;
        return utils.retryAsync(() => this._targetDiscoveryStrategy.getTarget(address, port, this._targetFilter, targetUrl), timeout, /*intervalDelay=*/ 200)
            .catch(err => Promise.reject(errors.runtimeConnectionTimeout(timeout, err.message)))
            .then(target => {
            selectedTarget = target;
            return this.attachToWebsocketUrl(target.webSocketDebuggerUrl, extraCRDPChannelPort);
        }).then(() => {
            this._attachedTarget = selectedTarget;
        });
    }
    run() {
        // This is a CDP version difference which will have to be handled more elegantly with others later...
        // For now, we need to send both messages and ignore a failing one.
        return Promise.all([
            this.api.Runtime.runIfWaitingForDebugger(),
            this.api.Runtime.run()
        ])
            .then(() => { }, () => { });
    }
    close() {
        this._socket.close();
    }
    onClose(handler) {
        this._socket.on('close', handler);
    }
    get version() {
        return this._attachedTarget.version
            .then(version => {
            return (version) ? version : new chromeTargetDiscoveryStrategy_1.TargetVersions(chromeTargetDiscoveryStrategy_1.Version.unknownVersion(), chromeTargetDiscoveryStrategy_1.Version.unknownVersion());
        });
    }
}
ChromeConnection.ATTACH_TIMEOUT = 10000; // ms
exports.ChromeConnection = ChromeConnection;

//# sourceMappingURL=chromeConnection.js.map
