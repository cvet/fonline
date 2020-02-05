"use strict";
var __extends = (this && this.__extends) || (function () {
    var extendStatics = Object.setPrototypeOf ||
        ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
        function (d, b) { for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p]; };
    return function (d, b) {
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var __rest = (this && this.__rest) || function (s, e) {
    var t = {};
    for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p) && e.indexOf(p) < 0)
        t[p] = s[p];
    if (s != null && typeof Object.getOwnPropertySymbols === "function")
        for (var i = 0, p = Object.getOwnPropertySymbols(s); i < p.length; i++) if (e.indexOf(p[i]) < 0)
            t[p[i]] = s[p[i]];
    return t;
};
Object.defineProperty(exports, "__esModule", { value: true });
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
var diagnostic_channel_1 = require("diagnostic-channel");
// register a "filter" with each logger that publishes the data about to be logged
var winston2PatchFunction = function (originalWinston) {
    var originalLog = originalWinston.Logger.prototype.log;
    var curLevels;
    var loggingFilter = function (level, message, meta) {
        var levelKind;
        if (curLevels === originalWinston.config.npm.levels) {
            levelKind = "npm";
        }
        else if (curLevels === originalWinston.config.syslog.levels) {
            levelKind = "syslog";
        }
        else {
            levelKind = "unknown";
        }
        diagnostic_channel_1.channel.publish("winston", { level: level, message: message, meta: meta, levelKind: levelKind });
        return message;
    };
    // whenever someone logs, ensure our filter comes last
    originalWinston.Logger.prototype.log = function log() {
        curLevels = this.levels;
        if (!this.filters || this.filters.length === 0) {
            this.filters = [loggingFilter];
        }
        else if (this.filters[this.filters.length - 1] !== loggingFilter) {
            this.filters = this.filters.filter(function (f) { return f !== loggingFilter; });
            this.filters.push(loggingFilter);
        }
        return originalLog.apply(this, arguments);
    };
    return originalWinston;
};
var winston3PatchFunction = function (originalWinston) {
    var mapLevelToKind = function (winston, level) {
        var levelKind;
        if (winston.config.npm.levels[level] != null) {
            levelKind = "npm";
        }
        else if (winston.config.syslog.levels[level] != null) {
            levelKind = "syslog";
        }
        else {
            levelKind = "unknown";
        }
        return levelKind;
    };
    var AppInsightsTransport = /** @class */ (function (_super) {
        __extends(AppInsightsTransport, _super);
        function AppInsightsTransport(winston, opts) {
            var _this = _super.call(this, opts) || this;
            _this.winston = winston;
            return _this;
        }
        AppInsightsTransport.prototype.log = function (info, callback) {
            // tslint:disable-next-line:prefer-const - try to obtain level from Symbol(level) afterwards
            var message = info.message, level = info.level, meta = info.meta, splat = __rest(info, ["message", "level", "meta"]);
            level = typeof Symbol["for"] === "function" ? info[Symbol["for"]("level")] : level; // Symbol(level) is uncolorized, so prefer getting it from here
            var levelKind = mapLevelToKind(this.winston, level);
            meta = meta || {}; // Winston _somtimes_ puts metadata inside meta, so start from here
            for (var key in splat) {
                if (splat.hasOwnProperty(key)) {
                    meta[key] = splat[key];
                }
            }
            diagnostic_channel_1.channel.publish("winston", { message: message, level: level, levelKind: levelKind, meta: meta });
            callback();
        };
        return AppInsightsTransport;
    }(originalWinston.Transport));
    // Patch this function
    function patchedConfigure() {
        // Grab highest sev logging level in case of custom logging levels
        var levels = arguments[0].levels || originalWinston.config.npm.levels;
        var lastLevel;
        for (var level in levels) {
            if (levels.hasOwnProperty(level)) {
                lastLevel = lastLevel === undefined || levels[level] > levels[lastLevel] ? level : lastLevel;
            }
        }
        this.add(new AppInsightsTransport(originalWinston, { level: lastLevel }));
    }
    var origCreate = originalWinston.createLogger;
    originalWinston.createLogger = function patchedCreate() {
        // Grab highest sev logging level in case of custom logging levels
        var levels = arguments[0].levels || originalWinston.config.npm.levels;
        var lastLevel;
        for (var level in levels) {
            if (levels.hasOwnProperty(level)) {
                lastLevel = lastLevel === undefined || levels[level] > levels[lastLevel] ? level : lastLevel;
            }
        }
        // Add custom app insights transport to the end
        // Remark: Configure is not available until after createLogger()
        // and the Logger prototype is not exported in winston 3.x, so
        // patch both createLogger and configure. Could also call configure
        // again after createLogger, but that would cause configure to be called
        // twice per create.
        var result = origCreate.apply(this, arguments);
        result.add(new AppInsightsTransport(originalWinston, { level: lastLevel }));
        var origConfigure = result.configure;
        result.configure = function () {
            origConfigure.apply(this, arguments);
            patchedConfigure.apply(this, arguments);
        };
        return result;
    };
    var origRootConfigure = originalWinston.createLogger;
    originalWinston.configure = function () {
        origRootConfigure.apply(this, arguments);
        patchedConfigure.apply(this, arguments);
    };
    originalWinston.add(new AppInsightsTransport(originalWinston));
    return originalWinston;
};
exports.winston3 = {
    versionSpecifier: "3.x",
    patch: winston3PatchFunction,
};
exports.winston2 = {
    versionSpecifier: "2.x",
    patch: winston2PatchFunction,
};
function enable() {
    diagnostic_channel_1.channel.registerMonkeyPatch("winston", exports.winston2);
    diagnostic_channel_1.channel.registerMonkeyPatch("winston", exports.winston3);
}
exports.enable = enable;
//# sourceMappingURL=winston.pub.js.map