/// <reference types="node" />
import events = require("events");
import Traceparent = require("../Library/Traceparent");
import Tracestate = require("../Library/Tracestate");
export interface CustomProperties {
    /**
     * Get a custom property from the correlation context
     */
    getProperty(key: string): string;
    /**
     * Store a custom property in the correlation context.
     * Do not store sensitive information here.
     * Properties stored here are exposed via outgoing HTTP headers for correlating data cross-component.
     * The characters ',' and '=' are disallowed within keys or values.
     */
    setProperty(key: string, value: string): void;
}
export interface PrivateCustomProperties extends CustomProperties {
    addHeaderData(header: string): void;
    serializeToHeader(): string;
}
export interface CorrelationContext {
    operation: {
        name: string;
        id: string;
        parentId: string;
        traceparent?: Traceparent;
        tracestate?: Tracestate;
    };
    /** Do not store sensitive information here.
     *  Properties here are exposed via outgoing HTTP headers for correlating data cross-component.
     */
    customProperties: CustomProperties;
}
export declare class CorrelationContextManager {
    private static enabled;
    private static hasEverEnabled;
    private static forceClsHooked;
    private static session;
    private static cls;
    private static CONTEXT_NAME;
    /**
     *  Provides the current Context.
     *  The context is the most recent one entered into for the current
     *  logical chain of execution, including across asynchronous calls.
     */
    static getCurrentContext(): CorrelationContext;
    /**
     *  A helper to generate objects conforming to the CorrelationContext interface
     */
    static generateContextObject(operationId: string, parentId?: string, operationName?: string, correlationContextHeader?: string, traceparent?: Traceparent, tracestate?: Tracestate): CorrelationContext;
    /**
     *  Runs a function inside a given Context.
     *  All logical children of the execution path that entered this Context
     *  will receive this Context object on calls to GetCurrentContext.
     */
    static runWithContext(context: CorrelationContext, fn: () => any): any;
    /**
     * Wrapper for cls-hooked bindEmitter method
     */
    static wrapEmitter(emitter: events.EventEmitter): void;
    /**
     *  Patches a callback to restore the correct Context when getCurrentContext
     *  is run within it. This is necessary if automatic correlation fails to work
     *  with user-included libraries.
     *
     *  The supplied callback will be given the same context that was present for
     *  the call to wrapCallback.  */
    static wrapCallback<T extends Function>(fn: T): T;
    /**
     *  Enables the CorrelationContextManager.
     */
    static enable(forceClsHooked?: boolean): void;
    /**
     *  Disables the CorrelationContextManager.
     */
    static disable(): void;
    /**
     * Reset the namespace
     */
    static reset(): void;
    /**
     *  Reports if CorrelationContextManager is able to run in this environment
     */
    static isNodeVersionCompatible(): boolean;
    /**
     * We only want to use cls-hooked when it uses async_hooks api (8.2+), else
     * use async-listener (plain -cls)
     */
    static shouldUseClsHooked(): boolean;
    /**
     * A TypeError is triggered by cls-hooked for node [8.0, 8.2)
     * @internal Used in tests only
     */
    static canUseClsHooked(): boolean;
}
