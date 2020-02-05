"use strict";
var __assign = (this && this.__assign) || Object.assign || function(t) {
    for (var s, i = 1, n = arguments.length; i < n; i++) {
        s = arguments[i];
        for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p))
            t[p] = s[p];
    }
    return t;
};
var os = require("os");
var Contracts = require("../Declarations/Contracts");
var Constants = require("../Declarations/Constants");
var Util = require("./Util");
var Logging = require("./Logging");
var StreamId = Util.w3cTraceId(); // Create a guid
var QuickPulseEnvelopeFactory = (function () {
    function QuickPulseEnvelopeFactory() {
    }
    QuickPulseEnvelopeFactory.createQuickPulseEnvelope = function (metrics, documents, config, context) {
        var machineName = (os && typeof os.hostname === "function"
            && os.hostname()) || "Unknown"; // Note: os.hostname() was added in node v0.3.3
        var instance = (context.tags
            && context.keys
            && context.keys.cloudRoleInstance
            && context.tags[context.keys.cloudRoleInstance]) || machineName;
        var envelope = {
            Documents: documents.length > 0 ? documents : null,
            InstrumentationKey: config.instrumentationKey || "",
            Metrics: metrics.length > 0 ? metrics : null,
            InvariantVersion: 1,
            Timestamp: "/Date(" + Date.now() + ")/",
            Version: context.tags[context.keys.internalSdkVersion],
            StreamId: StreamId,
            MachineName: machineName,
            Instance: instance
        };
        return envelope;
    };
    QuickPulseEnvelopeFactory.createQuickPulseMetric = function (telemetry) {
        var data;
        data = {
            Name: telemetry.name,
            Value: telemetry.value,
            Weight: telemetry.count || 1
        };
        return data;
    };
    QuickPulseEnvelopeFactory.telemetryEnvelopeToQuickPulseDocument = function (envelope) {
        switch (envelope.data.baseType) {
            case Contracts.TelemetryTypeString.Event:
                return QuickPulseEnvelopeFactory.createQuickPulseEventDocument(envelope);
            case Contracts.TelemetryTypeString.Exception:
                return QuickPulseEnvelopeFactory.createQuickPulseExceptionDocument(envelope);
            case Contracts.TelemetryTypeString.Trace:
                return QuickPulseEnvelopeFactory.createQuickPulseTraceDocument(envelope);
            case Contracts.TelemetryTypeString.Dependency:
                return QuickPulseEnvelopeFactory.createQuickPulseDependencyDocument(envelope);
            case Contracts.TelemetryTypeString.Request:
                return QuickPulseEnvelopeFactory.createQuickPulseRequestDocument(envelope);
        }
        return null;
    };
    QuickPulseEnvelopeFactory.createQuickPulseEventDocument = function (envelope) {
        var document = QuickPulseEnvelopeFactory.createQuickPulseDocument(envelope);
        var name = envelope.data.baseData.name;
        var eventDocument = __assign({}, document, { Name: name });
        return eventDocument;
    };
    QuickPulseEnvelopeFactory.createQuickPulseTraceDocument = function (envelope) {
        var document = QuickPulseEnvelopeFactory.createQuickPulseDocument(envelope);
        var severityLevel = envelope.data.baseData.severityLevel || 0;
        var traceDocument = __assign({}, document, { Message: envelope.data.baseData.message, SeverityLevel: Contracts.SeverityLevel[severityLevel] });
        return traceDocument;
    };
    QuickPulseEnvelopeFactory.createQuickPulseExceptionDocument = function (envelope) {
        var document = QuickPulseEnvelopeFactory.createQuickPulseDocument(envelope);
        var exceptionDetails = envelope.data.baseData.exceptions;
        var exception = '';
        var exceptionMessage = '';
        var exceptionType = '';
        // Try to fill exception information from first error only
        if (exceptionDetails && exceptionDetails.length > 0) {
            // Try to grab the stack from parsedStack or stack
            if (exceptionDetails[0].parsedStack && exceptionDetails[0].parsedStack.length > 0) {
                exceptionDetails[0].parsedStack.forEach(function (err) {
                    exception += err.assembly + "\n";
                });
            }
            else if (exceptionDetails[0].stack && exceptionDetails[0].stack.length > 0) {
                exception = exceptionDetails[0].stack;
            }
            exceptionMessage = exceptionDetails[0].message;
            exceptionType = exceptionDetails[0].typeName;
        }
        var exceptionDocument = __assign({}, document, { Exception: exception, ExceptionMessage: exceptionMessage, ExceptionType: exceptionType });
        return exceptionDocument;
    };
    QuickPulseEnvelopeFactory.createQuickPulseRequestDocument = function (envelope) {
        var document = QuickPulseEnvelopeFactory.createQuickPulseDocument(envelope);
        var baseData = envelope.data.baseData;
        var requestDocument = __assign({}, document, { Name: baseData.name, Success: baseData.success, Duration: baseData.duration, ResponseCode: baseData.responseCode, OperationName: baseData.name // TODO: is this correct?
         });
        return requestDocument;
    };
    QuickPulseEnvelopeFactory.createQuickPulseDependencyDocument = function (envelope) {
        var document = QuickPulseEnvelopeFactory.createQuickPulseDocument(envelope);
        var baseData = envelope.data.baseData;
        var dependencyDocument = __assign({}, document, { Name: baseData.name, Target: baseData.target, Success: baseData.success, Duration: baseData.duration, ResultCode: baseData.resultCode, CommandName: baseData.data, OperationName: document.OperationId, DependencyTypeName: baseData.type });
        return dependencyDocument;
    };
    QuickPulseEnvelopeFactory.createQuickPulseDocument = function (envelope) {
        var documentType;
        var __type;
        var operationId, properties;
        if (envelope.data.baseType) {
            __type = Constants.TelemetryTypeStringToQuickPulseType[envelope.data.baseType];
            documentType = Constants.TelemetryTypeStringToQuickPulseDocumentType[envelope.data.baseType];
        }
        else {
            // Remark: This should never be hit because createQuickPulseDocument is only called within
            // valid baseType values
            Logging.warn("Document type invalid; not sending live metric document", envelope.data.baseType);
        }
        operationId = envelope.tags[QuickPulseEnvelopeFactory.keys.operationId];
        properties = QuickPulseEnvelopeFactory.aggregateProperties(envelope);
        var document = {
            DocumentType: documentType,
            __type: __type,
            OperationId: operationId,
            Version: "1.0",
            Properties: properties
        };
        return document;
    };
    QuickPulseEnvelopeFactory.aggregateProperties = function (envelope) {
        var properties = [];
        // Collect measurements
        var meas = (envelope.data.baseData).measurements || {};
        for (var key in meas) {
            if (meas.hasOwnProperty(key)) {
                var value = meas[key];
                var property = { key: key, value: value };
                properties.push(property);
            }
        }
        // Collect properties
        var props = (envelope.data.baseData).properties || {};
        for (var key in props) {
            if (props.hasOwnProperty(key)) {
                var value = props[key];
                var property = { key: key, value: value };
                properties.push(property);
            }
        }
        return properties;
    };
    QuickPulseEnvelopeFactory.keys = new Contracts.ContextTagKeys();
    return QuickPulseEnvelopeFactory;
}());
module.exports = QuickPulseEnvelopeFactory;
//# sourceMappingURL=QuickPulseEnvelopeFactory.js.map