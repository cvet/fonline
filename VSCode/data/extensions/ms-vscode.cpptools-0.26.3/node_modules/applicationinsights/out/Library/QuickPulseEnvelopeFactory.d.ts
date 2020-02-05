import Contracts = require("../Declarations/Contracts");
import Config = require("./Config");
import Context = require("./Context");
declare class QuickPulseEnvelopeFactory {
    private static keys;
    static createQuickPulseEnvelope(metrics: Contracts.MetricQuickPulse[], documents: Contracts.DocumentQuickPulse[], config: Config, context: Context): Contracts.EnvelopeQuickPulse;
    static createQuickPulseMetric(telemetry: Contracts.MetricTelemetry): Contracts.MetricQuickPulse;
    static telemetryEnvelopeToQuickPulseDocument(envelope: Contracts.Envelope): Contracts.DocumentQuickPulse;
    private static createQuickPulseEventDocument(envelope);
    private static createQuickPulseTraceDocument(envelope);
    private static createQuickPulseExceptionDocument(envelope);
    private static createQuickPulseRequestDocument(envelope);
    private static createQuickPulseDependencyDocument(envelope);
    private static createQuickPulseDocument(envelope);
    private static aggregateProperties(envelope);
}
export = QuickPulseEnvelopeFactory;
