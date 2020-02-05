"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
var QuickPulseCounter;
(function (QuickPulseCounter) {
    // Memory
    QuickPulseCounter["COMMITTED_BYTES"] = "\\Memory\\Committed Bytes";
    // CPU
    QuickPulseCounter["PROCESSOR_TIME"] = "\\Processor(_Total)\\% Processor Time";
    // Request
    QuickPulseCounter["REQUEST_RATE"] = "\\ApplicationInsights\\Requests/Sec";
    QuickPulseCounter["REQUEST_FAILURE_RATE"] = "\\ApplicationInsights\\Requests Failed/Sec";
    QuickPulseCounter["REQUEST_DURATION"] = "\\ApplicationInsights\\Request Duration";
    // Dependency
    QuickPulseCounter["DEPENDENCY_RATE"] = "\\ApplicationInsights\\Dependency Calls/Sec";
    QuickPulseCounter["DEPENDENCY_FAILURE_RATE"] = "\\ApplicationInsights\\Dependency Calls Failed/Sec";
    QuickPulseCounter["DEPENDENCY_DURATION"] = "\\ApplicationInsights\\Dependency Call Duration";
    // Exception
    QuickPulseCounter["EXCEPTION_RATE"] = "\\ApplicationInsights\\Exceptions/Sec";
})(QuickPulseCounter = exports.QuickPulseCounter || (exports.QuickPulseCounter = {}));
var PerformanceCounter;
(function (PerformanceCounter) {
    // Memory
    PerformanceCounter["PRIVATE_BYTES"] = "\\Process(??APP_WIN32_PROC??)\\Private Bytes";
    PerformanceCounter["AVAILABLE_BYTES"] = "\\Memory\\Available Bytes";
    // CPU
    PerformanceCounter["PROCESSOR_TIME"] = "\\Processor(_Total)\\% Processor Time";
    PerformanceCounter["PROCESS_TIME"] = "\\Process(??APP_WIN32_PROC??)\\% Processor Time";
    // Requests
    PerformanceCounter["REQUEST_RATE"] = "\\ASP.NET Applications(??APP_W3SVC_PROC??)\\Requests/Sec";
    PerformanceCounter["REQUEST_DURATION"] = "\\ASP.NET Applications(??APP_W3SVC_PROC??)\\Request Execution Time";
})(PerformanceCounter = exports.PerformanceCounter || (exports.PerformanceCounter = {}));
;
/**
 * Map a PerformanceCounter/QuickPulseCounter to a QuickPulseCounter. If no mapping exists, mapping is *undefined*
 */
exports.PerformanceToQuickPulseCounter = (_a = {},
    _a[PerformanceCounter.PROCESSOR_TIME] = QuickPulseCounter.PROCESSOR_TIME,
    _a[PerformanceCounter.REQUEST_RATE] = QuickPulseCounter.REQUEST_RATE,
    _a[PerformanceCounter.REQUEST_DURATION] = QuickPulseCounter.REQUEST_DURATION,
    // Remap quick pulse only counters
    _a[QuickPulseCounter.COMMITTED_BYTES] = QuickPulseCounter.COMMITTED_BYTES,
    _a[QuickPulseCounter.REQUEST_FAILURE_RATE] = QuickPulseCounter.REQUEST_FAILURE_RATE,
    _a[QuickPulseCounter.DEPENDENCY_RATE] = QuickPulseCounter.DEPENDENCY_RATE,
    _a[QuickPulseCounter.DEPENDENCY_FAILURE_RATE] = QuickPulseCounter.DEPENDENCY_FAILURE_RATE,
    _a[QuickPulseCounter.DEPENDENCY_DURATION] = QuickPulseCounter.DEPENDENCY_DURATION,
    _a[QuickPulseCounter.EXCEPTION_RATE] = QuickPulseCounter.EXCEPTION_RATE,
    _a);
exports.QuickPulseDocumentType = {
    Event: "Event",
    Exception: "Exception",
    Trace: "Trace",
    Metric: "Metric",
    Request: "Request",
    Dependency: "RemoteDependency"
};
exports.QuickPulseType = {
    Event: "EventTelemetryDocument",
    Exception: "ExceptionTelemetryDocument",
    Trace: "TraceTelemetryDocument",
    Metric: "MetricTelemetryDocument",
    Request: "RequestTelemetryDocument",
    Dependency: "DependencyTelemetryDocument"
};
exports.TelemetryTypeStringToQuickPulseType = {
    EventData: exports.QuickPulseType.Event,
    ExceptionData: exports.QuickPulseType.Exception,
    MessageData: exports.QuickPulseType.Trace,
    MetricData: exports.QuickPulseType.Metric,
    RequestData: exports.QuickPulseType.Request,
    RemoteDependencyData: exports.QuickPulseType.Dependency
};
exports.TelemetryTypeStringToQuickPulseDocumentType = {
    EventData: exports.QuickPulseDocumentType.Event,
    ExceptionData: exports.QuickPulseDocumentType.Exception,
    MessageData: exports.QuickPulseDocumentType.Trace,
    MetricData: exports.QuickPulseDocumentType.Metric,
    RequestData: exports.QuickPulseDocumentType.Request,
    RemoteDependencyData: exports.QuickPulseDocumentType.Dependency
};
var _a;
//# sourceMappingURL=Constants.js.map