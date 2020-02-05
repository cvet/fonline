/**
 * Helper class to manage parsing and validation of traceparent header. Also handles hierarchical
 * back-compatibility headers generated from traceparent. W3C traceparent spec is documented at
 * https://www.w3.org/TR/trace-context/#traceparent-field
 */
declare class Traceparent {
    private static DEFAULT_TRACE_FLAG;
    private static DEFAULT_VERSION;
    legacyRootId: string;
    parentId: string;
    spanId: string;
    traceFlag: string;
    traceId: string;
    version: string;
    constructor(traceparent?: string, parentId?: string);
    static isValidTraceId(id: string): boolean;
    static isValidSpanId(id: string): boolean;
    getBackCompatRequestId(): string;
    toString(): string;
    updateSpanId(): void;
}
export = Traceparent;
