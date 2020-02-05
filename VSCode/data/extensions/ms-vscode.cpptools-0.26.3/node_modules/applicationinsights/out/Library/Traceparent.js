"use strict";
var Util = require("./Util");
var CorrelationIdManager = require("./CorrelationIdManager");
/**
 * Helper class to manage parsing and validation of traceparent header. Also handles hierarchical
 * back-compatibility headers generated from traceparent. W3C traceparent spec is documented at
 * https://www.w3.org/TR/trace-context/#traceparent-field
 */
var Traceparent = (function () {
    function Traceparent(traceparent, parentId) {
        this.traceFlag = Traceparent.DEFAULT_TRACE_FLAG;
        this.version = Traceparent.DEFAULT_VERSION;
        if (traceparent && typeof traceparent === "string") {
            // If incoming request contains traceparent: parse it, set operation, parent and telemetry id accordingly. traceparent should be injected into outgoing requests. request-id should be injected in back-compat format |traceId.spanId. so that older SDKs could understand it.
            if (traceparent.split(",").length > 1) {
                this.traceId = Util.w3cTraceId();
                this.spanId = Util.w3cTraceId().substr(0, 16);
            }
            else {
                var traceparentArr = traceparent.trim().split("-");
                var len = traceparentArr.length;
                if (len >= 4) {
                    this.version = traceparentArr[0];
                    this.traceId = traceparentArr[1];
                    this.spanId = traceparentArr[2];
                    this.traceFlag = traceparentArr[3];
                }
                else {
                    this.traceId = Util.w3cTraceId();
                    this.spanId = Util.w3cTraceId().substr(0, 16);
                }
                // Version validation
                if (!this.version.match(/^[0-9a-f]{2}$/g)) {
                    this.version = Traceparent.DEFAULT_VERSION;
                    this.traceId = Util.w3cTraceId();
                }
                if (this.version === "00" && len !== 4) {
                    this.traceId = Util.w3cTraceId();
                    this.spanId = Util.w3cTraceId().substr(0, 16);
                }
                if (this.version === "ff") {
                    this.version = Traceparent.DEFAULT_VERSION;
                    this.traceId = Util.w3cTraceId();
                    this.spanId = Util.w3cTraceId().substr(0, 16);
                }
                if (!this.version.match(/^0[0-9a-f]$/g)) {
                    this.version = Traceparent.DEFAULT_VERSION;
                }
                // TraceFlag validation
                if (!this.traceFlag.match(/^[0-9a-f]{2}$/g)) {
                    this.traceFlag = Traceparent.DEFAULT_VERSION;
                    this.traceId = Util.w3cTraceId();
                }
                // Validate TraceId, regenerate new traceid if invalid
                if (!Traceparent.isValidTraceId(this.traceId)) {
                    this.traceId = Util.w3cTraceId();
                }
                // Validate Span Id, discard entire traceparent if invalid
                if (!Traceparent.isValidSpanId(this.spanId)) {
                    this.spanId = Util.w3cTraceId().substr(0, 16);
                    this.traceId = Util.w3cTraceId();
                }
                // Save backCompat parentId
                this.parentId = this.getBackCompatRequestId();
            }
        }
        else if (parentId) {
            // If incoming request contains only request-id, new traceid and spanid should be started, request-id value should be used as a parent. Root part of request-id should be stored in custom dimension on the request telemetry if root part is different from traceid. On the outgoing request side, request-id should be emitted in the |traceId.spanId. format.
            this.parentId = parentId.slice(); // copy
            var operationId = CorrelationIdManager.getRootId(parentId);
            if (!Traceparent.isValidTraceId(operationId)) {
                this.legacyRootId = operationId;
                operationId = Util.w3cTraceId();
            }
            if (parentId.indexOf("|") !== -1) {
                parentId = parentId.substring(1 + parentId.substring(0, parentId.length - 1).lastIndexOf("."), parentId.length - 1);
            }
            this.traceId = operationId;
            this.spanId = parentId;
        }
        else {
            // Fallback default constructor
            // if request does not contain any correlation headers, see case p2
            this.traceId = Util.w3cTraceId();
            this.spanId = Util.w3cTraceId().substr(0, 16);
        }
    }
    Traceparent.isValidTraceId = function (id) {
        return id.match(/^[0-9a-f]{32}$/) && id !== "00000000000000000000000000000000";
    };
    Traceparent.isValidSpanId = function (id) {
        return id.match(/^[0-9a-f]{16}$/) && id !== "0000000000000000";
    };
    Traceparent.prototype.getBackCompatRequestId = function () {
        return "|" + this.traceId + "." + this.spanId + ".";
    };
    Traceparent.prototype.toString = function () {
        return this.version + "-" + this.traceId + "-" + this.spanId + "-" + this.traceFlag;
    };
    Traceparent.prototype.updateSpanId = function () {
        this.spanId = Util.w3cTraceId().substr(0, 16);
    };
    Traceparent.DEFAULT_TRACE_FLAG = "00";
    Traceparent.DEFAULT_VERSION = "00";
    return Traceparent;
}());
module.exports = Traceparent;
//# sourceMappingURL=Traceparent.js.map