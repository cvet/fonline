"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const vscode_debugadapter_1 = require("vscode-debugadapter");
const utils_1 = require("./utils");
class TelemetryReporter {
    constructor() {
        this._globalTelemetryProperties = {};
    }
    reportEvent(name, data) {
        if (this._sendEvent) {
            const combinedData = Object.assign({}, this._globalTelemetryProperties, data);
            const event = new vscode_debugadapter_1.OutputEvent(name, 'telemetry', combinedData);
            this._sendEvent(event);
        }
    }
    setupEventHandler(_sendEvent) {
        this._sendEvent = _sendEvent;
    }
    addCustomGlobalProperty(additionalGlobalTelemetryProperties) {
        Object.assign(this._globalTelemetryProperties, additionalGlobalTelemetryProperties);
    }
}
exports.TelemetryReporter = TelemetryReporter;
// If you add an async global property, all events after that will include it
class AsyncGlobalPropertiesTelemetryReporter {
    constructor(_telemetryReporter) {
        this._telemetryReporter = _telemetryReporter;
        this._actionsQueue = Promise.resolve();
        // We just store the parameter
    }
    reportEvent(name, data) {
        /*
         * TODO: Put this code back after VS stops dropping telemetry events that happen after fatal errors, and disconnecting...
         * VS has a bug where it drops telemetry events that happen after a fatal error, or after the DA starts disconnecting. Our
         * temporary workaround is to make telemetry sync, so it'll likely be sent before we send the fatal errors, etc...
        this._actionsQueue = this._actionsQueue.then(() => // We block the report event until all the addCustomGlobalProperty have finished
            this._telemetryReporter.reportEvent(name, data));
         */
        this._telemetryReporter.reportEvent(name, data);
    }
    setupEventHandler(_sendEvent) {
        this._telemetryReporter.setupEventHandler(_sendEvent);
    }
    addCustomGlobalProperty(additionalGlobalPropertiesPromise) {
        const reportedPropertyP = Promise.resolve(additionalGlobalPropertiesPromise).then(property => this._telemetryReporter.addCustomGlobalProperty(property), rejection => this.reportErrorWhileWaitingForProperty(rejection));
        this._actionsQueue = Promise.all([this._actionsQueue, reportedPropertyP]);
    }
    reportErrorWhileWaitingForProperty(rejection) {
        let properties = {};
        properties.successful = 'false';
        properties.exceptionType = 'firstChance';
        utils_1.fillErrorDetails(properties, rejection);
        /* __GDPR__
           "error-while-adding-custom-global-property" : {
             "${include}": [
                 "${IExecutionResultTelemetryProperties}"
             ]
           }
         */
        this._telemetryReporter.reportEvent('error-while-adding-custom-global-property', properties);
    }
}
exports.AsyncGlobalPropertiesTelemetryReporter = AsyncGlobalPropertiesTelemetryReporter;
class NullTelemetryReporter {
    reportEvent(name, data) {
        // no-op
    }
    setupEventHandler(_sendEvent) {
        // no-op
    }
}
exports.NullTelemetryReporter = NullTelemetryReporter;
exports.DefaultTelemetryIntervalInMilliseconds = 10000;
class BatchTelemetryReporter {
    constructor(_telemetryReporter, _cadenceInMilliseconds = exports.DefaultTelemetryIntervalInMilliseconds) {
        this._telemetryReporter = _telemetryReporter;
        this._cadenceInMilliseconds = _cadenceInMilliseconds;
        this.reset();
        this.setup();
    }
    reportEvent(name, data) {
        if (!this._eventBuckets[name]) {
            this._eventBuckets[name] = [];
        }
        this._eventBuckets[name].push(data);
    }
    finalize() {
        this.send();
        clearInterval(this._timer);
    }
    setup() {
        this._timer = setInterval(() => this.send(), this._cadenceInMilliseconds);
    }
    reset() {
        this._eventBuckets = {};
    }
    send() {
        for (const eventName in this._eventBuckets) {
            const bucket = this._eventBuckets[eventName];
            let properties = BatchTelemetryReporter.transfromBucketData(bucket);
            this._telemetryReporter.reportEvent(eventName, properties);
        }
        this.reset();
    }
    /**
     * Transfrom the bucket of events data from the form:
     * [{
     *  p1: v1,
     *  p2: v2
     * },
     * {
     *  p1: w1,
     *  p2: w2
     *  p3: w3
     * }]
     *
     * to
     * {
     *   p1: [v1,   w1],
     *   p2: [v2,   w2],
     *   p3: [null, w3]
     * }
     *
     *
     * The later form is easier for downstream telemetry analysis.
     */
    static transfromBucketData(bucketForEventType) {
        const allPropertyNamesInTheBucket = BatchTelemetryReporter.collectPropertyNamesFromAllEvents(bucketForEventType);
        let properties = {};
        // Create a holder for all potential property names.
        for (const key of allPropertyNamesInTheBucket) {
            properties[`aggregated.${key}`] = [];
        }
        // Run through all the events in the bucket, collect the values for each property name.
        for (const event of bucketForEventType) {
            for (const propertyName of allPropertyNamesInTheBucket) {
                properties[`aggregated.${propertyName}`].push(event[propertyName] === undefined ? null : event[propertyName]);
            }
        }
        // Serialize each array as the final aggregated property value.
        for (const propertyName of allPropertyNamesInTheBucket) {
            properties[`aggregated.${propertyName}`] = JSON.stringify(properties[`aggregated.${propertyName}`]);
        }
        return properties;
    }
    /**
     * Get the property keys from all the entries of a event bucket:
     *
     * So
     * [{
     *  p1: v1,
     *  p2: v2
     * },
     * {
     *  p1: w1,
     *  p2: w2
     *  p3: w3
     * }]
     *
     * will return ['p1', 'p2', 'p3']
     */
    static collectPropertyNamesFromAllEvents(bucket) {
        let propertyNamesSet = {};
        for (const entry of bucket) {
            for (const key of Object.keys(entry)) {
                propertyNamesSet[key] = true;
            }
        }
        return Object.keys(propertyNamesSet);
    }
}
exports.BatchTelemetryReporter = BatchTelemetryReporter;
class TelemetryPropertyCollector {
    constructor() {
        this._properties = {};
    }
    getProperties() {
        return this._properties;
    }
    addTelemetryProperty(propertyName, value) {
        this._properties[propertyName] = value;
    }
}
exports.TelemetryPropertyCollector = TelemetryPropertyCollector;
exports.telemetry = new AsyncGlobalPropertiesTelemetryReporter(new TelemetryReporter());

//# sourceMappingURL=telemetry.js.map
