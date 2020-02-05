import { DocumentQuickPulse } from "./DocumentQuickPulse";
export interface ExceptionDocumentQuickPulse extends DocumentQuickPulse {
    Exception: string;
    ExceptionMessage: string;
    ExceptionType: string;
}
