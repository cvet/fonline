import Contracts = require("./Contracts");
export declare enum QuickPulseCounter {
    COMMITTED_BYTES = "\\Memory\\Committed Bytes",
    PROCESSOR_TIME = "\\Processor(_Total)\\% Processor Time",
    REQUEST_RATE = "\\ApplicationInsights\\Requests/Sec",
    REQUEST_FAILURE_RATE = "\\ApplicationInsights\\Requests Failed/Sec",
    REQUEST_DURATION = "\\ApplicationInsights\\Request Duration",
    DEPENDENCY_RATE = "\\ApplicationInsights\\Dependency Calls/Sec",
    DEPENDENCY_FAILURE_RATE = "\\ApplicationInsights\\Dependency Calls Failed/Sec",
    DEPENDENCY_DURATION = "\\ApplicationInsights\\Dependency Call Duration",
    EXCEPTION_RATE = "\\ApplicationInsights\\Exceptions/Sec",
}
export declare enum PerformanceCounter {
    PRIVATE_BYTES = "\\Process(??APP_WIN32_PROC??)\\Private Bytes",
    AVAILABLE_BYTES = "\\Memory\\Available Bytes",
    PROCESSOR_TIME = "\\Processor(_Total)\\% Processor Time",
    PROCESS_TIME = "\\Process(??APP_WIN32_PROC??)\\% Processor Time",
    REQUEST_RATE = "\\ASP.NET Applications(??APP_W3SVC_PROC??)\\Requests/Sec",
    REQUEST_DURATION = "\\ASP.NET Applications(??APP_W3SVC_PROC??)\\Request Execution Time",
}
/**
 * Map a PerformanceCounter/QuickPulseCounter to a QuickPulseCounter. If no mapping exists, mapping is *undefined*
 */
export declare const PerformanceToQuickPulseCounter: {
    [key: string]: QuickPulseCounter;
};
export declare type QuickPulseDocumentType = "Event" | "Exception" | "Trace" | "Metric" | "Request" | "RemoteDependency";
export declare type QuickPulseType = "EventTelemetryDocument" | "ExceptionTelemetryDocument" | "TraceTelemetryDocument" | "MetricTelemetryDocument" | "RequestTelemetryDocument" | "DependencyTelemetryDocument";
export declare const QuickPulseDocumentType: {
    [key in Contracts.TelemetryTypeKeys]: QuickPulseDocumentType;
};
export declare const QuickPulseType: {
    [key in Contracts.TelemetryTypeKeys]: QuickPulseType;
};
export declare const TelemetryTypeStringToQuickPulseType: {
    [key in Contracts.TelemetryTypeValues]: QuickPulseType;
};
export declare const TelemetryTypeStringToQuickPulseDocumentType: {
    [key in Contracts.TelemetryTypeValues]: QuickPulseDocumentType;
};
