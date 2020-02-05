export interface DocumentQuickPulse {
    __type: string;
    DocumentType: string;
    Version: string;
    OperationId: string;
    Properties: IDocumentProperty[];
}
export interface IDocumentProperty {
    key: string;
    value: string;
}
