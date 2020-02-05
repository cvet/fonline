"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const vscode_debugadapter_1 = require("vscode-debugadapter");
/* Rational: The debug adapter only exposes the debugging capabilities of the Chrome Debugging Protocol.
   We want to be able to connect other components to expose other of the capabilities to the users.
   We are trying to do this by creating a multiplexor that will take one connection to a Chrome Debugging Protocol websocket,
   and simulate from that that we have actually many independent connections to several Chrome Debugging Protocol websockets.
   Given that implementing truly independent connections is not trivial, we are choosing to implement only the features that we
   seem to need so far to support a component for an external Console, and an external DOM Explorer.

   The way we are going to make that work is that we'll pass the actual websocket to chrome/node to the Multiplexor, and then we are going
   to requests two channels from it. One will be the debugger channel, that will be used by the traditional debug adapter for everything it does.
   Another channel will be the extraCRDPEndpoint channel that will be offered thorugh the client using a websocket port (so it'll simulate that
   this is an actual connection to chrome or node), so we can connect any utilities that we want there.

   In the future, if we need to, it's easy to modify this multiplexor to support more than 2 channels.
*/
/* Assumptions made to implement this multiplexor:
    1. The message IDs of CRDP don't need to be sent in order
    2. The Domain.enable messages don't have any side-effects (We might send them multiple times)
    3. The clients are ready to recieve domain messages when they send the Domain.enable message (A better design would be to assume that they are ready after we've sent the response for that message, but this approach seems to be working so far)
    4. The clients never disable any Domain
    5. The clients enable all domains in the first 60 seconds after they've connected
 */
function extractDomain(method) {
    const methodParts = method.split('.');
    if (methodParts.length === 2) {
        return methodParts[0];
    }
    else {
        throwCriticalError(`The method ${method} didn't have exactly two parts`);
        return 'Unknown';
    }
}
function encodifyChannel(channelId, id) {
    return id * 10 + channelId;
}
function decodifyChannelId(encodifiedId) {
    return encodifiedId % 10;
}
function decodifyId(encodifiedId) {
    return Math.floor(encodifiedId / 10);
}
function throwCriticalError(message) {
    vscode_debugadapter_1.logger.error('CRDP Multiplexor - CRITICAL-ERROR: ' + message);
    throw new Error(message);
}
class CRDPMultiplexor {
    constructor(_wrappedLikeSocket) {
        this._wrappedLikeSocket = _wrappedLikeSocket;
        this._channels = [];
        this._wrappedLikeSocket.on('message', data => this.onMessage(data));
    }
    onMessage(data) {
        const message = JSON.parse(data);
        if (message.id !== undefined) {
            this.onResponseMessage(message, data);
        }
        else if (message.method) {
            this.onDomainNotification(message, data);
        }
        else {
            throwCriticalError(`Message didn't have id nor method: ${data}`);
        }
    }
    onResponseMessage(message, data) {
        // The message is a response, so it should only go to the channel that requested this
        const channel = this._channels[decodifyChannelId(message.id)];
        if (channel) {
            message.id = decodifyId(message.id);
            data = JSON.stringify(message);
            channel.callMessageCallbacks(data);
        }
        else {
            throwCriticalError(`Didn't find channel for message with id: ${message.id} and data: <${data}>`);
        }
    }
    onDomainNotification(message, data) {
        // The message is a notification, so it should go to all channels. The channels itself will filter based on the enabled domains
        const domain = extractDomain(message.method);
        for (const channel of this._channels) {
            channel.callDomainMessageCallbacks(domain, data);
        }
    }
    addChannel(channelName) {
        if (this._channels.length >= 10) {
            throw new Error(`Only 10 channels are supported`);
        }
        const channel = new CRDPChannel(channelName, this._channels.length, this);
        this._channels.push(channel);
        return channel;
    }
    send(channel, data) {
        const message = JSON.parse(data);
        if (message.id !== undefined) {
            message.id = encodifyChannel(channel.id, message.id);
            data = JSON.stringify(message);
        }
        else {
            throwCriticalError(`Channel [${channel.name}] sent a message without an id: ${data}`);
        }
        this._wrappedLikeSocket.send(data);
    }
    addListenerOfNonMultiplexedEvent(event, cb) {
        this._wrappedLikeSocket.on(event, cb);
    }
    removeListenerOfNonMultiplexedEvent(event, cb) {
        this._wrappedLikeSocket.removeListener(event, cb);
    }
}
exports.CRDPMultiplexor = CRDPMultiplexor;
class CRDPChannel {
    constructor(name, id, _multiplexor) {
        this.name = name;
        this.id = id;
        this._multiplexor = _multiplexor;
        this._messageCallbacks = [];
        this._enabledDomains = {};
        this._pendingMessagesForDomain = {};
    }
    callMessageCallbacks(messageData) {
        this._messageCallbacks.forEach(callback => callback(messageData));
    }
    callDomainMessageCallbacks(domain, messageData) {
        if (this._enabledDomains[domain]) {
            this.callMessageCallbacks(messageData);
        }
        else if (this._pendingMessagesForDomain !== null) {
            // We give clients 60 seconds after they connect to the channel to enable domains and receive all messages
            this.storeMessageForLater(domain, messageData);
        }
    }
    storeMessageForLater(domain, messageData) {
        let messagesForDomain = this._pendingMessagesForDomain[domain];
        if (messagesForDomain === undefined) {
            this._pendingMessagesForDomain[domain] = [];
            messagesForDomain = this._pendingMessagesForDomain[domain];
        }
        // Usually this is too much logging, but we might use it while debugging
        // logger.log(`CRDP Multiplexor - Storing message to channel ${this.name} for ${domain} for later: ${messageData}`);
        messagesForDomain.push(messageData);
    }
    send(messageData) {
        const message = JSON.parse(messageData);
        const method = message.method;
        const isEnableMethod = method && method.endsWith('.enable');
        let domain;
        if (isEnableMethod) {
            domain = extractDomain(method);
            this._enabledDomains[domain] = true;
        }
        this._multiplexor.send(this, messageData);
        if (isEnableMethod) {
            this.sendUnsentPendingMessages(domain);
        }
    }
    sendUnsentPendingMessages(domain) {
        if (this._pendingMessagesForDomain !== null) {
            const pendingMessagesData = this._pendingMessagesForDomain[domain];
            if (pendingMessagesData !== undefined && this._messageCallbacks.length) {
                vscode_debugadapter_1.logger.log(`CRDP Multiplexor - Sending pending messages of domain ${domain}(Count = ${pendingMessagesData.length})`);
                delete this._pendingMessagesForDomain[domain];
                pendingMessagesData.forEach(pendingMessageData => {
                    this.callDomainMessageCallbacks(domain, pendingMessageData);
                });
            }
        }
    }
    discardUnsentPendingMessages() {
        vscode_debugadapter_1.logger.log(`CRDP Multiplexor - Discarding unsent pending messages for domains: ${Object.keys(this._pendingMessagesForDomain).join(', ')}`);
        this._pendingMessagesForDomain = null;
    }
    on(event, cb) {
        if (event === 'message') {
            if (this._messageCallbacks.length === 0) {
                setTimeout(() => this.discardUnsentPendingMessages(), CRDPChannel.timeToPreserveMessagesInMillis);
            }
            this._messageCallbacks.push(cb);
        }
        else {
            this._multiplexor.addListenerOfNonMultiplexedEvent(event, cb);
        }
    }
    removeListener(event, cb) {
        if (event === 'message') {
            const index = this._messageCallbacks.indexOf(cb);
            this._messageCallbacks.splice(index, 1);
        }
        else {
            this._multiplexor.removeListenerOfNonMultiplexedEvent(event, cb);
        }
    }
}
CRDPChannel.timeToPreserveMessagesInMillis = 60 * 1000;
exports.CRDPChannel = CRDPChannel;

//# sourceMappingURL=crdpMultiplexor.js.map
