"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const vscode_debugadapter_1 = require("vscode-debugadapter");
const utils = require("../utils");
const ChromeUtils = require("./chromeUtils");
class BaseVariableContainer {
    constructor(objectId, evaluateName) {
        this.objectId = objectId;
        this.evaluateName = evaluateName;
    }
    expand(variablesManager, filter, start, count) {
        return variablesManager.getVariablesForObjectId(this.objectId, this.evaluateName, filter, start, count);
    }
    setValue(variablesManager, name, value) {
        return utils.errP('setValue not supported by this variable type');
    }
}
exports.BaseVariableContainer = BaseVariableContainer;
class PropertyContainer extends BaseVariableContainer {
    setValue(variablesManager, name, value) {
        return variablesManager.setPropertyValue(this.objectId, name, value);
    }
}
exports.PropertyContainer = PropertyContainer;
class LoggedObjects extends BaseVariableContainer {
    constructor(args) {
        super(undefined);
        this.args = args;
    }
    expand(variablesManager, filter, start, count) {
        return Promise.all(this.args.map((arg, i) => variablesManager.remoteObjectToVariable('' + i, arg, undefined, /*stringify=*/ false, 'repl')));
    }
}
exports.LoggedObjects = LoggedObjects;
class ScopeContainer extends BaseVariableContainer {
    constructor(frameId, origScopeIndex, objectId, thisObj, returnValue) {
        super(objectId, '');
        this._thisObj = thisObj;
        this._returnValue = returnValue;
        this._frameId = frameId;
        this._origScopeIndex = origScopeIndex;
    }
    /**
     * Call super then insert the 'this' object if needed
     */
    expand(variablesManager, filter, start, count) {
        // No filtering in scopes right now
        return super.expand(variablesManager, 'all', start, count).then(variables => {
            if (this._thisObj) {
                // If this is a scope that should have the 'this', prop, insert it at the top of the list
                return this.insertRemoteObject(variablesManager, variables, 'this', this._thisObj);
            }
            return variables;
        }).then(variables => {
            if (this._returnValue) {
                return this.insertRemoteObject(variablesManager, variables, 'Return value', this._returnValue);
            }
            return variables;
        });
    }
    setValue(variablesManager, name, value) {
        return variablesManager.setVariableValue(this._frameId, this._origScopeIndex, name, value);
    }
    insertRemoteObject(variablesManager, variables, name, obj) {
        return variablesManager.remoteObjectToVariable(name, obj).then(variable => {
            variables.unshift(variable);
            return variables;
        });
    }
}
exports.ScopeContainer = ScopeContainer;
class ExceptionContainer extends PropertyContainer {
    constructor(objectId, exception) {
        super(exception.objectId, undefined);
        this._exception = exception;
    }
    /**
     * Expand the exception as if it were a Scope
     */
    static create(exception) {
        return exception.objectId ?
            new ExceptionContainer(exception.objectId, exception) :
            new ExceptionValueContainer(exception);
    }
}
exports.ExceptionContainer = ExceptionContainer;
/**
 * For when a value is thrown instead of an object
 */
class ExceptionValueContainer extends ExceptionContainer {
    constructor(exception) {
        super('EXCEPTION_ID', exception);
    }
    /**
     * Make up a fake 'Exception' property to hold the thrown value, displayed under the Exception Scope
     */
    expand(variablesManager, filter, start, count) {
        const excValuePropDescriptor = { name: 'Exception', value: this._exception };
        return variablesManager.propertyDescriptorToVariable(excValuePropDescriptor)
            .then(variable => [variable]);
    }
}
exports.ExceptionValueContainer = ExceptionValueContainer;
function isIndexedPropName(name) {
    return !!name.match(/^\d+$/);
}
exports.isIndexedPropName = isIndexedPropName;
const PREVIEW_PROPS_DEFAULT = 3;
const PREVIEW_PROPS_CONSOLE = 8;
const PREVIEW_PROP_LENGTH = 50;
const ELLIPSIS = '…';
function getArrayPreview(object, context) {
    let value = object.description;
    if (object.preview) {
        const numProps = context === 'repl' ? PREVIEW_PROPS_CONSOLE : PREVIEW_PROPS_DEFAULT;
        const indexedProps = object.preview.properties
            .filter(prop => isIndexedPropName(prop.name));
        // Take the first 3 props, and parse the indexes
        const propsWithIdx = indexedProps.slice(0, numProps)
            .map((prop, i) => {
            return {
                idx: parseInt(prop.name, 10),
                value: propertyPreviewToString(prop)
            };
        });
        // Insert ... where there are undefined indexes
        const propValues = [];
        for (let i = 0; i < propsWithIdx.length; i++) {
            const prop = propsWithIdx[i];
            const prevIdx = i === 0 ? -1 : propsWithIdx[i - 1].idx;
            if (prop.idx > prevIdx + 1) {
                propValues.push(ELLIPSIS);
            }
            propValues.push(prop.value);
        }
        let propsPreview = propValues.join(', ');
        if (object.preview.overflow || indexedProps.length > numProps) {
            propsPreview += ', ' + ELLIPSIS;
        }
        value += ` [${propsPreview}]`;
    }
    return value;
}
function getObjectPreview(object, context) {
    let value = object.description;
    if (object.preview) {
        const numProps = context === 'repl' ? PREVIEW_PROPS_CONSOLE : PREVIEW_PROPS_DEFAULT;
        const props = object.preview.properties.slice(0, numProps);
        let propsPreview = props
            .map(prop => {
            const name = prop.name || `""`;
            return `${name}: ${propertyPreviewToString(prop)}`;
        })
            .join(', ');
        if (object.preview.overflow || object.preview.properties.length > numProps) {
            propsPreview += ', …';
        }
        value += ` {${propsPreview}}`;
    }
    return value;
}
function propertyPreviewToString(prop) {
    const value = typeof prop.value === 'undefined' ?
        `<${prop.type}>` :
        trimProperty(prop.value);
    return prop.type === 'string' ?
        `"${value}"` :
        value;
}
function trimProperty(value) {
    return (value !== undefined && value !== null && value.length > PREVIEW_PROP_LENGTH) ?
        value.substr(0, PREVIEW_PROP_LENGTH) + ELLIPSIS :
        value;
}
function getRemoteObjectPreview(object, stringify = true, context) {
    if (object) {
        if (object.type === 'object') {
            return getRemoteObjectPreview_object(object, context);
        }
        else if (object.type === 'function') {
            return getRemoteObjectPreview_function(object, context);
        }
        else {
            return getRemoteObjectPreview_primitive(object, stringify);
        }
    }
    return '';
}
exports.getRemoteObjectPreview = getRemoteObjectPreview;
function getRemoteObjectPreview_object(object, context) {
    const objectDescription = object.description || '';
    if (object.subtype === 'internal#location') {
        // Could format this nicely later, see #110
        return 'internal#location';
    }
    else if (object.subtype === 'null') {
        return 'null';
    }
    else if (object.subtype === 'array' || object.subtype === 'typedarray') {
        return getArrayPreview(object, context);
    }
    else if (object.subtype === 'error') {
        // The Error's description contains the whole stack which is not a nice description.
        // Up to the first newline is just the error name/message.
        const firstNewlineIdx = objectDescription.indexOf('\n');
        return firstNewlineIdx >= 0 ?
            objectDescription.substr(0, firstNewlineIdx) :
            objectDescription;
    }
    else if (object.subtype === 'promise' && object.preview) {
        const promiseStatus = object.preview.properties.filter(prop => prop.name === '[[PromiseStatus]]')[0];
        return promiseStatus ?
            objectDescription + ' { ' + promiseStatus.value + ' }' :
            objectDescription;
    }
    else if (object.subtype === 'generator' && object.preview) {
        const generatorStatus = object.preview.properties.filter(prop => prop.name === '[[GeneratorStatus]]')[0];
        return generatorStatus ?
            objectDescription + ' { ' + generatorStatus.value + ' }' :
            objectDescription;
    }
    else if (object.type === 'object' && object.preview) {
        return getObjectPreview(object, context);
    }
    else {
        return objectDescription;
    }
}
exports.getRemoteObjectPreview_object = getRemoteObjectPreview_object;
function getRemoteObjectPreview_primitive(object, stringify) {
    // The value is a primitive value, or something that has a description (not object, primitive, or undefined). And force to be string
    if (typeof object.value === 'undefined') {
        return object.description + '';
    }
    else if (object.type === 'number') {
        // .value is truncated, so use .description, the full string representation
        // Should be like '3' or 'Infinity'.
        return object.description;
    }
    else if (object.type === 'boolean') {
        // Never stringified
        return '' + object.value;
    }
    else {
        return stringify ? `"${object.value}"` : object.value;
    }
}
exports.getRemoteObjectPreview_primitive = getRemoteObjectPreview_primitive;
function getRemoteObjectPreview_function(object, context) {
    const firstBraceIdx = object.description.indexOf('{');
    if (firstBraceIdx >= 0) {
        return object.description.substring(0, firstBraceIdx) + '{ … }';
    }
    else {
        const firstArrowIdx = object.description.indexOf('=>');
        return firstArrowIdx >= 0 ?
            object.description.substring(0, firstArrowIdx + 2) + ' …' :
            object.description;
    }
}
exports.getRemoteObjectPreview_function = getRemoteObjectPreview_function;
class VariableHandles {
    constructor() {
        this._variableHandles = new vscode_debugadapter_1.Handles(1);
        this._consoleVariableHandles = new vscode_debugadapter_1.Handles(1e5);
    }
    onPaused() {
        // Only reset the variableHandles, the console vars are still visible
        this._variableHandles.reset();
    }
    create(value, context = 'variables') {
        return this.getHandles(context).create(value);
    }
    get(handle) {
        return this._variableHandles.get(handle) || this._consoleVariableHandles.get(handle);
    }
    getHandles(context) {
        return context === 'repl' ?
            this._consoleVariableHandles :
            this._variableHandles;
    }
}
exports.VariableHandles = VariableHandles;
function getCollectionNumPropsByPreview(object) {
    let indexedVariables = 0;
    let namedVariables = object.preview.properties.length + 1; // +1 for [[Entries]];
    return { indexedVariables, namedVariables };
}
exports.getCollectionNumPropsByPreview = getCollectionNumPropsByPreview;
function getArrayNumPropsByPreview(object) {
    let indexedVariables = 0;
    const indexedProps = object.preview.properties
        .filter(prop => isIndexedPropName(prop.name));
    if (indexedProps.length) {
        // +1 because (last index=0) => 1 prop
        indexedVariables = parseInt(indexedProps[indexedProps.length - 1].name, 10) + 1;
    }
    const namedVariables = object.preview.properties.length - indexedProps.length + 2; // 2 for __proto__ and length
    return { indexedVariables, namedVariables };
}
exports.getArrayNumPropsByPreview = getArrayNumPropsByPreview;
function createPrimitiveVariableWithValue(name, value, parentEvaluateName) {
    return {
        name,
        value,
        variablesReference: 0,
        evaluateName: ChromeUtils.getEvaluateName(parentEvaluateName, name)
    };
}
exports.createPrimitiveVariableWithValue = createPrimitiveVariableWithValue;
function createPropertyContainer(object, evaluateName) {
    return new PropertyContainer(object.objectId, evaluateName);
}
exports.createPropertyContainer = createPropertyContainer;
function createPrimitiveVariable(name, object, parentEvaluateName, stringify) {
    const value = getRemoteObjectPreview_primitive(object, stringify);
    const variable = createPrimitiveVariableWithValue(name, value, parentEvaluateName);
    variable.type = object.type;
    return variable;
}
exports.createPrimitiveVariable = createPrimitiveVariable;
function createFunctionVariable(name, object, context, handles, parentEvaluateName) {
    let value;
    const firstBraceIdx = object.description.indexOf('{');
    if (firstBraceIdx >= 0) {
        value = object.description.substring(0, firstBraceIdx) + '{ … }';
    }
    else {
        const firstArrowIdx = object.description.indexOf('=>');
        value = firstArrowIdx >= 0 ?
            object.description.substring(0, firstArrowIdx + 2) + ' …' :
            object.description;
    }
    const evaluateName = ChromeUtils.getEvaluateName(parentEvaluateName, name);
    return {
        name,
        value,
        type: utils.uppercaseFirstLetter(object.type),
        variablesReference: handles.create(new PropertyContainer(object.objectId, evaluateName), context),
        evaluateName
    };
}
exports.createFunctionVariable = createFunctionVariable;

//# sourceMappingURL=variables.js.map
