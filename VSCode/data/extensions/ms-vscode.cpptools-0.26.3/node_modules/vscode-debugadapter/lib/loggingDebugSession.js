"use strict";
/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
const Logger = require("./logger");
const logger = Logger.logger;
const debugSession_1 = require("./debugSession");
class LoggingDebugSession extends debugSession_1.DebugSession {
    constructor(obsolete_logFilePath, obsolete_debuggerLinesAndColumnsStartAt1, obsolete_isServer) {
        super(obsolete_debuggerLinesAndColumnsStartAt1, obsolete_isServer);
        this.obsolete_logFilePath = obsolete_logFilePath;
        this.on('error', (event) => {
            logger.error(event.body);
        });
    }
    start(inStream, outStream) {
        super.start(inStream, outStream);
        logger.init(e => this.sendEvent(e), this.obsolete_logFilePath, this._isServer);
    }
    /**
     * Overload sendEvent to log
     */
    sendEvent(event) {
        if (!(event instanceof Logger.LogOutputEvent)) {
            // Don't create an infinite loop...
            let objectToLog = event;
            if (event instanceof debugSession_1.OutputEvent && event.body && event.body.data && event.body.data.doNotLogOutput) {
                delete event.body.data.doNotLogOutput;
                objectToLog = Object.assign({}, event);
                objectToLog.body = Object.assign({}, event.body, { output: '<output not logged>' });
            }
            logger.verbose(`To client: ${JSON.stringify(objectToLog)}`);
        }
        super.sendEvent(event);
    }
    /**
     * Overload sendRequest to log
     */
    sendRequest(command, args, timeout, cb) {
        logger.verbose(`To client: ${JSON.stringify(command)}(${JSON.stringify(args)}), timeout: ${timeout}`);
        super.sendRequest(command, args, timeout, cb);
    }
    /**
     * Overload sendResponse to log
     */
    sendResponse(response) {
        logger.verbose(`To client: ${JSON.stringify(response)}`);
        super.sendResponse(response);
    }
    dispatchRequest(request) {
        logger.verbose(`From client: ${request.command}(${JSON.stringify(request.arguments)})`);
        super.dispatchRequest(request);
    }
}
exports.LoggingDebugSession = LoggingDebugSession;
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoibG9nZ2luZ0RlYnVnU2Vzc2lvbi5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbIi4uL3NyYy9sb2dnaW5nRGVidWdTZXNzaW9uLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7QUFBQTs7O2dHQUdnRzs7QUFJaEcsbUNBQW1DO0FBQ25DLE1BQU0sTUFBTSxHQUFHLE1BQU0sQ0FBQyxNQUFNLENBQUM7QUFDN0IsaURBQXlEO0FBRXpELE1BQWEsbUJBQW9CLFNBQVEsMkJBQVk7SUFDcEQsWUFBMkIsb0JBQTZCLEVBQUUsd0NBQWtELEVBQUUsaUJBQTJCO1FBQ3hJLEtBQUssQ0FBQyx3Q0FBd0MsRUFBRSxpQkFBaUIsQ0FBQyxDQUFDO1FBRHpDLHlCQUFvQixHQUFwQixvQkFBb0IsQ0FBUztRQUd2RCxJQUFJLENBQUMsRUFBRSxDQUFDLE9BQU8sRUFBRSxDQUFDLEtBQTBCLEVBQUUsRUFBRTtZQUMvQyxNQUFNLENBQUMsS0FBSyxDQUFDLEtBQUssQ0FBQyxJQUFJLENBQUMsQ0FBQztRQUMxQixDQUFDLENBQUMsQ0FBQztJQUNKLENBQUM7SUFFTSxLQUFLLENBQUMsUUFBK0IsRUFBRSxTQUFnQztRQUM3RSxLQUFLLENBQUMsS0FBSyxDQUFDLFFBQVEsRUFBRSxTQUFTLENBQUMsQ0FBQztRQUNqQyxNQUFNLENBQUMsSUFBSSxDQUFDLENBQUMsQ0FBQyxFQUFFLENBQUMsSUFBSSxDQUFDLFNBQVMsQ0FBQyxDQUFDLENBQUMsRUFBRSxJQUFJLENBQUMsb0JBQW9CLEVBQUUsSUFBSSxDQUFDLFNBQVMsQ0FBQyxDQUFDO0lBQ2hGLENBQUM7SUFFRDs7T0FFRztJQUNJLFNBQVMsQ0FBQyxLQUEwQjtRQUMxQyxJQUFJLENBQUMsQ0FBQyxLQUFLLFlBQVksTUFBTSxDQUFDLGNBQWMsQ0FBQyxFQUFFO1lBQzlDLG1DQUFtQztZQUVuQyxJQUFJLFdBQVcsR0FBRyxLQUFLLENBQUM7WUFDeEIsSUFBSSxLQUFLLFlBQVksMEJBQVcsSUFBSSxLQUFLLENBQUMsSUFBSSxJQUFJLEtBQUssQ0FBQyxJQUFJLENBQUMsSUFBSSxJQUFJLEtBQUssQ0FBQyxJQUFJLENBQUMsSUFBSSxDQUFDLGNBQWMsRUFBRTtnQkFDcEcsT0FBTyxLQUFLLENBQUMsSUFBSSxDQUFDLElBQUksQ0FBQyxjQUFjLENBQUM7Z0JBQ3RDLFdBQVcscUJBQVEsS0FBSyxDQUFFLENBQUM7Z0JBQzNCLFdBQVcsQ0FBQyxJQUFJLHFCQUFRLEtBQUssQ0FBQyxJQUFJLElBQUUsTUFBTSxFQUFFLHFCQUFxQixHQUFFLENBQUE7YUFDbkU7WUFFRCxNQUFNLENBQUMsT0FBTyxDQUFDLGNBQWMsSUFBSSxDQUFDLFNBQVMsQ0FBQyxXQUFXLENBQUMsRUFBRSxDQUFDLENBQUM7U0FDNUQ7UUFFRCxLQUFLLENBQUMsU0FBUyxDQUFDLEtBQUssQ0FBQyxDQUFDO0lBQ3hCLENBQUM7SUFFRDs7T0FFRztJQUNJLFdBQVcsQ0FBQyxPQUFlLEVBQUUsSUFBUyxFQUFFLE9BQWUsRUFBRSxFQUE4QztRQUM3RyxNQUFNLENBQUMsT0FBTyxDQUFDLGNBQWMsSUFBSSxDQUFDLFNBQVMsQ0FBQyxPQUFPLENBQUMsSUFBSSxJQUFJLENBQUMsU0FBUyxDQUFDLElBQUksQ0FBQyxlQUFlLE9BQU8sRUFBRSxDQUFDLENBQUM7UUFDdEcsS0FBSyxDQUFDLFdBQVcsQ0FBQyxPQUFPLEVBQUUsSUFBSSxFQUFFLE9BQU8sRUFBRSxFQUFFLENBQUMsQ0FBQztJQUMvQyxDQUFDO0lBRUQ7O09BRUc7SUFDSSxZQUFZLENBQUMsUUFBZ0M7UUFDbkQsTUFBTSxDQUFDLE9BQU8sQ0FBQyxjQUFjLElBQUksQ0FBQyxTQUFTLENBQUMsUUFBUSxDQUFDLEVBQUUsQ0FBQyxDQUFDO1FBQ3pELEtBQUssQ0FBQyxZQUFZLENBQUMsUUFBUSxDQUFDLENBQUM7SUFDOUIsQ0FBQztJQUVTLGVBQWUsQ0FBQyxPQUE4QjtRQUN2RCxNQUFNLENBQUMsT0FBTyxDQUFDLGdCQUFnQixPQUFPLENBQUMsT0FBTyxJQUFJLElBQUksQ0FBQyxTQUFTLENBQUMsT0FBTyxDQUFDLFNBQVMsQ0FBRSxHQUFHLENBQUMsQ0FBQztRQUN6RixLQUFLLENBQUMsZUFBZSxDQUFDLE9BQU8sQ0FBQyxDQUFDO0lBQ2hDLENBQUM7Q0FDRDtBQXRERCxrREFzREMiLCJzb3VyY2VzQ29udGVudCI6WyIvKi0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLVxuICogIENvcHlyaWdodCAoYykgTWljcm9zb2Z0IENvcnBvcmF0aW9uLiBBbGwgcmlnaHRzIHJlc2VydmVkLlxuICogIExpY2Vuc2VkIHVuZGVyIHRoZSBNSVQgTGljZW5zZS4gU2VlIExpY2Vuc2UudHh0IGluIHRoZSBwcm9qZWN0IHJvb3QgZm9yIGxpY2Vuc2UgaW5mb3JtYXRpb24uXG4gKi0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tKi9cblxuaW1wb3J0IHtEZWJ1Z1Byb3RvY29sfSBmcm9tICd2c2NvZGUtZGVidWdwcm90b2NvbCc7XG5cbmltcG9ydCAqIGFzIExvZ2dlciBmcm9tICcuL2xvZ2dlcic7XG5jb25zdCBsb2dnZXIgPSBMb2dnZXIubG9nZ2VyO1xuaW1wb3J0IHtEZWJ1Z1Nlc3Npb24sIE91dHB1dEV2ZW50fSBmcm9tICcuL2RlYnVnU2Vzc2lvbic7XG5cbmV4cG9ydCBjbGFzcyBMb2dnaW5nRGVidWdTZXNzaW9uIGV4dGVuZHMgRGVidWdTZXNzaW9uIHtcblx0cHVibGljIGNvbnN0cnVjdG9yKHByaXZhdGUgb2Jzb2xldGVfbG9nRmlsZVBhdGg/OiBzdHJpbmcsIG9ic29sZXRlX2RlYnVnZ2VyTGluZXNBbmRDb2x1bW5zU3RhcnRBdDE/OiBib29sZWFuLCBvYnNvbGV0ZV9pc1NlcnZlcj86IGJvb2xlYW4pIHtcblx0XHRzdXBlcihvYnNvbGV0ZV9kZWJ1Z2dlckxpbmVzQW5kQ29sdW1uc1N0YXJ0QXQxLCBvYnNvbGV0ZV9pc1NlcnZlcik7XG5cblx0XHR0aGlzLm9uKCdlcnJvcicsIChldmVudDogRGVidWdQcm90b2NvbC5FdmVudCkgPT4ge1xuXHRcdFx0bG9nZ2VyLmVycm9yKGV2ZW50LmJvZHkpO1xuXHRcdH0pO1xuXHR9XG5cblx0cHVibGljIHN0YXJ0KGluU3RyZWFtOiBOb2RlSlMuUmVhZGFibGVTdHJlYW0sIG91dFN0cmVhbTogTm9kZUpTLldyaXRhYmxlU3RyZWFtKTogdm9pZCB7XG5cdFx0c3VwZXIuc3RhcnQoaW5TdHJlYW0sIG91dFN0cmVhbSk7XG5cdFx0bG9nZ2VyLmluaXQoZSA9PiB0aGlzLnNlbmRFdmVudChlKSwgdGhpcy5vYnNvbGV0ZV9sb2dGaWxlUGF0aCwgdGhpcy5faXNTZXJ2ZXIpO1xuXHR9XG5cblx0LyoqXG5cdCAqIE92ZXJsb2FkIHNlbmRFdmVudCB0byBsb2dcblx0ICovXG5cdHB1YmxpYyBzZW5kRXZlbnQoZXZlbnQ6IERlYnVnUHJvdG9jb2wuRXZlbnQpOiB2b2lkIHtcblx0XHRpZiAoIShldmVudCBpbnN0YW5jZW9mIExvZ2dlci5Mb2dPdXRwdXRFdmVudCkpIHtcblx0XHRcdC8vIERvbid0IGNyZWF0ZSBhbiBpbmZpbml0ZSBsb29wLi4uXG5cblx0XHRcdGxldCBvYmplY3RUb0xvZyA9IGV2ZW50O1xuXHRcdFx0aWYgKGV2ZW50IGluc3RhbmNlb2YgT3V0cHV0RXZlbnQgJiYgZXZlbnQuYm9keSAmJiBldmVudC5ib2R5LmRhdGEgJiYgZXZlbnQuYm9keS5kYXRhLmRvTm90TG9nT3V0cHV0KSB7XG5cdFx0XHRcdGRlbGV0ZSBldmVudC5ib2R5LmRhdGEuZG9Ob3RMb2dPdXRwdXQ7XG5cdFx0XHRcdG9iamVjdFRvTG9nID0geyAuLi5ldmVudCB9O1xuXHRcdFx0XHRvYmplY3RUb0xvZy5ib2R5ID0geyAuLi5ldmVudC5ib2R5LCBvdXRwdXQ6ICc8b3V0cHV0IG5vdCBsb2dnZWQ+JyB9XG5cdFx0XHR9XG5cblx0XHRcdGxvZ2dlci52ZXJib3NlKGBUbyBjbGllbnQ6ICR7SlNPTi5zdHJpbmdpZnkob2JqZWN0VG9Mb2cpfWApO1xuXHRcdH1cblxuXHRcdHN1cGVyLnNlbmRFdmVudChldmVudCk7XG5cdH1cblxuXHQvKipcblx0ICogT3ZlcmxvYWQgc2VuZFJlcXVlc3QgdG8gbG9nXG5cdCAqL1xuXHRwdWJsaWMgc2VuZFJlcXVlc3QoY29tbWFuZDogc3RyaW5nLCBhcmdzOiBhbnksIHRpbWVvdXQ6IG51bWJlciwgY2I6IChyZXNwb25zZTogRGVidWdQcm90b2NvbC5SZXNwb25zZSkgPT4gdm9pZCk6IHZvaWQge1xuXHRcdGxvZ2dlci52ZXJib3NlKGBUbyBjbGllbnQ6ICR7SlNPTi5zdHJpbmdpZnkoY29tbWFuZCl9KCR7SlNPTi5zdHJpbmdpZnkoYXJncyl9KSwgdGltZW91dDogJHt0aW1lb3V0fWApO1xuXHRcdHN1cGVyLnNlbmRSZXF1ZXN0KGNvbW1hbmQsIGFyZ3MsIHRpbWVvdXQsIGNiKTtcblx0fVxuXG5cdC8qKlxuXHQgKiBPdmVybG9hZCBzZW5kUmVzcG9uc2UgdG8gbG9nXG5cdCAqL1xuXHRwdWJsaWMgc2VuZFJlc3BvbnNlKHJlc3BvbnNlOiBEZWJ1Z1Byb3RvY29sLlJlc3BvbnNlKTogdm9pZCB7XG5cdFx0bG9nZ2VyLnZlcmJvc2UoYFRvIGNsaWVudDogJHtKU09OLnN0cmluZ2lmeShyZXNwb25zZSl9YCk7XG5cdFx0c3VwZXIuc2VuZFJlc3BvbnNlKHJlc3BvbnNlKTtcblx0fVxuXG5cdHByb3RlY3RlZCBkaXNwYXRjaFJlcXVlc3QocmVxdWVzdDogRGVidWdQcm90b2NvbC5SZXF1ZXN0KTogdm9pZCB7XG5cdFx0bG9nZ2VyLnZlcmJvc2UoYEZyb20gY2xpZW50OiAke3JlcXVlc3QuY29tbWFuZH0oJHtKU09OLnN0cmluZ2lmeShyZXF1ZXN0LmFyZ3VtZW50cykgfSlgKTtcblx0XHRzdXBlci5kaXNwYXRjaFJlcXVlc3QocmVxdWVzdCk7XG5cdH1cbn1cbiJdfQ==