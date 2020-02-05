"use strict";
var __assign = (this && this.__assign) || Object.assign || function(t) {
    for (var s, i = 1, n = arguments.length; i < n; i++) {
        s = arguments[i];
        for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p))
            t[p] = s[p];
    }
    return t;
};
Object.defineProperty(exports, "__esModule", { value: true });
var Config = require("../Library/Config");
var Context = require("../Library/Context");
var Logging = require("../Library/Logging");
var AutoCollectNativePerformance = (function () {
    function AutoCollectNativePerformance(client) {
        this._disabledMetrics = {};
        // Note: Only 1 instance of this can exist. So when we reconstruct this object,
        // just disable old native instance and reset JS member variables
        if (AutoCollectNativePerformance.INSTANCE) {
            AutoCollectNativePerformance.INSTANCE.dispose();
        }
        AutoCollectNativePerformance.INSTANCE = this;
        this._client = client;
    }
    /**
     *  Reports if NativePerformance is able to run in this environment
     */
    AutoCollectNativePerformance.isNodeVersionCompatible = function () {
        var nodeVer = process.versions.node.split(".");
        return parseInt(nodeVer[0]) >= 6;
    };
    /**
     * Start instance of native metrics agent.
     *
     * @param {boolean} isEnabled
     * @param {number} [collectionInterval=60000]
     * @memberof AutoCollectNativePerformance
     */
    AutoCollectNativePerformance.prototype.enable = function (isEnabled, disabledMetrics, collectionInterval) {
        var _this = this;
        if (disabledMetrics === void 0) { disabledMetrics = {}; }
        if (collectionInterval === void 0) { collectionInterval = 60000; }
        if (!AutoCollectNativePerformance.isNodeVersionCompatible()) {
            return;
        }
        if (AutoCollectNativePerformance._metricsAvailable == undefined && isEnabled && !this._isInitialized) {
            // Try to require in the native-metrics library. If it's found initialize it, else do nothing and never try again.
            try {
                var NativeMetricsEmitters = require("applicationinsights-native-metrics");
                AutoCollectNativePerformance._emitter = new NativeMetricsEmitters();
                AutoCollectNativePerformance._metricsAvailable = true;
                Logging.info("Native metrics module successfully loaded!");
            }
            catch (err) {
                // Package not available. Never try again
                AutoCollectNativePerformance._metricsAvailable = false;
                return;
            }
        }
        this._isEnabled = isEnabled;
        this._disabledMetrics = disabledMetrics;
        if (this._isEnabled && !this._isInitialized) {
            this._isInitialized = true;
        }
        // Enable the emitter if we were able to construct one
        if (this._isEnabled && AutoCollectNativePerformance._emitter) {
            // enable self
            AutoCollectNativePerformance._emitter.enable(true, collectionInterval);
            this._handle = setInterval(function () { return _this._trackNativeMetrics(); }, collectionInterval);
            this._handle.unref();
        }
        else if (AutoCollectNativePerformance._emitter) {
            // disable self
            AutoCollectNativePerformance._emitter.enable(false);
            if (this._handle) {
                clearInterval(this._handle);
                this._handle = undefined;
            }
        }
    };
    /**
     * Cleanup this instance of AutoCollectNativePerformance
     *
     * @memberof AutoCollectNativePerformance
     */
    AutoCollectNativePerformance.prototype.dispose = function () {
        this.enable(false);
    };
    /**
     * Parse environment variable and overwrite isEnabled based on respective fields being set
     *
     * @private
     * @static
     * @param {(boolean | IDisabledExtendedMetrics)} collectExtendedMetrics
     * @returns {(boolean | IDisabledExtendedMetrics)}
     * @memberof AutoCollectNativePerformance
     */
    AutoCollectNativePerformance.parseEnabled = function (collectExtendedMetrics) {
        var disableAll = process.env[Config.ENV_nativeMetricsDisableAll];
        var individualOptOuts = process.env[Config.ENV_nativeMetricsDisablers];
        // case 1: disable all env var set, RETURN with isEnabled=false
        if (disableAll) {
            return { isEnabled: false, disabledMetrics: {} };
        }
        // case 2: individual env vars set, RETURN with isEnabled=true, disabledMetrics={...}
        if (individualOptOuts) {
            var optOutsArr = individualOptOuts.split(",");
            var disabledMetrics = {};
            if (optOutsArr.length > 0) {
                for (var _i = 0, optOutsArr_1 = optOutsArr; _i < optOutsArr_1.length; _i++) {
                    var opt = optOutsArr_1[_i];
                    disabledMetrics[opt] = true;
                }
            }
            // case 2a: collectExtendedMetrics is an object, overwrite existing ones if they exist
            if (typeof collectExtendedMetrics === "object") {
                return { isEnabled: true, disabledMetrics: __assign({}, collectExtendedMetrics, disabledMetrics) };
            }
            // case 2b: collectExtendedMetrics is a boolean, set disabledMetrics as is
            return { isEnabled: collectExtendedMetrics, disabledMetrics: disabledMetrics };
        }
        // case 4: no env vars set, input arg is a boolean, RETURN with isEnabled=collectExtendedMetrics, disabledMetrics={}
        if (typeof collectExtendedMetrics === "boolean") {
            return { isEnabled: collectExtendedMetrics, disabledMetrics: {} };
        }
        else {
            // case 5: no env vars set, input arg is object, RETURN with isEnabled=true, disabledMetrics=collectExtendedMetrics
            return { isEnabled: true, disabledMetrics: collectExtendedMetrics };
        }
    };
    /**
     * Trigger an iteration of native metrics collection
     *
     * @private
     * @memberof AutoCollectNativePerformance
     */
    AutoCollectNativePerformance.prototype._trackNativeMetrics = function () {
        var shouldSendAll = true;
        if (typeof this._isEnabled !== "object") {
            shouldSendAll = this._isEnabled;
        }
        if (shouldSendAll) {
            this._trackGarbageCollection();
            this._trackEventLoop();
            this._trackHeapUsage();
        }
    };
    /**
     * Tracks garbage collection stats for this interval. One custom metric is sent per type of garbage
     * collection that occurred during this collection interval.
     *
     * @private
     * @memberof AutoCollectNativePerformance
     */
    AutoCollectNativePerformance.prototype._trackGarbageCollection = function () {
        if (this._disabledMetrics.gc) {
            return;
        }
        var gcData = AutoCollectNativePerformance._emitter.getGCData();
        for (var gc in gcData) {
            var metrics = gcData[gc].metrics;
            var name_1 = gc + " Garbage Collection Duration";
            var stdDev = Math.sqrt(metrics.sumSquares / metrics.count - Math.pow(metrics.total / metrics.count, 2)) || 0;
            this._client.trackMetric({
                name: name_1,
                value: metrics.total,
                count: metrics.count,
                max: metrics.max,
                min: metrics.min,
                stdDev: stdDev,
                tagOverrides: (_a = {},
                    _a[this._client.context.keys.internalSdkVersion] = "node-nativeperf:" + Context.sdkVersion,
                    _a)
            });
        }
        var _a;
    };
    /**
     * Tracks event loop ticks per interval as a custom metric. Also included in the metric is min/max/avg
     * time spent in event loop for this interval.
     *
     * @private
     * @returns {void}
     * @memberof AutoCollectNativePerformance
     */
    AutoCollectNativePerformance.prototype._trackEventLoop = function () {
        if (this._disabledMetrics.loop) {
            return;
        }
        var loopData = AutoCollectNativePerformance._emitter.getLoopData();
        var metrics = loopData.loopUsage;
        if (metrics.count == 0) {
            return;
        }
        var name = "Event Loop CPU Time";
        var stdDev = Math.sqrt(metrics.sumSquares / metrics.count - Math.pow(metrics.total / metrics.count, 2)) || 0;
        this._client.trackMetric({
            name: name,
            value: metrics.total,
            count: metrics.count,
            min: metrics.min,
            max: metrics.max,
            stdDev: stdDev,
            tagOverrides: (_a = {},
                _a[this._client.context.keys.internalSdkVersion] = "node-nativeperf:" + Context.sdkVersion,
                _a)
        });
        var _a;
    };
    /**
     * Track heap memory usage metrics as a custom metric.
     *
     * @private
     * @memberof AutoCollectNativePerformance
     */
    AutoCollectNativePerformance.prototype._trackHeapUsage = function () {
        if (this._disabledMetrics.heap) {
            return;
        }
        var memoryUsage = process.memoryUsage();
        var heapUsed = memoryUsage.heapUsed, heapTotal = memoryUsage.heapTotal, rss = memoryUsage.rss;
        this._client.trackMetric({
            name: "Memory Usage (Heap)",
            value: heapUsed,
            count: 1,
            tagOverrides: (_a = {},
                _a[this._client.context.keys.internalSdkVersion] = "node-nativeperf:" + Context.sdkVersion,
                _a)
        });
        this._client.trackMetric({
            name: "Memory Total (Heap)",
            value: heapTotal,
            count: 1,
            tagOverrides: (_b = {},
                _b[this._client.context.keys.internalSdkVersion] = "node-nativeperf:" + Context.sdkVersion,
                _b)
        });
        this._client.trackMetric({
            name: "Memory Usage (Non-Heap)",
            value: rss - heapTotal,
            count: 1,
            tagOverrides: (_c = {},
                _c[this._client.context.keys.internalSdkVersion] = "node-nativeperf:" + Context.sdkVersion,
                _c)
        });
        var _a, _b, _c;
    };
    return AutoCollectNativePerformance;
}());
exports.AutoCollectNativePerformance = AutoCollectNativePerformance;
//# sourceMappingURL=NativePerformance.js.map