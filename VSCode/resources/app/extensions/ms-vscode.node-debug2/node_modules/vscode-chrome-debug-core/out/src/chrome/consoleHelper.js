"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const Color = require("color");
const variables = require("./variables");
function formatExceptionDetails(e) {
    if (!e.exception) {
        return `${e.text || 'Uncaught Error'}\n${stackTraceToString(e.stackTrace)}`;
    }
    return (e.exception.className && e.exception.className.endsWith('Error') && e.exception.description) ||
        (`Error: ${variables.getRemoteObjectPreview(e.exception)}\n${stackTraceToString(e.stackTrace)}`);
}
exports.formatExceptionDetails = formatExceptionDetails;
exports.clearConsoleCode = '\u001b[2J';
function formatConsoleArguments(type, args, stackTrace) {
    switch (type) {
        case 'log':
        case 'debug':
        case 'info':
        case 'error':
        case 'warning':
        case 'dir':
        case 'timeEnd':
        case 'count':
            args = resolveParams(args);
            break;
        case 'assert':
            const formattedParams = args.length ?
                // 'assert' doesn't support format specifiers
                resolveParams(args, /*skipFormatSpecifiers=*/ true) :
                [];
            const assertMsg = (formattedParams[0] && formattedParams[0].type === 'string') ?
                formattedParams.shift().value :
                '';
            let outputText = `Assertion failed: ${assertMsg}\n` + stackTraceToString(stackTrace);
            args = [{ type: 'string', value: outputText }, ...formattedParams];
            break;
        case 'startGroup':
        case 'startGroupCollapsed':
            let startMsg = '‹Start group›';
            const formattedGroupParams = resolveParams(args);
            if (formattedGroupParams.length && formattedGroupParams[0].type === 'string') {
                startMsg += ': ' + formattedGroupParams.shift().value;
            }
            args = [{ type: 'string', value: startMsg }, ...formattedGroupParams];
            break;
        case 'endGroup':
            args = [{ type: 'string', value: '‹End group›' }];
            break;
        case 'trace':
            args = [{ type: 'string', value: 'console.trace()\n' + stackTraceToString(stackTrace) }];
            break;
        case 'clear':
            args = [{ type: 'string', value: exports.clearConsoleCode }];
            break;
        default:
            // Some types we have to ignore
            return null;
    }
    const isError = type === 'assert' || type === 'error';
    return { args, isError };
}
exports.formatConsoleArguments = formatConsoleArguments;
/**
 * Collapse non-object arguments, and apply format specifiers (%s, %d, etc). Return a reduced a formatted list of RemoteObjects.
 */
function resolveParams(args, skipFormatSpecifiers) {
    if (!args.length || args[0].objectId) {
        // If the first arg is not text, nothing is going to happen here
        return args;
    }
    // Find all %s, %i, etc in the first argument, which is always the main text. Strip %
    let formatSpecifiers;
    const firstTextArg = args.shift();
    // currentCollapsedStringArg is the accumulated text
    let currentCollapsedStringArg = variables.getRemoteObjectPreview(firstTextArg, /*stringify=*/ false) + '';
    if (firstTextArg.type === 'string' && !skipFormatSpecifiers) {
        formatSpecifiers = (currentCollapsedStringArg.match(/\%[sidfoOc]/g) || [])
            .map(spec => spec[1]);
    }
    else {
        formatSpecifiers = [];
    }
    const processedArgs = [];
    const pushStringArg = (strArg) => {
        if (typeof strArg === 'string') {
            processedArgs.push({ type: 'string', value: strArg });
        }
    };
    // Collapse all text parameters, formatting properly if there's a format specifier
    for (let argIdx = 0; argIdx < args.length; argIdx++) {
        const arg = args[argIdx];
        const formatSpec = formatSpecifiers.shift();
        const formatted = formatArg(formatSpec, arg);
        currentCollapsedStringArg = currentCollapsedStringArg || '';
        if (typeof formatted === 'string') {
            if (formatSpec) {
                // If this param had a format specifier, search and replace it with the formatted param.
                currentCollapsedStringArg = currentCollapsedStringArg.replace('%' + formatSpec, formatted);
            }
            else {
                currentCollapsedStringArg += (currentCollapsedStringArg ? ' ' + formatted : formatted);
            }
        }
        else if (formatSpec) {
            // `formatted` is an object - split currentCollapsedStringArg around the current formatSpec and add the object
            const curSpecIdx = currentCollapsedStringArg.indexOf('%' + formatSpec);
            const processedPart = currentCollapsedStringArg.slice(0, curSpecIdx);
            if (processedPart) {
                pushStringArg(processedPart);
            }
            currentCollapsedStringArg = currentCollapsedStringArg.slice(curSpecIdx + 2);
            processedArgs.push(formatted);
        }
        else {
            pushStringArg(currentCollapsedStringArg);
            currentCollapsedStringArg = null;
            processedArgs.push(formatted);
        }
    }
    pushStringArg(currentCollapsedStringArg);
    return processedArgs;
}
function formatArg(formatSpec, arg) {
    const paramValue = String(typeof arg.value !== 'undefined' ? arg.value : arg.description);
    if (formatSpec === 's') {
        return paramValue;
    }
    else if (['i', 'd'].indexOf(formatSpec) >= 0) {
        return Math.floor(+paramValue) + '';
    }
    else if (formatSpec === 'f') {
        return +paramValue + '';
    }
    else if (formatSpec === 'c') {
        return formatColorArg(arg);
    }
    else if (formatSpec === 'O') {
        if (arg.objectId) {
            return arg;
        }
        else {
            return paramValue;
        }
    }
    else {
        // No formatSpec, or unsupported formatSpec:
        // %o - expandable DOM element
        if (arg.objectId) {
            return arg;
        }
        else {
            return paramValue;
        }
    }
}
function formatColorArg(arg) {
    const cssRegex = /\s*(.*?)\s*:\s*(.*?)\s*(?:;|$)/g;
    let escapedSequence;
    let match = cssRegex.exec(arg.value);
    while (match != null) {
        if (match.length === 3) {
            if (escapedSequence === undefined) {
                // Some valid pattern appeared, initialize escapedSequence.
                // If the pattern has no value like `color:`, then this should remain an empty string.
                escapedSequence = '';
            }
            if (match[2]) {
                switch (match[1]) {
                    case 'color':
                        const color = getAnsi16Color(match[2]);
                        if (color) {
                            escapedSequence += `;${color}`;
                        }
                        break;
                    case 'background':
                        const background = getAnsi16Color(match[2]);
                        if (background) {
                            escapedSequence += `;${background + 10}`;
                        }
                        break;
                    case 'font-weight':
                        if (match[2] === 'bold') {
                            escapedSequence += ';1';
                        }
                        break;
                    case 'text-decoration':
                        if (match[2] === 'underline') {
                            escapedSequence += ';4';
                        }
                        break;
                    default:
                }
            }
        }
        match = cssRegex.exec(arg.value);
    }
    if (typeof escapedSequence === 'string') {
        escapedSequence = `\x1b[0${escapedSequence}m`;
    }
    return escapedSequence;
}
function stackTraceToString(stackTrace) {
    if (!stackTrace) {
        return '';
    }
    return stackTrace.callFrames
        .map(frame => {
        const fnName = frame.functionName || (frame.url ? '(anonymous)' : '(eval)');
        const fileName = frame.url ? frame.url : 'eval';
        return `    at ${fnName} (${fileName}:${frame.lineNumber + 1}:${frame.columnNumber})`;
    })
        .join('\n');
}
function getAnsi16Color(colorString) {
    try {
        // Color can parse hex and color names
        const color = new Color(colorString);
        return color.ansi16().object().ansi16;
    }
    catch (ex) {
        // Unable to parse Color
        // For instance, "inherit" color will throw
    }
    return undefined;
}

//# sourceMappingURL=consoleHelper.js.map
