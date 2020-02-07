"use strict";
/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
module.exports = function init(modules) {
    const ts = modules.typescript;
    function hasVersion(requiredMajor, requiredMinor) {
        const parts = ts.version.split('.');
        const majorVersion = Number(parts[0]);
        return majorVersion < requiredMajor || ((majorVersion === requiredMajor) && requiredMinor <= Number(parts[1]));
    }
    function decorate(languageService, logger) {
        var _a, _b;
        const intercept = Object.create(null);
        if (!hasVersion(3 /* major */, 7 /* minor */)) {
            (_a = logger) === null || _a === void 0 ? void 0 : _a.msg(`typescript-vscode-sh-plugin not active, version ${3 /* major */}.${7 /* minor */} required, is ${ts.version}`, ts.server.Msg.Info);
            return languageService;
        }
        (_b = logger) === null || _b === void 0 ? void 0 : _b.msg(`typescript-vscode-sh-plugin initialized. Intercepting getEncodedSemanticClassifications and getEncodedSyntacticClassifications.`, ts.server.Msg.Info);
        intercept.getEncodedSemanticClassifications = (filename, span) => {
            return {
                spans: getSemanticTokens(languageService, filename, span),
                endOfLineState: ts.EndOfLineState.None
            };
        };
        intercept.getEncodedSyntacticClassifications = (_filename, _span) => {
            return {
                spans: [],
                endOfLineState: ts.EndOfLineState.None
            };
        };
        return new Proxy(languageService, {
            get: (target, property) => {
                return intercept[property] || target[property];
            },
        });
    }
    function getSemanticTokens(jsLanguageService, fileName, span) {
        let resultTokens = [];
        const collector = (node, typeIdx, modifierSet) => {
            resultTokens.push(node.getStart(), node.getWidth(), ((typeIdx + 1) << 8 /* typeOffset */) + modifierSet);
        };
        collectTokens(jsLanguageService, fileName, span, collector);
        return resultTokens;
    }
    function collectTokens(jsLanguageService, fileName, span, collector) {
        const program = jsLanguageService.getProgram();
        if (program) {
            const typeChecker = program.getTypeChecker();
            let inJSXElement = false;
            function visit(node) {
                if (!node || !ts.textSpanIntersectsWith(span, node.pos, node.getFullWidth()) || node.getFullWidth() === 0) {
                    return;
                }
                const prevInJSXElement = inJSXElement;
                if (ts.isJsxElement(node) || ts.isJsxSelfClosingElement(node)) {
                    inJSXElement = true;
                }
                if (ts.isJsxExpression(node)) {
                    inJSXElement = false;
                }
                if (ts.isIdentifier(node) && !inJSXElement) {
                    let symbol = typeChecker.getSymbolAtLocation(node);
                    if (symbol) {
                        if (symbol.flags & ts.SymbolFlags.Alias) {
                            symbol = typeChecker.getAliasedSymbol(symbol);
                        }
                        let typeIdx = classifySymbol(symbol);
                        if (typeIdx !== undefined) {
                            let modifierSet = 0;
                            if (node.parent) {
                                const parentTypeIdx = tokenFromDeclarationMapping[node.parent.kind];
                                if (parentTypeIdx === typeIdx && node.parent.name === node) {
                                    modifierSet = 1 << 0 /* declaration */;
                                }
                            }
                            const decl = symbol.valueDeclaration;
                            const modifiers = decl ? ts.getCombinedModifierFlags(decl) : 0;
                            const nodeFlags = decl ? ts.getCombinedNodeFlags(decl) : 0;
                            if (modifiers & ts.ModifierFlags.Static) {
                                modifierSet |= 1 << 1 /* static */;
                            }
                            if (modifiers & ts.ModifierFlags.Async) {
                                modifierSet |= 1 << 2 /* async */;
                            }
                            if ((modifiers & ts.ModifierFlags.Readonly) || (nodeFlags & ts.NodeFlags.Const) || (symbol.getFlags() & ts.SymbolFlags.EnumMember)) {
                                modifierSet |= 1 << 3 /* readonly */;
                            }
                            collector(node, typeIdx, modifierSet);
                        }
                    }
                }
                ts.forEachChild(node, visit);
                inJSXElement = prevInJSXElement;
            }
            const sourceFile = program.getSourceFile(fileName);
            if (sourceFile) {
                visit(sourceFile);
            }
        }
    }
    function classifySymbol(symbol) {
        const flags = symbol.getFlags();
        if (flags & ts.SymbolFlags.Class) {
            return 0 /* class */;
        }
        else if (flags & ts.SymbolFlags.Enum) {
            return 1 /* enum */;
        }
        else if (flags & ts.SymbolFlags.TypeAlias) {
            return 5 /* type */;
        }
        else if (flags & ts.SymbolFlags.Type) {
            if (flags & ts.SymbolFlags.Interface) {
                return 2 /* interface */;
            }
            if (flags & ts.SymbolFlags.TypeParameter) {
                return 4 /* typeParameter */;
            }
        }
        const decl = symbol.valueDeclaration || symbol.declarations && symbol.declarations[0];
        return decl && tokenFromDeclarationMapping[decl.kind];
    }
    const tokenFromDeclarationMapping = {
        [ts.SyntaxKind.VariableDeclaration]: 7 /* variable */,
        [ts.SyntaxKind.Parameter]: 6 /* parameter */,
        [ts.SyntaxKind.PropertyDeclaration]: 8 /* property */,
        [ts.SyntaxKind.ModuleDeclaration]: 3 /* namespace */,
        [ts.SyntaxKind.EnumDeclaration]: 1 /* enum */,
        [ts.SyntaxKind.EnumMember]: 8 /* property */,
        [ts.SyntaxKind.ClassDeclaration]: 0 /* class */,
        [ts.SyntaxKind.MethodDeclaration]: 10 /* member */,
        [ts.SyntaxKind.FunctionDeclaration]: 9 /* function */,
        [ts.SyntaxKind.MethodSignature]: 10 /* member */,
        [ts.SyntaxKind.GetAccessor]: 8 /* property */,
        [ts.SyntaxKind.PropertySignature]: 8 /* property */,
        [ts.SyntaxKind.InterfaceDeclaration]: 2 /* interface */,
        [ts.SyntaxKind.TypeAliasDeclaration]: 5 /* type */,
        [ts.SyntaxKind.TypeParameter]: 4 /* typeParameter */
    };
    return {
        create(info) {
            return decorate(info.languageService, info.project.projectService.logger);
        },
        onConfigurationChanged(_config) {
        },
        // added for testing
        decorate(languageService) {
            return decorate(languageService);
        }
    };
};
//# sourceMappingURL=index.js.map