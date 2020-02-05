"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
var diagnostic_channel_1 = require("diagnostic-channel");
var mongodbPatchFunction = function (originalMongo) {
    var listener = originalMongo.instrument({
        operationIdGenerator: {
            next: function () {
                return diagnostic_channel_1.channel.bindToContext(function (cb) { return cb(); });
            },
        },
    });
    var eventMap = {};
    listener.on("started", function (event) {
        if (eventMap[event.requestId]) {
            // Note: Mongo can generate 2 completely separate requests
            // which share the same requestId, if a certain race condition is triggered.
            // For now, we accept that this can happen and potentially miss or mislabel some events.
            return;
        }
        eventMap[event.requestId] = event;
    });
    listener.on("succeeded", function (event) {
        var startedData = eventMap[event.requestId];
        if (startedData) {
            delete eventMap[event.requestId];
        }
        event.operationId(function () { return diagnostic_channel_1.channel.publish("mongodb", { startedData: startedData, event: event, succeeded: true }); });
    });
    listener.on("failed", function (event) {
        var startedData = eventMap[event.requestId];
        if (startedData) {
            delete eventMap[event.requestId];
        }
        event.operationId(function () { return diagnostic_channel_1.channel.publish("mongodb", { startedData: startedData, event: event, succeeded: false }); });
    });
    return originalMongo;
};
var mongodb3PatchFunction = function (originalMongo) {
    var listener = originalMongo.instrument();
    var eventMap = {};
    var contextMap = {};
    listener.on("started", function (event) {
        if (eventMap[event.requestId]) {
            // Note: Mongo can generate 2 completely separate requests
            // which share the same requestId, if a certain race condition is triggered.
            // For now, we accept that this can happen and potentially miss or mislabel some events.
            return;
        }
        contextMap[event.requestId] = diagnostic_channel_1.channel.bindToContext(function (cb) { return cb(); });
        eventMap[event.requestId] = event;
    });
    listener.on("succeeded", function (event) {
        var startedData = eventMap[event.requestId];
        if (startedData) {
            delete eventMap[event.requestId];
        }
        if (typeof event === "object" && typeof contextMap[event.requestId] === "function") {
            contextMap[event.requestId](function () { return diagnostic_channel_1.channel.publish("mongodb", { startedData: startedData, event: event, succeeded: true }); });
            delete contextMap[event.requestId];
        }
    });
    listener.on("failed", function (event) {
        var startedData = eventMap[event.requestId];
        if (startedData) {
            delete eventMap[event.requestId];
        }
        if (typeof event === "object" && typeof contextMap[event.requestId] === "function") {
            contextMap[event.requestId](function () { return diagnostic_channel_1.channel.publish("mongodb", { startedData: startedData, event: event, succeeded: false }); });
            delete contextMap[event.requestId];
        }
    });
    return originalMongo;
};
// In mongodb 3.3.0, mongodb-core was merged into mongodb, so the same patching
// can be used here. this.s.pool was changed to this.s.coreTopology.s.pool
var mongodbcorePatchFunction = function (originalMongo) {
    var originalConnect = originalMongo.Server.prototype.connect;
    originalMongo.Server.prototype.connect = function contextPreservingConnect() {
        var ret = originalConnect.apply(this, arguments);
        // Messages sent to mongo progress through a pool
        // This can result in context getting mixed between different responses
        // so we wrap the callbacks to restore appropriate state
        var originalWrite = this.s.coreTopology.s.pool.write;
        this.s.coreTopology.s.pool.write = function contextPreservingWrite() {
            var cbidx = typeof arguments[1] === "function" ? 1 : 2;
            if (typeof arguments[cbidx] === "function") {
                arguments[cbidx] = diagnostic_channel_1.channel.bindToContext(arguments[cbidx]);
            }
            return originalWrite.apply(this, arguments);
        };
        // Logout is a special case, it doesn't call the write function but instead
        // directly calls into connection.write
        var originalLogout = this.s.coreTopology.s.pool.logout;
        this.s.coreTopology.s.pool.logout = function contextPreservingLogout() {
            if (typeof arguments[1] === "function") {
                arguments[1] = diagnostic_channel_1.channel.bindToContext(arguments[1]);
            }
            return originalLogout.apply(this, arguments);
        };
        return ret;
    };
    return originalMongo;
};
var mongodb330PatchFunction = function (originalMongo) {
    mongodbcorePatchFunction(originalMongo); // apply mongodb-core patches
    var listener = originalMongo.instrument();
    var eventMap = {};
    var contextMap = {};
    listener.on("started", function (event) {
        if (eventMap[event.requestId]) {
            // Note: Mongo can generate 2 completely separate requests
            // which share the same requestId, if a certain race condition is triggered.
            // For now, we accept that this can happen and potentially miss or mislabel some events.
            return;
        }
        contextMap[event.requestId] = diagnostic_channel_1.channel.bindToContext(function (cb) { return cb(); });
        eventMap[event.requestId] = event;
    });
    listener.on("succeeded", function (event) {
        var startedData = eventMap[event.requestId];
        if (startedData) {
            delete eventMap[event.requestId];
        }
        if (typeof event === "object" && typeof contextMap[event.requestId] === "function") {
            contextMap[event.requestId](function () { return diagnostic_channel_1.channel.publish("mongodb", { startedData: startedData, event: event, succeeded: true }); });
            delete contextMap[event.requestId];
        }
    });
    listener.on("failed", function (event) {
        var startedData = eventMap[event.requestId];
        if (startedData) {
            delete eventMap[event.requestId];
        }
        if (typeof event === "object" && typeof contextMap[event.requestId] === "function") {
            contextMap[event.requestId](function () { return diagnostic_channel_1.channel.publish("mongodb", { startedData: startedData, event: event, succeeded: false }); });
            delete contextMap[event.requestId];
        }
    });
    return originalMongo;
};
exports.mongo2 = {
    versionSpecifier: ">= 2.0.0 <= 3.0.5",
    patch: mongodbPatchFunction,
};
exports.mongo3 = {
    versionSpecifier: "> 3.0.5 < 3.3.0",
    patch: mongodb3PatchFunction,
};
exports.mongo330 = {
    versionSpecifier: ">= 3.3.0 < 4.0.0",
    patch: mongodb330PatchFunction,
};
function enable() {
    diagnostic_channel_1.channel.registerMonkeyPatch("mongodb", exports.mongo2);
    diagnostic_channel_1.channel.registerMonkeyPatch("mongodb", exports.mongo3);
    diagnostic_channel_1.channel.registerMonkeyPatch("mongodb", exports.mongo330);
}
exports.enable = enable;
//# sourceMappingURL=mongodb.pub.js.map