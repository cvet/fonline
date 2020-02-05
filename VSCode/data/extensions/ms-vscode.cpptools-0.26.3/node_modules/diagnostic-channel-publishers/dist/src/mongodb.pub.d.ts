import { IModulePatcher } from "diagnostic-channel";
export interface IMongoData {
    startedData: {
        databaseName?: string;
        command?: any;
    };
    event: {
        commandName?: string;
        duration?: number;
        failure?: string;
        reply?: any;
    };
    succeeded: boolean;
}
export declare const mongo2: IModulePatcher;
export declare const mongo3: IModulePatcher;
export declare const mongo330: IModulePatcher;
export declare function enable(): void;
