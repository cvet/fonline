/// <reference types="node" />
import http = require("http");
import TelemetryClient = require("../Library/TelemetryClient");
declare class AutoCollectPerformance {
    static INSTANCE: AutoCollectPerformance;
    private static _totalRequestCount;
    private static _totalFailedRequestCount;
    private static _lastRequestExecutionTime;
    private static _totalDependencyCount;
    private static _totalFailedDependencyCount;
    private static _lastDependencyExecutionTime;
    private static _totalExceptionCount;
    private static _intervalDependencyExecutionTime;
    private static _intervalRequestExecutionTime;
    private _enableLiveMetricsCounters;
    private _collectionInterval;
    private _client;
    private _handle;
    private _isEnabled;
    private _isInitialized;
    private _lastAppCpuUsage;
    private _lastHrtime;
    private _lastCpus;
    private _lastDependencies;
    private _lastRequests;
    private _lastExceptions;
    /**
     * @param enableLiveMetricsCounters - enable sending additional live metrics information (dependency metrics, exception metrics, committed memory)
     */
    constructor(client: TelemetryClient, collectionInterval?: number, enableLiveMetricsCounters?: boolean);
    enable(isEnabled: boolean, collectionInterval?: number): void;
    static countRequest(request: http.ServerRequest, response: http.ServerResponse): void;
    static countException(): void;
    static countDependency(duration: number | string, success: boolean): void;
    isInitialized(): boolean;
    static isEnabled(): boolean;
    trackPerformance(): void;
    private _trackCpu();
    private _trackMemory();
    private _trackNetwork();
    private _trackDependencyRate();
    private _trackExceptionRate();
    dispose(): void;
}
export = AutoCollectPerformance;
