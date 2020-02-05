import TelemetryClient = require("../Library/TelemetryClient");
/**
 * Interface which defines which specific extended metrics should be disabled
 *
 * @export
 * @interface IDisabledExtendedMetrics
 */
export interface IDisabledExtendedMetrics {
    gc?: boolean;
    heap?: boolean;
    loop?: boolean;
}
export declare class AutoCollectNativePerformance {
    static INSTANCE: AutoCollectNativePerformance;
    private static _emitter;
    private static _metricsAvailable;
    private _isEnabled;
    private _isInitialized;
    private _handle;
    private _client;
    private _disabledMetrics;
    constructor(client: TelemetryClient);
    /**
     *  Reports if NativePerformance is able to run in this environment
     */
    static isNodeVersionCompatible(): boolean;
    /**
     * Start instance of native metrics agent.
     *
     * @param {boolean} isEnabled
     * @param {number} [collectionInterval=60000]
     * @memberof AutoCollectNativePerformance
     */
    enable(isEnabled: boolean, disabledMetrics?: IDisabledExtendedMetrics, collectionInterval?: number): void;
    /**
     * Cleanup this instance of AutoCollectNativePerformance
     *
     * @memberof AutoCollectNativePerformance
     */
    dispose(): void;
    /**
     * Parse environment variable and overwrite isEnabled based on respective fields being set
     *
     * @private
     * @static
     * @param {(boolean | IDisabledExtendedMetrics)} collectExtendedMetrics
     * @returns {(boolean | IDisabledExtendedMetrics)}
     * @memberof AutoCollectNativePerformance
     */
    static parseEnabled(collectExtendedMetrics: boolean | IDisabledExtendedMetrics): {
        isEnabled: boolean;
        disabledMetrics: IDisabledExtendedMetrics;
    };
    /**
     * Trigger an iteration of native metrics collection
     *
     * @private
     * @memberof AutoCollectNativePerformance
     */
    private _trackNativeMetrics();
    /**
     * Tracks garbage collection stats for this interval. One custom metric is sent per type of garbage
     * collection that occurred during this collection interval.
     *
     * @private
     * @memberof AutoCollectNativePerformance
     */
    private _trackGarbageCollection();
    /**
     * Tracks event loop ticks per interval as a custom metric. Also included in the metric is min/max/avg
     * time spent in event loop for this interval.
     *
     * @private
     * @returns {void}
     * @memberof AutoCollectNativePerformance
     */
    private _trackEventLoop();
    /**
     * Track heap memory usage metrics as a custom metric.
     *
     * @private
     * @memberof AutoCollectNativePerformance
     */
    private _trackHeapUsage();
}
