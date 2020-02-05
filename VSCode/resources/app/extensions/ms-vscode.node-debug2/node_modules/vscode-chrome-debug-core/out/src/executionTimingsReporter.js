"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const utils_1 = require("./utils");
const events_1 = require("events");
exports.stepStartedEventName = 'stepStarted';
exports.milestoneReachedEventName = 'milestoneReached';
exports.stepCompletedEventName = 'stepCompleted';
exports.requestCompletedEventName = 'requestCompleted';
class StepProgressEventsEmitter extends events_1.EventEmitter {
    constructor(_nestedEmitters = []) {
        super();
        this._nestedEmitters = _nestedEmitters;
    }
    emitStepStarted(stepName) {
        this.emit(exports.stepStartedEventName, { stepName: stepName });
    }
    emitMilestoneReached(milestoneName) {
        this.emit(exports.milestoneReachedEventName, { milestoneName: milestoneName });
    }
    emitStepCompleted(stepName) {
        this.emit(exports.stepCompletedEventName, { stepName: stepName });
    }
    emitRequestCompleted(requestName, requestStartTime, timeTakenByRequestInMilliseconds) {
        this.emit(exports.requestCompletedEventName, { requestName: requestName, startTime: requestStartTime, timeTakenInMilliseconds: timeTakenByRequestInMilliseconds });
    }
    on(event, listener) {
        super.on(event, listener);
        this._nestedEmitters.forEach(nestedEmitter => nestedEmitter.on(event, listener));
        return this;
    }
    removeListener(event, listener) {
        super.removeListener(event, listener);
        this._nestedEmitters.forEach(nestedEmitter => nestedEmitter.removeListener(event, listener));
        return this;
    }
}
exports.StepProgressEventsEmitter = StepProgressEventsEmitter;
class SubscriptionManager {
    constructor() {
        this._removeSubscriptionActions = [];
    }
    on(eventEmitter, event, listener) {
        eventEmitter.on(event, listener);
        this._removeSubscriptionActions.push(() => eventEmitter.removeListener(event, listener));
    }
    removeAll() {
        for (const removeSubscriptionAction of this._removeSubscriptionActions) {
            removeSubscriptionAction();
        }
        this._removeSubscriptionActions = [];
    }
}
class ExecutionTimingsReporter {
    constructor() {
        this._eventsExecutionTimesInMilliseconds = {};
        this._stepsList = [];
        this._subscriptionManager = new SubscriptionManager();
        this._requestProperties = {};
        /* __GDPR__FRAGMENT__
           "StepNames" : {
              "BeforeFirstStep" : { "classification": "SystemMetaData", "purpose": "FeatureInsight" }
           }
         */
        this._currentStepName = 'BeforeFirstStep';
        this._currentStepStartTime = this._allStartTime = process.hrtime();
    }
    recordPreviousStepAndConfigureNewStep(newStepName) {
        this.recordTimeTaken(this._currentStepName, this._currentStepStartTime);
        this._stepsList.push(this._currentStepName);
        this._currentStepStartTime = process.hrtime();
        this._currentStepName = newStepName;
    }
    recordTimeTaken(eventName, sinceWhen) {
        const timeTakenInMilliseconds = utils_1.calculateElapsedTime(sinceWhen);
        this.addElementToArrayProperty(this._eventsExecutionTimesInMilliseconds, eventName, timeTakenInMilliseconds);
    }
    recordTotalTimeUntilMilestone(milestoneName) {
        this.recordTimeTaken(milestoneName, this._allStartTime);
    }
    generateReport() {
        /* __GDPR__FRAGMENT__
           "StepNames" : {
              "AfterLastStep" : { "classification": "SystemMetaData", "purpose": "FeatureInsight" }
           }
         */
        this.recordPreviousStepAndConfigureNewStep('AfterLastStep');
        this._subscriptionManager.removeAll(); // Remove all subscriptions so we don't get any new events
        /* __GDPR__FRAGMENT__
           "ReportProps" : {
              "Steps" : { "classification": "SystemMetaData", "purpose": "FeatureInsight" },
              "All" : { "classification": "SystemMetaData", "purpose": "FeatureInsight" },
              "${wildcard}": [
                 {
                    "${prefix}": "Request.",
                    "${classification}": { "classification": "SystemMetaData", "purpose": "FeatureInsight" }
                 }
              ],
              "${include}": [ "${RequestProperties}", "${StepNames}" ]
           }
         */
        return Object.assign({}, {
            Steps: this._stepsList,
            All: utils_1.calculateElapsedTime(this._allStartTime)
        }, this._requestProperties, this._eventsExecutionTimesInMilliseconds);
    }
    recordRequestCompleted(requestName, startTime, timeTakenInMilliseconds) {
        /* __GDPR__FRAGMENT__
           "RequestProperties" : {
              "${wildcard}": [
                 {
                    "${prefix}": "Request.",
                    "${classification}": { "classification": "SystemMetaData", "purpose": "FeatureInsight" }
                 }
              ]
           }
         */
        const propertyPrefix = `Request.${requestName}.`;
        this.addElementToArrayProperty(this._requestProperties, propertyPrefix + 'startTime', startTime);
        this.addElementToArrayProperty(this._requestProperties, propertyPrefix + 'timeTakenInMilliseconds', timeTakenInMilliseconds);
    }
    addElementToArrayProperty(object, propertyName, elementToAdd) {
        const propertiesArray = object[propertyName] = object[propertyName] || [];
        propertiesArray.push(elementToAdd);
    }
    subscribeTo(eventEmitter) {
        this._subscriptionManager.on(eventEmitter, exports.stepStartedEventName, (args) => {
            this.recordPreviousStepAndConfigureNewStep(args.stepName);
        });
        this._subscriptionManager.on(eventEmitter, exports.milestoneReachedEventName, (args) => {
            this.recordTotalTimeUntilMilestone(args.milestoneName);
        });
        this._subscriptionManager.on(eventEmitter, exports.stepCompletedEventName, (args) => {
            /* __GDPR__FRAGMENT__
               "StepNames" : {
                  "${wildcard}": [
                     {
                        "${prefix}": "WaitingAfter",
                        "${classification}": { "classification": "SystemMetaData", "purpose": "FeatureInsight" }
                     }
                  ]
               }
             */
            this.recordTotalTimeUntilMilestone(`WaitingAfter.${args.stepName}`);
        });
        this._subscriptionManager.on(eventEmitter, exports.requestCompletedEventName, (args) => {
            this.recordRequestCompleted(args.requestName, args.startTime, args.timeTakenInMilliseconds);
        });
    }
}
exports.ExecutionTimingsReporter = ExecutionTimingsReporter;

//# sourceMappingURL=executionTimingsReporter.js.map
