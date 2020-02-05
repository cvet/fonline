"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
var diagnostic_channel_1 = require("diagnostic-channel");
var events_1 = require("events");
function postgres6PatchFunction(originalPg, originalPgPath) {
    var originalClientQuery = originalPg.Client.prototype.query;
    var diagnosticOriginalFunc = "__diagnosticOriginalFunc";
    // wherever the callback is passed, find it, save it, and remove it from the call
    // to the the original .query() function
    originalPg.Client.prototype.query = function query(config, values, callback) {
        var data = {
            query: {},
            database: {
                host: this.connectionParameters.host,
                port: this.connectionParameters.port,
            },
            result: null,
            error: null,
            duration: 0,
        };
        var start = process.hrtime();
        var queryResult;
        function patchCallback(cb) {
            if (cb && cb[diagnosticOriginalFunc]) {
                cb = cb[diagnosticOriginalFunc];
            }
            var trackingCallback = diagnostic_channel_1.channel.bindToContext(function (err, res) {
                var end = process.hrtime(start);
                data.result = res && { rowCount: res.rowCount, command: res.command };
                data.error = err;
                data.duration = Math.ceil((end[0] * 1e3) + (end[1] / 1e6));
                diagnostic_channel_1.channel.publish("postgres", data);
                // emulate weird internal behavior in pg@6
                // on success, the callback is called *before* query events are emitted
                // on failure, the callback is called *instead of* the query emitting events
                // with no events, that means no promises (since the promise is resolved/rejected in an event handler)
                // since we are always inserting ourselves as a callback, we have to restore the original
                // behavior if the user didn't provide one themselves
                if (err) {
                    if (cb) {
                        return cb.apply(this, arguments);
                    }
                    else if (queryResult && queryResult instanceof events_1.EventEmitter) {
                        queryResult.emit("error", err);
                    }
                }
                else if (cb) {
                    cb.apply(this, arguments);
                }
            });
            try {
                Object.defineProperty(trackingCallback, diagnosticOriginalFunc, { value: cb });
                return trackingCallback;
            }
            catch (e) {
                // this should never happen, but bailout in case it does
                return cb;
            }
        }
        // this function takes too many variations of arguments.
        // this patches any provided callback or creates a new callback if one wasn't provided.
        // since the callback is always called (if provided) in addition to always having a Promisified
        // EventEmitter returned (well, sometimes -- see above), its safe to insert a callback if none was given
        try {
            if (typeof config === "string") {
                if (values instanceof Array) {
                    data.query.preparable = {
                        text: config,
                        args: values,
                    };
                    callback = patchCallback(callback);
                }
                else {
                    data.query.text = config;
                    // pg v6 will, for some reason, accept both
                    // client.query("...", undefined, () => {...})
                    // **and**
                    // client.query("...", () => {...});
                    // Internally, precedence is given to the callback argument
                    if (callback) {
                        callback = patchCallback(callback);
                    }
                    else {
                        values = patchCallback(values);
                    }
                }
            }
            else {
                if (typeof config.name === "string") {
                    data.query.plan = config.name;
                }
                else if (config.values instanceof Array) {
                    data.query.preparable = {
                        text: config.text,
                        args: config.values,
                    };
                }
                else {
                    data.query.text = config.text;
                }
                if (callback) {
                    callback = patchCallback(callback);
                }
                else if (values) {
                    values = patchCallback(values);
                }
                else {
                    config.callback = patchCallback(config.callback);
                }
            }
        }
        catch (e) {
            // if our logic here throws, bail out and just let pg do its thing
            return originalClientQuery.apply(this, arguments);
        }
        arguments[0] = config;
        arguments[1] = values;
        arguments[2] = callback;
        arguments.length = (arguments.length > 3) ? arguments.length : 3;
        queryResult = originalClientQuery.apply(this, arguments);
        return queryResult;
    };
    return originalPg;
}
function postgres7PatchFunction(originalPg, originalPgPath) {
    var originalClientQuery = originalPg.Client.prototype.query;
    var diagnosticOriginalFunc = "__diagnosticOriginalFunc";
    // wherever the callback is passed, find it, save it, and remove it from the call
    // to the the original .query() function
    originalPg.Client.prototype.query = function query(config, values, callback) {
        var _this = this;
        var callbackProvided = !!callback; // Starting in pg@7.x+, Promise is returned only if !callbackProvided
        var data = {
            query: {},
            database: {
                host: this.connectionParameters.host,
                port: this.connectionParameters.port,
            },
            result: null,
            error: null,
            duration: 0,
        };
        var start = process.hrtime();
        var queryResult;
        function patchCallback(cb) {
            if (cb && cb[diagnosticOriginalFunc]) {
                cb = cb[diagnosticOriginalFunc];
            }
            var trackingCallback = diagnostic_channel_1.channel.bindToContext(function (err, res) {
                var end = process.hrtime(start);
                data.result = res && { rowCount: res.rowCount, command: res.command };
                data.error = err;
                data.duration = Math.ceil((end[0] * 1e3) + (end[1] / 1e6));
                diagnostic_channel_1.channel.publish("postgres", data);
                if (err) {
                    if (cb) {
                        return cb.apply(this, arguments);
                    }
                    else if (queryResult && queryResult instanceof events_1.EventEmitter) {
                        queryResult.emit("error", err);
                    }
                }
                else if (cb) {
                    cb.apply(this, arguments);
                }
            });
            try {
                Object.defineProperty(trackingCallback, diagnosticOriginalFunc, { value: cb });
                return trackingCallback;
            }
            catch (e) {
                // this should never happen, but bailout in case it does
                return cb;
            }
        }
        // Only try to wrap the callback if it is a function. We want to keep the same
        // behavior of returning a promise only if no callback is provided. Wrapping
        // a nonfunction makes it a function and pg will interpret it as a callback
        try {
            if (typeof config === "string") {
                if (values instanceof Array) {
                    data.query.preparable = {
                        text: config,
                        args: values,
                    };
                    callbackProvided = typeof callback === "function";
                    callback = callback ? patchCallback(callback) : callback;
                }
                else {
                    data.query.text = config;
                    if (callback) {
                        callbackProvided = typeof callback === "function";
                        callback = callbackProvided ? patchCallback(callback) : callback;
                    }
                    else {
                        callbackProvided = typeof values === "function";
                        values = callbackProvided ? patchCallback(values) : values;
                    }
                }
            }
            else {
                if (typeof config.name === "string") {
                    data.query.plan = config.name;
                }
                else if (config.values instanceof Array) {
                    data.query.preparable = {
                        text: config.text,
                        args: config.values,
                    };
                }
                else {
                    data.query.text = config.text;
                }
                if (callback) {
                    callbackProvided = typeof callback === "function";
                    callback = patchCallback(callback);
                }
                else if (values) {
                    callbackProvided = typeof values === "function";
                    values = callbackProvided ? patchCallback(values) : values;
                }
                else {
                    callbackProvided = typeof config.callback === "function";
                    config.callback = callbackProvided ? patchCallback(config.callback) : config.callback;
                }
            }
        }
        catch (e) {
            // if our logic here throws, bail out and just let pg do its thing
            return originalClientQuery.apply(this, arguments);
        }
        arguments[0] = config;
        arguments[1] = values;
        arguments[2] = callback;
        arguments.length = (arguments.length > 3) ? arguments.length : 3;
        queryResult = originalClientQuery.apply(this, arguments);
        if (!callbackProvided) {
            // no callback, so create a pass along promise
            return queryResult
                // pass resolved promise after publishing the event
                .then(function (result) {
                patchCallback()(undefined, result);
                return new _this._Promise(function (resolve, reject) {
                    resolve(result);
                });
            })
                // pass along rejected promise after publishing the error
                .catch(function (error) {
                patchCallback()(error, undefined);
                return new _this._Promise(function (resolve, reject) {
                    reject(error);
                });
            });
        }
        return queryResult;
    };
    return originalPg;
}
exports.postgres6 = {
    versionSpecifier: "6.*",
    patch: postgres6PatchFunction,
};
exports.postgres7 = {
    versionSpecifier: "7.*",
    patch: postgres7PatchFunction,
};
function enable() {
    diagnostic_channel_1.channel.registerMonkeyPatch("pg", exports.postgres6);
    diagnostic_channel_1.channel.registerMonkeyPatch("pg", exports.postgres7);
}
exports.enable = enable;
//# sourceMappingURL=pg.pub.js.map