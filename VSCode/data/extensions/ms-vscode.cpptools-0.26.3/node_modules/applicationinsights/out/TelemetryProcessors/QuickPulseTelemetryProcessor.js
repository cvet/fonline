"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
var AutoCollectPerformance = require("../AutoCollection/Performance");
function quickPulseTelemetryProcessor(envelope, client) {
    if (client) {
        client.addDocument(envelope);
        // Increment rate counters
        switch (envelope.data.baseType) {
            case "ExceptionData":
                AutoCollectPerformance.countException();
                break;
            case "RequestData":
                // These are already autocounted by HttpRequest.
                // Note: Not currently counting manual trackRequest calls
                // here to avoid affecting existing autocollection metrics
                break;
            case "RemoteDependencyData":
                var baseData = envelope.data.baseData;
                AutoCollectPerformance.countDependency(baseData.duration, baseData.success);
                break;
        }
    }
    return true;
}
exports.quickPulseTelemetryProcessor = quickPulseTelemetryProcessor;
//# sourceMappingURL=QuickPulseTelemetryProcessor.js.map