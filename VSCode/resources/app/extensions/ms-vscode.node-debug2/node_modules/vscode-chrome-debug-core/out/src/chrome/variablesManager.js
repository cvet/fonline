"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const vscode_debugadapter_1 = require("vscode-debugadapter");
const ChromeUtils = require("./chromeUtils");
const variables_1 = require("./variables");
const variables = require("./variables");
const errors = require("../errors");
const utils = require("../utils");
/**
 * A container for managing get/set variable calls
 */
class VariablesManager {
    constructor(_chromeConnection) {
        this._chromeConnection = _chromeConnection;
        this._variableHandles = new variables.VariableHandles();
    }
    get chrome() { return this._chromeConnection.api; }
    getVariables(args) {
        if (!this.chrome) {
            return utils.errP(errors.runtimeNotConnectedMsg);
        }
        const handle = this._variableHandles.get(args.variablesReference);
        if (!handle) {
            return Promise.resolve(undefined);
        }
        return handle.expand(this, args.filter, args.start, args.count)
            .catch(err => {
            vscode_debugadapter_1.logger.log('Error handling variables request: ' + err.toString());
            return [];
        }).then(variables => {
            return { variables };
        });
    }
    getVariablesForObjectId(objectId, evaluateName, filter, start, count) {
        if (typeof start === 'number' && typeof count === 'number') {
            return this.getFilteredVariablesForObject(objectId, evaluateName, filter, start, count);
        }
        filter = filter === 'indexed' ? 'all' : filter;
        return Promise.all([
            // Need to make two requests to get all properties
            this.getRuntimeProperties({ objectId, ownProperties: false, accessorPropertiesOnly: true, generatePreview: true }),
            this.getRuntimeProperties({ objectId, ownProperties: true, accessorPropertiesOnly: false, generatePreview: true })
        ]).then(getPropsResponses => {
            // Sometimes duplicates will be returned - merge all descriptors by name
            const propsByName = new Map();
            const internalPropsByName = new Map();
            getPropsResponses.forEach(response => {
                if (response) {
                    response.result.forEach(propDesc => propsByName.set(propDesc.name, propDesc));
                    if (response.internalProperties) {
                        response.internalProperties.forEach(internalProp => {
                            internalPropsByName.set(internalProp.name, internalProp);
                        });
                    }
                }
            });
            // Convert Chrome prop descriptors to DebugProtocol vars
            const variables = [];
            propsByName.forEach(propDesc => {
                if (!filter || filter === 'all' || (variables_1.isIndexedPropName(propDesc.name) === (filter === 'indexed'))) {
                    variables.push(this.propertyDescriptorToVariable(propDesc, objectId, evaluateName));
                }
            });
            internalPropsByName.forEach(internalProp => {
                if (!filter || filter === 'all' || (variables_1.isIndexedPropName(internalProp.name) === (filter === 'indexed'))) {
                    variables.push(Promise.resolve(this.internalPropertyDescriptorToVariable(internalProp, evaluateName)));
                }
            });
            return Promise.all(variables);
        }).then(variables => {
            // Sort all variables properly
            return variables.sort((var1, var2) => ChromeUtils.compareVariableNames(var1.name, var2.name));
        });
    }
    onPaused() {
        this._variableHandles.onPaused();
    }
    createHandle(value, context) {
        return this._variableHandles.create(value, context);
    }
    setPropertyValue(objectId, propName, value) {
        const setPropertyValueFn = `function() { return this["${propName}"] = ${value}; }`;
        return this.chrome.Runtime.callFunctionOn({
            objectId,
            functionDeclaration: setPropertyValueFn,
            silent: true
        }).then(response => {
            if (response.exceptionDetails) {
                const errMsg = ChromeUtils.errorMessageFromExceptionDetails(response.exceptionDetails);
                return Promise.reject(errors.errorFromEvaluate(errMsg));
            }
            else {
                // Temporary, Microsoft/vscode#12019
                return ChromeUtils.remoteObjectToValue(response.result).value;
            }
        }, error => Promise.reject(errors.errorFromEvaluate(error.message)));
    }
    getRuntimeProperties(params) {
        return this.chrome.Runtime.getProperties(params)
            .catch(err => {
            if (err.message.startsWith('Cannot find context with specified id')) {
                // Hack to ignore this error until we fix https://github.com/Microsoft/vscode/issues/18001 to not request variables at unexpected times.
                return null;
            }
            else {
                throw err;
            }
        });
    }
    getFilteredVariablesForObject(objectId, evaluateName, filter, start, count) {
        // No ES6, in case we talk to an old runtime
        const getIndexedVariablesFn = `
            function getIndexedVariables(start, count) {
                var result = [];
                for (var i = start; i < (start + count); i++) result[i] = this[i];
                return result;
            }`;
        // TODO order??
        const getNamedVariablesFn = `
            function getNamedVariablesFn(start, count) {
                var result = [];
                var ownProps = Object.getOwnPropertyNames(this);
                for (var i = start; i < (start + count); i++) result[i] = ownProps[i];
                return result;
            }`;
        const getVarsFn = filter === 'indexed' ? getIndexedVariablesFn : getNamedVariablesFn;
        return this.getFilteredVariablesForObjectId(objectId, evaluateName, getVarsFn, filter, start, count);
    }
    getFilteredVariablesForObjectId(objectId, evaluateName, getVarsFn, filter, start, count) {
        return this.chrome.Runtime.callFunctionOn({
            objectId,
            functionDeclaration: getVarsFn,
            arguments: [{ value: start }, { value: count }],
            silent: true
        }).then(evalResponse => {
            if (evalResponse.exceptionDetails) {
                const errMsg = ChromeUtils.errorMessageFromExceptionDetails(evalResponse.exceptionDetails);
                return Promise.reject(errors.errorFromEvaluate(errMsg));
            }
            else {
                // The eval was successful and returned a reference to the array object. Get the props, then filter
                // out everything except the index names.
                return this.getVariablesForObjectId(evalResponse.result.objectId, evaluateName, filter)
                    .then(variables => variables.filter(variable => variables_1.isIndexedPropName(variable.name)));
            }
        }, error => Promise.reject(errors.errorFromEvaluate(error.message)));
    }
    setVariable(args) {
        const handle = this._variableHandles.get(args.variablesReference);
        if (!handle) {
            return Promise.reject(errors.setValueNotSupported());
        }
        return handle.setValue(this, args.name, args.value)
            .then(value => ({ value }));
    }
    setVariableValue(callFrameId, scopeNumber, variableName, value) {
        let evalResultObject;
        return this.chrome.Debugger.evaluateOnCallFrame({ callFrameId, expression: value, silent: true }).then(evalResponse => {
            if (evalResponse.exceptionDetails) {
                const errMsg = ChromeUtils.errorMessageFromExceptionDetails(evalResponse.exceptionDetails);
                return Promise.reject(errors.errorFromEvaluate(errMsg));
            }
            else {
                evalResultObject = evalResponse.result;
                const newValue = ChromeUtils.remoteObjectToCallArgument(evalResultObject);
                return this.chrome.Debugger.setVariableValue({ callFrameId, scopeNumber, variableName, newValue });
            }
        }, error => Promise.reject(errors.errorFromEvaluate(error.message)))
            .then(() => ChromeUtils.remoteObjectToValue(evalResultObject).value);
    }
    createObjectVariable(name, object, parentEvaluateName, context) {
        if (object.subtype === 'internal#location') {
            // Could format this nicely later, see #110
            return Promise.resolve(variables.createPrimitiveVariableWithValue(name, 'internal#location', parentEvaluateName));
        }
        else if (object.subtype === 'null') {
            return Promise.resolve(variables.createPrimitiveVariableWithValue(name, 'null', parentEvaluateName));
        }
        const value = variables.getRemoteObjectPreview_object(object, context);
        let propCountP;
        if (object.subtype === 'array' || object.subtype === 'typedarray') {
            if (object.preview && !object.preview.overflow) {
                propCountP = Promise.resolve(variables.getArrayNumPropsByPreview(object));
            }
            else if (object.className === 'Buffer') {
                propCountP = this.getBufferNumPropsByEval(object.objectId);
            }
            else {
                propCountP = this.getArrayNumPropsByEval(object.objectId);
            }
        }
        else if (object.subtype === 'set' || object.subtype === 'map') {
            if (object.preview && !object.preview.overflow) {
                propCountP = Promise.resolve(variables.getCollectionNumPropsByPreview(object));
            }
            else {
                propCountP = this.getCollectionNumPropsByEval(object.objectId);
            }
        }
        else {
            propCountP = Promise.resolve({
                indexedVariables: undefined,
                namedVariables: undefined
            });
        }
        const evaluateName = ChromeUtils.getEvaluateName(parentEvaluateName, name);
        const variablesReference = this._variableHandles.create(variables.createPropertyContainer(object, evaluateName), context);
        return propCountP.then(({ indexedVariables, namedVariables }) => ({
            name,
            value,
            type: utils.uppercaseFirstLetter(object.type),
            variablesReference,
            indexedVariables,
            namedVariables,
            evaluateName
        }));
    }
    propertyDescriptorToVariable(propDesc, owningObjectId, parentEvaluateName) {
        return __awaiter(this, void 0, void 0, function* () {
            if (propDesc.get) {
                // Getter
                const grabGetterValue = 'function remoteFunction(propName) { return this[propName]; }';
                let response;
                try {
                    response = yield this.chrome.Runtime.callFunctionOn({
                        objectId: owningObjectId,
                        functionDeclaration: grabGetterValue,
                        arguments: [{ value: propDesc.name }]
                    });
                }
                catch (error) {
                    vscode_debugadapter_1.logger.error(`Error evaluating getter for '{propDesc.name}' - {error.toString()}`);
                    return { name: propDesc.name, value: error.toString(), variablesReference: 0 };
                }
                if (response.exceptionDetails) {
                    // Not an error, getter could be `get foo() { throw new Error('bar'); }`
                    const exceptionMessage = ChromeUtils.errorMessageFromExceptionDetails(response.exceptionDetails);
                    vscode_debugadapter_1.logger.verbose('Exception thrown evaluating getter - ' + exceptionMessage);
                    return { name: propDesc.name, value: exceptionMessage, variablesReference: 0 };
                }
                else {
                    return this.remoteObjectToVariable(propDesc.name, response.result, parentEvaluateName);
                }
            }
            else if (propDesc.set) {
                // setter without a getter, unlikely
                return { name: propDesc.name, value: 'setter', variablesReference: 0 };
            }
            else {
                // Non getter/setter
                return this.internalPropertyDescriptorToVariable(propDesc, parentEvaluateName);
            }
        });
    }
    getArrayNumPropsByEval(objectId) {
        // +2 for __proto__ and length
        const getNumPropsFn = `function() { return [this.length, Object.keys(this).length - this.length + 2]; }`;
        return this.getNumPropsByEval(objectId, getNumPropsFn);
    }
    getBufferNumPropsByEval(objectId) {
        // +2 for __proto__ and length
        // Object.keys doesn't return other props from a Buffer
        const getNumPropsFn = `function() { return [this.length, 0]; }`;
        return this.getNumPropsByEval(objectId, getNumPropsFn);
    }
    getCollectionNumPropsByEval(objectId) {
        const getNumPropsFn = `function() { return [0, Object.keys(this).length + 1]; }`; // +1 for [[Entries]];
        return this.getNumPropsByEval(objectId, getNumPropsFn);
    }
    getNumPropsByEval(objectId, getNumPropsFn) {
        return this.chrome.Runtime.callFunctionOn({
            objectId,
            functionDeclaration: getNumPropsFn,
            silent: true,
            returnByValue: true
        }).then(response => {
            if (response.exceptionDetails) {
                const errMsg = ChromeUtils.errorMessageFromExceptionDetails(response.exceptionDetails);
                return Promise.reject(errors.errorFromEvaluate(errMsg));
            }
            else {
                const resultProps = response.result.value;
                if (resultProps.length !== 2) {
                    return Promise.reject(errors.errorFromEvaluate('Did not get expected props, got ' + JSON.stringify(resultProps)));
                }
                return { indexedVariables: resultProps[0], namedVariables: resultProps[1] };
            }
        }, error => Promise.reject(errors.errorFromEvaluate(error.message)));
    }
    remoteObjectToVariable(name, object, parentEvaluateName, stringify = true, context = 'variables') {
        return __awaiter(this, void 0, void 0, function* () {
            name = name || '""';
            if (object) {
                if (object.type === 'object') {
                    return this.createObjectVariable(name, object, parentEvaluateName, context);
                }
                else if (object.type === 'function') {
                    return variables.createFunctionVariable(name, object, context, this._variableHandles, parentEvaluateName);
                }
                else {
                    return variables.createPrimitiveVariable(name, object, parentEvaluateName, stringify);
                }
            }
            else {
                return variables.createPrimitiveVariableWithValue(name, '', parentEvaluateName);
            }
        });
    }
    internalPropertyDescriptorToVariable(propDesc, parentEvaluateName) {
        return this.remoteObjectToVariable(propDesc.name, propDesc.value, parentEvaluateName);
    }
}
exports.VariablesManager = VariablesManager;

//# sourceMappingURL=variablesManager.js.map
