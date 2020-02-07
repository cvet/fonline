export declare const enum TokenType {
    class = 0,
    enum = 1,
    interface = 2,
    namespace = 3,
    typeParameter = 4,
    type = 5,
    parameter = 6,
    variable = 7,
    property = 8,
    function = 9,
    member = 10,
    _ = 11
}
export declare const enum TokenModifier {
    declaration = 0,
    static = 1,
    async = 2,
    readonly = 3,
    _ = 4
}
export declare const enum TokenEncodingConsts {
    typeOffset = 8,
    modifierMask = 255
}
export declare const enum VersionRequirement {
    major = 3,
    minor = 7
}
