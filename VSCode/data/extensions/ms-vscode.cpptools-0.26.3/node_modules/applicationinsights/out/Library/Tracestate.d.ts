/**
 * Helper class to manage parsing and strict-validation of tracestate header. W3C tracestate spec
 * is documented at https://www.w3.org/TR/trace-context/#header-value
 * @class Tracestate
 */
declare class Tracestate {
    static strict: boolean;
    fieldmap: string[];
    constructor(id?: string);
    toString(): string;
    private static validateKeyChars(key);
    private parseHeader(id);
}
export = Tracestate;
