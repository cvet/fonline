"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const fs = require("fs");
const path = require("path");
const mkdirp = require("mkdirp");
const debugSession_1 = require("./debugSession");
var LogLevel;
(function (LogLevel) {
    LogLevel[LogLevel["Verbose"] = 0] = "Verbose";
    LogLevel[LogLevel["Log"] = 1] = "Log";
    LogLevel[LogLevel["Warn"] = 2] = "Warn";
    LogLevel[LogLevel["Error"] = 3] = "Error";
    LogLevel[LogLevel["Stop"] = 4] = "Stop";
})(LogLevel = exports.LogLevel || (exports.LogLevel = {}));
class Logger {
    constructor() {
        this._pendingLogQ = [];
    }
    log(msg, level = LogLevel.Log) {
        msg = msg + '\n';
        this._write(msg, level);
    }
    verbose(msg) {
        this.log(msg, LogLevel.Verbose);
    }
    warn(msg) {
        this.log(msg, LogLevel.Warn);
    }
    error(msg) {
        this.log(msg, LogLevel.Error);
    }
    dispose() {
        if (this._currentLogger) {
            const disposeP = this._currentLogger.dispose();
            this._currentLogger = null;
            return disposeP;
        }
        else {
            return Promise.resolve();
        }
    }
    /**
     * `log` adds a newline, `write` doesn't
     */
    _write(msg, level = LogLevel.Log) {
        // [null, undefined] => string
        msg = msg + '';
        if (this._pendingLogQ) {
            this._pendingLogQ.push({ msg, level });
        }
        else if (this._currentLogger) {
            this._currentLogger.log(msg, level);
        }
    }
    /**
     * Set the logger's minimum level to log in the console, and whether to log to the file. Log messages are queued before this is
     * called the first time, because minLogLevel defaults to Warn.
     */
    setup(consoleMinLogLevel, _logFilePath, prependTimestamp = true) {
        const logFilePath = typeof _logFilePath === 'string' ?
            _logFilePath :
            (_logFilePath && this._logFilePathFromInit);
        if (this._currentLogger) {
            const options = {
                consoleMinLogLevel,
                logFilePath,
                prependTimestamp
            };
            this._currentLogger.setup(options).then(() => {
                // Now that we have a minimum logLevel, we can clear out the queue of pending messages
                if (this._pendingLogQ) {
                    const logQ = this._pendingLogQ;
                    this._pendingLogQ = null;
                    logQ.forEach(item => this._write(item.msg, item.level));
                }
            });
        }
    }
    init(logCallback, logFilePath, logToConsole) {
        // Re-init, create new global Logger
        this._pendingLogQ = this._pendingLogQ || [];
        this._currentLogger = new InternalLogger(logCallback, logToConsole);
        this._logFilePathFromInit = logFilePath;
    }
}
exports.Logger = Logger;
exports.logger = new Logger();
/**
 * Manages logging, whether to console.log, file, or VS Code console.
 * Encapsulates the state specific to each logging session
 */
class InternalLogger {
    constructor(logCallback, isServer) {
        /** Dispose and allow exit to continue normally */
        this.beforeExitCallback = () => this.dispose();
        this._logCallback = logCallback;
        this._logToConsole = isServer;
        this._minLogLevel = LogLevel.Warn;
        this.disposeCallback = (signal, code) => {
            this.dispose();
            // Exit with 128 + value of the signal code.
            // https://nodejs.org/api/process.html#process_exit_codes
            code = code || 2; // SIGINT
            code += 128;
            process.exit(code);
        };
    }
    setup(options) {
        return __awaiter(this, void 0, void 0, function* () {
            this._minLogLevel = options.consoleMinLogLevel;
            this._prependTimestamp = options.prependTimestamp;
            // Open a log file in the specified location. Overwritten on each run.
            if (options.logFilePath) {
                if (!path.isAbsolute(options.logFilePath)) {
                    this.log(`logFilePath must be an absolute path: ${options.logFilePath}`, LogLevel.Error);
                }
                else {
                    const handleError = err => this.sendLog(`Error creating log file at path: ${options.logFilePath}. Error: ${err.toString()}\n`, LogLevel.Error);
                    try {
                        yield mkdirpPromise(path.dirname(options.logFilePath));
                        this.log(`Verbose logs are written to:\n`, LogLevel.Warn);
                        this.log(options.logFilePath + '\n', LogLevel.Warn);
                        this._logFileStream = fs.createWriteStream(options.logFilePath);
                        this.logDateTime();
                        this.setupShutdownListeners();
                        this._logFileStream.on('error', err => {
                            handleError(err);
                        });
                    }
                    catch (err) {
                        handleError(err);
                    }
                }
            }
        });
    }
    logDateTime() {
        let d = new Date();
        let dateString = d.getUTCFullYear() + '-' + `${d.getUTCMonth() + 1}` + '-' + d.getUTCDate();
        const timeAndDateStamp = dateString + ', ' + getFormattedTimeString();
        this.log(timeAndDateStamp + '\n', LogLevel.Verbose, false);
    }
    setupShutdownListeners() {
        process.addListener('beforeExit', this.beforeExitCallback);
        process.addListener('SIGTERM', this.disposeCallback);
        process.addListener('SIGINT', this.disposeCallback);
    }
    removeShutdownListeners() {
        process.removeListener('beforeExit', this.beforeExitCallback);
        process.removeListener('SIGTERM', this.disposeCallback);
        process.removeListener('SIGINT', this.disposeCallback);
    }
    dispose() {
        return new Promise(resolve => {
            this.removeShutdownListeners();
            if (this._logFileStream) {
                this._logFileStream.end(resolve);
                this._logFileStream = null;
            }
            else {
                resolve();
            }
        });
    }
    log(msg, level, prependTimestamp = true) {
        if (this._minLogLevel === LogLevel.Stop) {
            return;
        }
        if (level >= this._minLogLevel) {
            this.sendLog(msg, level);
        }
        if (this._logToConsole) {
            const logFn = level === LogLevel.Error ? console.error :
                level === LogLevel.Warn ? console.warn :
                    null;
            if (logFn) {
                logFn(trimLastNewline(msg));
            }
        }
        // If an error, prepend with '[Error]'
        if (level === LogLevel.Error) {
            msg = `[${LogLevel[level]}] ${msg}`;
        }
        if (this._prependTimestamp && prependTimestamp) {
            msg = '[' + getFormattedTimeString() + '] ' + msg;
        }
        if (this._logFileStream) {
            this._logFileStream.write(msg);
        }
    }
    sendLog(msg, level) {
        // Truncate long messages, they can hang VS Code
        if (msg.length > 1500) {
            const endsInNewline = !!msg.match(/(\n|\r\n)$/);
            msg = msg.substr(0, 1500) + '[...]';
            if (endsInNewline) {
                msg = msg + '\n';
            }
        }
        if (this._logCallback) {
            const event = new LogOutputEvent(msg, level);
            this._logCallback(event);
        }
    }
}
function mkdirpPromise(folder) {
    return new Promise((resolve, reject) => {
        mkdirp(folder, err => {
            if (err) {
                reject(err);
            }
            else {
                resolve();
            }
        });
    });
}
class LogOutputEvent extends debugSession_1.OutputEvent {
    constructor(msg, level) {
        const category = level === LogLevel.Error ? 'stderr' :
            level === LogLevel.Warn ? 'console' :
                'stdout';
        super(msg, category);
    }
}
exports.LogOutputEvent = LogOutputEvent;
function trimLastNewline(str) {
    return str.replace(/(\n|\r\n)$/, '');
}
exports.trimLastNewline = trimLastNewline;
function getFormattedTimeString() {
    let d = new Date();
    let hourString = _padZeroes(2, String(d.getUTCHours()));
    let minuteString = _padZeroes(2, String(d.getUTCMinutes()));
    let secondString = _padZeroes(2, String(d.getUTCSeconds()));
    let millisecondString = _padZeroes(3, String(d.getUTCMilliseconds()));
    return hourString + ':' + minuteString + ':' + secondString + '.' + millisecondString + ' UTC';
}
function _padZeroes(minDesiredLength, numberToPad) {
    if (numberToPad.length >= minDesiredLength) {
        return numberToPad;
    }
    else {
        return String('0'.repeat(minDesiredLength) + numberToPad).slice(-minDesiredLength);
    }
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoibG9nZ2VyLmpzIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsiLi4vc3JjL2xvZ2dlci50cyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiO0FBQUE7OzREQUU0RDs7Ozs7Ozs7OztBQUU1RCx5QkFBeUI7QUFDekIsNkJBQTZCO0FBQzdCLGlDQUFpQztBQUNqQyxpREFBMkM7QUFFM0MsSUFBWSxRQU1YO0FBTkQsV0FBWSxRQUFRO0lBQ25CLDZDQUFXLENBQUE7SUFDWCxxQ0FBTyxDQUFBO0lBQ1AsdUNBQVEsQ0FBQTtJQUNSLHlDQUFTLENBQUE7SUFDVCx1Q0FBUSxDQUFBO0FBQ1QsQ0FBQyxFQU5XLFFBQVEsR0FBUixnQkFBUSxLQUFSLGdCQUFRLFFBTW5CO0FBZ0JELE1BQWEsTUFBTTtJQUFuQjtRQUlTLGlCQUFZLEdBQWUsRUFBRSxDQUFDO0lBMkV2QyxDQUFDO0lBekVBLEdBQUcsQ0FBQyxHQUFXLEVBQUUsS0FBSyxHQUFHLFFBQVEsQ0FBQyxHQUFHO1FBQ3BDLEdBQUcsR0FBRyxHQUFHLEdBQUcsSUFBSSxDQUFDO1FBQ2pCLElBQUksQ0FBQyxNQUFNLENBQUMsR0FBRyxFQUFFLEtBQUssQ0FBQyxDQUFDO0lBQ3pCLENBQUM7SUFFRCxPQUFPLENBQUMsR0FBVztRQUNsQixJQUFJLENBQUMsR0FBRyxDQUFDLEdBQUcsRUFBRSxRQUFRLENBQUMsT0FBTyxDQUFDLENBQUM7SUFDakMsQ0FBQztJQUVELElBQUksQ0FBQyxHQUFXO1FBQ2YsSUFBSSxDQUFDLEdBQUcsQ0FBQyxHQUFHLEVBQUUsUUFBUSxDQUFDLElBQUksQ0FBQyxDQUFDO0lBQzlCLENBQUM7SUFFRCxLQUFLLENBQUMsR0FBVztRQUNoQixJQUFJLENBQUMsR0FBRyxDQUFDLEdBQUcsRUFBRSxRQUFRLENBQUMsS0FBSyxDQUFDLENBQUM7SUFDL0IsQ0FBQztJQUVELE9BQU87UUFDTixJQUFJLElBQUksQ0FBQyxjQUFjLEVBQUU7WUFDeEIsTUFBTSxRQUFRLEdBQUcsSUFBSSxDQUFDLGNBQWMsQ0FB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QUcsR0FBRyxHQUFHLFlBQVksR0FBRyxHQUFHLEdBQUcsaUJBQWlCLEdBQUcsTUFBTSxDQUFDO0FBQ2hHLENBQUM7QUFFRCxTQUFTLFVBQVUsQ0FBQyxnQkFBd0IsRUFBRSxXQUFtQjtJQUNoRSxJQUFJLFdBQVcsQ0FBQyxNQUFNLElBQUksZ0JBQWdCLEVBQUU7UUFDM0MsT0FBTyxXQUFXLENBQUM7S0FDbkI7U0FBTTtRQUNOLE9BQU8sTUFBTSxDQUFDLEdBQUcsQ0FBQyxNQUFNLENBQUMsZ0JBQWdCLENBQUMsR0FBRyxXQUFXLENBQUMsQ0FBQyxLQUFLLENBQUMsQ0FBQyxnQkFBZ0IsQ0FBQyxDQUFDO0tBQ25GO0FBQ0YsQ0FBQyIsInNvdXJjZXNDb250ZW50IjpbIi8qLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tXG4gKiBDb3B5cmlnaHQgKEMpIE1pY3Jvc29mdCBDb3Jwb3JhdGlvbi4gQWxsIHJpZ2h0cyByZXNlcnZlZC5cbiAqLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0qL1xuXG5pbXBvcnQgKiBhcyBmcyBmcm9tICdmcyc7XG5pbXBvcnQgKiBhcyBwYXRoIGZyb20gJ3BhdGgnO1xuaW1wb3J0ICogYXMgbWtkaXJwIGZyb20gJ21rZGlycCc7XG5pbXBvcnQge091dHB1dEV2ZW50fSBmcm9tICcuL2RlYnVnU2Vzc2lvbic7XG5cbmV4cG9ydCBlbnVtIExvZ0xldmVsIHtcblx0VmVyYm9zZSA9IDAsXG5cdExvZyA9IDEsXG5cdFdhcm4gPSAyLFxuXHRFcnJvciA9IDMsXG5cdFN0b3AgPSA0XG59XG5cbmV4cG9ydCB0eXBlIElMb2dDYWxsYmFjayA9IChvdXRwdXRFdmVudDogT3V0cHV0RXZlbnQpID0+IHZvaWQ7XG5cbmludGVyZmFjZSBJTG9nSXRlbSB7XG5cdG1zZzogc3RyaW5nO1xuXHRsZXZlbDogTG9nTGV2ZWw7XG59XG5cbmV4cG9ydCBpbnRlcmZhY2UgSUxvZ2dlciB7XG5cdGxvZyhtc2c6IHN0cmluZywgbGV2ZWw/OiBMb2dMZXZlbCk6IHZvaWQ7XG5cdHZlcmJvc2UobXNnOiBzdHJpbmcpOiB2b2lkO1xuXHR3YXJuKG1zZzogc3RyaW5nKTogdm9pZDtcblx0ZXJyb3IobXNnOiBzdHJpbmcpOiB2b2lkO1xufVxuXG5leHBvcnQgY2xhc3MgTG9nZ2VyIHtcblx0cHJpdmF0ZSBfbG9nRmlsZVBhdGhGcm9tSW5pdDogc3RyaW5nO1xuXG5cdHByaXZhdGUgX2N1cnJlbnRMb2dnZXI6IEludGVybmFsTG9nZ2VyO1xuXHRwcml2YXRlIF9wZW5kaW5nTG9nUTogSUxvZ0l0ZW1bXSA9IFtdO1xuXG5cdGxvZyhtc2c6IHN0cmluZywgbGV2ZWwgPSBMb2dMZXZlbC5Mb2cpOiB2b2lkIHtcblx0XHRtc2cgPSBtc2cgKyAnXFxuJztcblx0XHR0aGlzLl93cml0ZShtc2csIGxldmVsKTtcblx0fVxuXG5cdHZlcmJvc2UobXNnOiBzdHJpbmcpOiB2b2lkIHtcblx0XHR0aGlzLmxvZyhtc2csIExvZ0xldmVsLlZlcmJvc2UpO1xuXHR9XG5cblx0d2Fybihtc2c6IHN0cmluZyk6IHZvaWQge1xuXHRcdHRoaXMubG9nKG1zZywgTG9nTGV2ZWwuV2Fybik7XG5cdH1cblxuXHRlcnJvcihtc2c6IHN0cmluZyk6IHZvaWQge1xuXHRcdHRoaXMubG9nKG1zZywgTG9nTGV2ZWwuRXJyb3IpO1xuXHR9XG5cblx0ZGlzcG9zZSgpOiBQcm9taXNlPHZvaWQ+IHtcblx0XHRpZiAodGhpcy5fY3VycmVudExvZ2dlcikge1xuXHRcdFx0Y29uc3QgZGlzcG9zZVAgPSB0aGlzLl9jdXJyZW50TG9nZ2VyLmRpc3Bvc2UoKTtcblx0XHRcdHRoaXMuX2N1cnJlbnRMb2dnZXIgPSBudWxsO1xuXHRcdFx0cmV0dXJuIGRpc3Bvc2VQO1xuXHRcdH0gZWxzZSB7XG5cdFx0XHRyZXR1cm4gUHJvbWlzZS5yZXNvbHZlKCk7XG5cdFx0fVxuXHR9XG5cblx0LyoqXG5cdCAqIGBsb2dgIGFkZHMgYSBuZXdsaW5lLCBgd3JpdGVgIGRvZXNuJ3Rcblx0ICovXG5cdHByaXZhdGUgX3dyaXRlKG1zZzogc3RyaW5nLCBsZXZlbCA9IExvZ0xldmVsLkxvZyk6IHZvaWQge1xuXHRcdC8vIFtudWxsLCB1bmRlZmluZWRdID0+IHN0cmluZ1xuXHRcdG1zZyA9IG1zZyArICcnO1xuXHRcdGlmICh0aGlzLl9wZW5kaW5nTG9nUSkge1xuXHRcdFx0dGhpcy5fcGVuZGluZ0xvZ1EucHVzaCh7IG1zZywgbGV2ZWwgfSk7XG5cdFx0fSBlbHNlIGlmICh0aGlzLl9jdXJyZW50TG9nZ2VyKSB7XG5cdFx0XHR0aGlzLl9jdXJyZW50TG9nZ2VyLmxvZyhtc2csIGxldmVsKTtcblx0XHR9XG5cdH1cblxuXHQvKipcblx0ICogU2V0IHRoZSBsb2dnZXIncyBtaW5pbXVtIGxldmVsIHRvIGxvZyBpbiB0aGUgY29uc29sZSwgYW5kIHdoZXRoZXIgdG8gbG9nIHRvIHRoZSBmaWxlLiBMb2cgbWVzc2FnZXMgYXJlIHF1ZXVlZCBiZWZvcmUgdGhpcyBpc1xuXHQgKiBjYWxsZWQgdGhlIGZpcnN0IHRpbWUsIGJlY2F1c2UgbWluTG9nTGV2ZWwgZGVmYXVsdHMgdG8gV2Fybi5cblx0ICovXG5cdHNldHVwKGNvbnNvbGVNaW5Mb2dMZXZlbDogTG9nTGV2ZWwsIF9sb2dGaWxlUGF0aD86IHN0cmluZ3xib29sZWFuLCBwcmVwZW5kVGltZXN0YW1wOiBib29sZWFuID0gdHJ1ZSk6IHZvaWQge1xuXHRcdGNvbnN0IGxvZ0ZpbGVQYXRoID0gdHlwZW9mIF9sb2dGaWxlUGF0aCA9PT0gJ3N0cmluZycgP1xuXHRcdFx0X2xvZ0ZpbGVQYXRoIDpcblx0XHRcdChfbG9nRmlsZVBhdGggJiYgdGhpcy5fbG9nRmlsZVBhdGhGcm9tSW5pdCk7XG5cblx0XHRpZiAodGhpcy5fY3VycmVudExvZ2dlcikge1xuXHRcdFx0Y29uc3Qgb3B0aW9ucyA9IHtcblx0XHRcdFx0Y29uc29sZU1pbkxvZ0xldmVsLFxuXHRcdFx0XHRsb2dGaWxlUGF0aCxcblx0XHRcdFx0cHJlcGVuZFRpbWVzdGFtcFxuXHRcdFx0fTtcblx0XHRcdHRoaXMuX2N1cnJlbnRMb2dnZXIuc2V0dXAob3B0aW9ucykudGhlbigoKSA9PiB7XG5cdFx0XHRcdC8vIE5vdyB0aGF0IHdlIGhhdmUgYSBtaW5pbXVtIGxvZ0xldmVsLCB3ZSBjYW4gY2xlYXIgb3V0IHRoZSBxdWV1ZSBvZiBwZW5kaW5nIG1lc3NhZ2VzXG5cdFx0XHRcdGlmICh0aGlzLl9wZW5kaW5nTG9nUSkge1xuXHRcdFx0XHRcdGNvbnN0IGxvZ1EgPSB0aGlzLl9wZW5kaW5nTG9nUTtcblx0XHRcdFx0XHR0aGlzLl9wZW5kaW5nTG9nUSA9IG51bGw7XG5cdFx0XHRcdFx0bG9nUS5mb3JFYWNoKGl0ZW0gPT4gdGhpcy5fd3JpdGUoaXRlbS5tc2csIGl0ZW0ubGV2ZWwpKTtcblx0XHRcdFx0fVxuXHRcdFx0fSk7XG5cblx0XHR9XG5cdH1cblxuXHRpbml0KGxvZ0NhbGxiYWNrOiBJTG9nQ2FsbGJhY2ssIGxvZ0ZpbGVQYXRoPzogc3RyaW5nLCBsb2dUb0NvbnNvbGU/OiBib29sZWFuKTogdm9pZCB7XG5cdFx0Ly8gUmUtaW5pdCwgY3JlYXRlIG5ldyBnbG9iYWwgTG9nZ2VyXG5cdFx0dGhpcy5fcGVuZGluZ0xvZ1EgPSB0aGlzLl9wZW5kaW5nTG9nUSB8fCBbXTtcblx0XHR0aGlzLl9jdXJyZW50TG9nZ2VyID0gbmV3IEludGVybmFsTG9nZ2VyKGxvZ0NhbGxiYWNrLCBsb2dUb0NvbnNvbGUpO1xuXHRcdHRoaXMuX2xvZ0ZpbGVQYXRoRnJvbUluaXQgPSBsb2dGaWxlUGF0aDtcblx0fVxufVxuXG5leHBvcnQgY29uc3QgbG9nZ2VyID0gbmV3IExvZ2dlcigpO1xuXG5pbnRlcmZhY2UgSUludGVybmFsTG9nZ2VyT3B0aW9ucyB7XG5cdGNvbnNvbGVNaW5Mb2dMZXZlbDogTG9nTGV2ZWw7XG5cdGxvZ0ZpbGVQYXRoPzogc3RyaW5nO1xuXHRwcmVwZW5kVGltZXN0YW1wPzogYm9vbGVhbjtcbn1cblxuLyoqXG4gKiBNYW5hZ2VzIGxvZ2dpbmcsIHdoZXRoZXIgdG8gY29uc29sZS5sb2csIGZpbGUsIG9yIFZTIENvZGUgY29uc29sZS5cbiAqIEVuY2Fwc3VsYXRlcyB0aGUgc3RhdGUgc3BlY2lmaWMgdG8gZWFjaCBsb2dnaW5nIHNlc3Npb25cbiAqL1xuY2xhc3MgSW50ZXJuYWxMb2dnZXIge1xuXHRwcml2YXRlIF9taW5Mb2dMZXZlbDogTG9nTGV2ZWw7XG5cdHByaXZhdGUgX2xvZ1RvQ29uc29sZTogYm9vbGVhbjtcblxuXHQvKiogTG9nIGluZm8gdGhhdCBtZWV0cyBtaW5Mb2dMZXZlbCBpcyBzZW50IHRvIHRoaXMgY2FsbGJhY2suICovXG5cdHByaXZhdGUgX2xvZ0NhbGxiYWNrOiBJTG9nQ2FsbGJhY2s7XG5cblx0LyoqIFdyaXRlIHN0ZWFtIGZvciBsb2cgZmlsZSAqL1xuXHRwcml2YXRlIF9sb2dGaWxlU3RyZWFtOiBmcy5Xcml0ZVN0cmVhbTtcblxuXHQvKiogRGlzcG9zZSBhbmQgYWxsb3cgZXhpdCB0byBjb250aW51ZSBub3JtYWxseSAqL1xuXHRwcml2YXRlIGJlZm9yZUV4aXRDYWxsYmFjayA9ICgpID0+IHRoaXMuZGlzcG9zZSgpO1xuXG5cdC8qKiBEaXNwb3NlIGFuZCBleGl0ICovXG5cdHByaXZhdGUgZGlzcG9zZUNhbGxiYWNrO1xuXG5cdC8qKiBXaGV0aGVyIHRvIGFkZCBhIHRpbWVzdGFtcCB0byBtZXNzYWdlcyBpbiB0aGUgbG9nZmlsZSAqL1xuXHRwcml2YXRlIF9wcmVwZW5kVGltZXN0YW1wOiBib29sZWFuO1xuXG5cdGNvbnN0cnVjdG9yKGxvZ0NhbGxiYWNrOiBJTG9nQ2FsbGJhY2ssIGlzU2VydmVyPzogYm9vbGVhbikge1xuXHRcdHRoaXMuX2xvZ0NhbGxiYWNrID0gbG9nQ2FsbGJhY2s7XG5cdFx0dGhpcy5fbG9nVG9Db25zb2xlID0gaXNTZXJ2ZXI7XG5cblx0XHR0aGlzLl9taW5Mb2dMZXZlbCA9IExvZ0xldmVsLldhcm47XG5cblx0XHR0aGlzLmRpc3Bvc2VDYWxsYmFjayA9IChzaWduYWw6IHN0cmluZywgY29kZTogbnVtYmVyKSA9PiB7XG5cdFx0XHR0aGlzLmRpc3Bvc2UoKTtcblxuXHRcdFx0Ly8gRXhpdCB3aXRoIDEyOCArIHZhbHVlIG9mIHRoZSBzaWduYWwgY29kZS5cblx0XHRcdC8vIGh0dHBzOi8vbm9kZWpzLm9yZy9hcGkvcHJvY2Vzcy5odG1sI3Byb2Nlc3NfZXhpdF9jb2Rlc1xuXHRcdFx0Y29kZSA9IGNvZGUgfHwgMjsgLy8gU0lHSU5UXG5cdFx0XHRjb2RlICs9IDEyODtcblxuXHRcdFx0cHJvY2Vzcy5leGl0KGNvZGUpO1xuXHRcdH07XG5cdH1cblxuXHRwdWJsaWMgYXN5bmMgc2V0dXAob3B0aW9uczogSUludGVybmFsTG9nZ2VyT3B0aW9ucyk6IFByb21pc2U8dm9pZD4ge1xuXHRcdHRoaXMuX21pbkxvZ0xldmVsID0gb3B0aW9ucy5jb25zb2xlTWluTG9nTGV2ZWw7XG5cdFx0dGhpcy5fcHJlcGVuZFRpbWVzdGFtcCA9IG9wdGlvbnMucHJlcGVuZFRpbWVzdGFtcDtcblxuXHRcdC8vIE9wZW4gYSBsb2cgZmlsZSBpbiB0aGUgc3BlY2lmaWVkIGxvY2F0aW9uLiBPdmVyd3JpdHRlbiBvbiBlYWNoIHJ1bi5cblx0XHRpZiAob3B0aW9ucy5sb2dGaWxlUGF0aCkge1xuXHRcdFx0aWYgKCFwYXRoLmlzQWJzb2x1dGUob3B0aW9ucy5sb2dGaWxlUGF0aCkpIHtcblx0XHRcdFx0dGhpcy5sb2coYGxvZ0ZpbGVQYXRoIG11c3QgYmUgYW4gYWJzb2x1dGUgcGF0aDogJHtvcHRpb25zLmxvZ0ZpbGVQYXRofWAsIExvZ0xldmVsLkVycm9yKTtcblx0XHRcdH0gZWxzZSB7XG5cdFx0XHRcdGNvbnN0IGhhbmRsZUVycm9yID0gZXJyID0+IHRoaXMuc2VuZExvZyhgRXJyb3IgY3JlYXRpbmcgbG9nIGZpbGUgYXQgcGF0aDogJHtvcHRpb25zLmxvZ0ZpbGVQYXRofS4gRXJyb3I6ICR7ZXJyLnRvU3RyaW5nKCl9XFxuYCwgTG9nTGV2ZWwuRXJyb3IpO1xuXG5cdFx0XHRcdHRyeSB7XG5cdFx0XHRcdFx0YXdhaXQgbWtkaXJwUHJvbWlzZShwYXRoLmRpcm5hbWUob3B0aW9ucy5sb2dGaWxlUGF0aCkpO1xuXHRcdFx0XHRcdHRoaXMubG9nKGBWZXJib3NlIGxvZ3MgYXJlIHdyaXR0ZW4gdG86XFxuYCwgTG9nTGV2ZWwuV2Fybik7XG5cdFx0XHRcdFx0dGhpcy5sb2cob3B0aW9ucy5sb2dGaWxlUGF0aCArICdcXG4nLCBMb2dMZXZlbC5XYXJuKTtcblxuXHRcdFx0XHRcdHRoaXMuX2xvZ0ZpbGVTdHJlYW0gPSBmcy5jcmVhdGVXcml0ZVN0cmVhbShvcHRpb25zLmxvZ0ZpbGVQYXRoKTtcblx0XHRcdFx0XHR0aGlzLmxvZ0RhdGVUaW1lKCk7XG5cdFx0XHRcdFx0dGhpcy5zZXR1cFNodXRkb3duTGlzdGVuZXJzKCk7XG5cdFx0XHRcdFx0dGhpcy5fbG9nRmlsZVN0cmVhbS5vbignZXJyb3InLCBlcnIgPT4ge1xuXHRcdFx0XHRcdFx0aGFuZGxlRXJyb3IoZXJyKTtcblx0XHRcdFx0XHR9KTtcblx0XHRcdFx0fSBjYXRjaCAoZXJyKSB7XG5cdFx0XHRcdFx0aGFuZGxlRXJyb3IoZXJyKTtcblx0XHRcdFx0fVxuXHRcdFx0fVxuXHRcdH1cblx0fVxuXG5cdHByaXZhdGUgbG9nRGF0ZVRpbWUoKTogdm9pZCB7XG5cdFx0bGV0IGQgPSBuZXcgRGF0ZSgpO1xuXHRcdGxldCBkYXRlU3RyaW5nID0gZC5nZXRVVENGdWxsWWVhcigpICsgJy0nICsgYCR7ZC5nZXRVVENNb250aCgpICsgMX1gICsgJy0nICsgZC5nZXRVVENEYXRlKCk7XG5cdFx0Y29uc3QgdGltZUFuZERhdGVTdGFtcCA9IGRhdGVTdHJpbmcgKyAnLCAnICsgZ2V0Rm9ybWF0dGVkVGltZVN0cmluZygpO1xuXHRcdHRoaXMubG9nKHRpbWVBbmREYXRlU3RhbXAgKyAnXFxuJywgTG9nTGV2ZWwuVmVyYm9zZSwgZmFsc2UpO1xuXHR9XG5cblx0cHJpdmF0ZSBzZXR1cFNodXRkb3duTGlzdGVuZXJzKCk6IHZvaWQge1xuXHRcdHByb2Nlc3MuYWRkTGlzdGVuZXIoJ2JlZm9yZUV4aXQnLCB0aGlzLmJlZm9yZUV4aXRDYWxsYmFjayk7XG5cdFx0cHJvY2Vzcy5hZGRMaXN0ZW5lcignU0lHVEVSTScsIHRoaXMuZGlzcG9zZUNhbGxiYWNrKTtcblx0XHRwcm9jZXNzLmFkZExpc3RlbmVyKCdTSUdJTlQnLCB0aGlzLmRpc3Bvc2VDYWxsYmFjayk7XG5cdH1cblxuXHRwcml2YXRlIHJlbW92ZVNodXRkb3duTGlzdGVuZXJzKCk6IHZvaWQge1xuXHRcdHByb2Nlc3MucmVtb3ZlTGlzdGVuZXIoJ2JlZm9yZUV4aXQnLCB0aGlzLmJlZm9yZUV4aXRDYWxsYmFjayk7XG5cdFx0cHJvY2Vzcy5yZW1vdmVMaXN0ZW5lcignU0lHVEVSTScsIHRoaXMuZGlzcG9zZUNhbGxiYWNrKTtcblx0XHRwcm9jZXNzLnJlbW92ZUxpc3RlbmVyKCdTSUdJTlQnLCB0aGlzLmRpc3Bvc2VDYWxsYmFjayk7XG5cdH1cblxuXHRwdWJsaWMgZGlzcG9zZSgpOiBQcm9taXNlPHZvaWQ+IHtcblx0XHRyZXR1cm4gbmV3IFByb21pc2UocmVzb2x2ZSA9PiB7XG5cdFx0XHR0aGlzLnJlbW92ZVNodXRkb3duTGlzdGVuZXJzKCk7XG5cdFx0XHRpZiAodGhpcy5fbG9nRmlsZVN0cmVhbSkge1xuXHRcdFx0XHR0aGlzLl9sb2dGaWxlU3RyZWFtLmVuZChyZXNvbHZlKTtcblx0XHRcdFx0dGhpcy5fbG9nRmlsZVN0cmVhbSA9IG51bGw7XG5cdFx0XHR9IGVsc2Uge1xuXHRcdFx0XHRyZXNvbHZlKCk7XG5cdFx0XHR9XG5cdFx0fSk7XG5cdH1cblxuXHRwdWJsaWMgbG9nKG1zZzogc3RyaW5nLCBsZXZlbDogTG9nTGV2ZWwsIHByZXBlbmRUaW1lc3RhbXAgPSB0cnVlKTogdm9pZCB7XG5cdFx0aWYgKHRoaXMuX21pbkxvZ0xldmVsID09PSBMb2dMZXZlbC5TdG9wKSB7XG5cdFx0XHRyZXR1cm47XG5cdFx0fVxuXG5cdFx0aWYgKGxldmVsID49IHRoaXMuX21pbkxvZ0xldmVsKSB7XG5cdFx0XHR0aGlzLnNlbmRMb2cobXNnLCBsZXZlbCk7XG5cdFx0fVxuXG5cdFx0aWYgKHRoaXMuX2xvZ1RvQ29uc29sZSkge1xuXHRcdFx0Y29uc3QgbG9nRm4gPVxuXHRcdFx0XHRsZXZlbCA9PT0gTG9nTGV2ZWwuRXJyb3IgPyBjb25zb2xlLmVycm9yIDpcblx0XHRcdFx0bGV2ZWwgPT09IExvZ0xldmVsLldhcm4gPyBjb25zb2xlLndhcm4gOlxuXHRcdFx0XHRudWxsO1xuXG5cdFx0XHRpZiAobG9nRm4pIHtcblx0XHRcdFx0bG9nRm4odHJpbUxhc3ROZXdsaW5lKG1zZykpO1xuXHRcdFx0fVxuXHRcdH1cblxuXHRcdC8vIElmIGFuIGVycm9yLCBwcmVwZW5kIHdpdGggJ1tFcnJvcl0nXG5cdFx0aWYgKGxldmVsID09PSBMb2dMZXZlbC5FcnJvcikge1xuXHRcdFx0bXNnID0gYFske0xvZ0xldmVsW2xldmVsXX1dICR7bXNnfWA7XG5cdFx0fVxuXG5cdFx0aWYgKHRoaXMuX3ByZXBlbmRUaW1lc3RhbXAgJiYgcHJlcGVuZFRpbWVzdGFtcCkge1xuXHRcdFx0bXNnID0gJ1snICsgZ2V0Rm9ybWF0dGVkVGltZVN0cmluZygpICsgJ10gJyArIG1zZztcblx0XHR9XG5cblx0XHRpZiAodGhpcy5fbG9nRmlsZVN0cmVhbSkge1xuXHRcdFx0dGhpcy5fbG9nRmlsZVN0cmVhbS53cml0ZShtc2cpO1xuXHRcdH1cblx0fVxuXG5cdHByaXZhdGUgc2VuZExvZyhtc2c6IHN0cmluZywgbGV2ZWw6IExvZ0xldmVsKTogdm9pZCB7XG5cdFx0Ly8gVHJ1bmNhdGUgbG9uZyBtZXNzYWdlcywgdGhleSBjYW4gaGFuZyBWUyBDb2RlXG5cdFx0aWYgKG1zZy5sZW5ndGggPiAxNTAwKSB7XG5cdFx0XHRjb25zdCBlbmRzSW5OZXdsaW5lID0gISFtc2cubWF0Y2goLyhcXG58XFxyXFxuKSQvKTtcblx0XHRcdG1zZyA9IG1zZy5zdWJzdHIoMCwgMTUwMCkgKyAnWy4uLl0nO1xuXHRcdFx0aWYgKGVuZHNJbk5ld2xpbmUpIHtcblx0XHRcdFx0bXNnID0gbXNnICsgJ1xcbic7XG5cdFx0XHR9XG5cdFx0fVxuXG5cdFx0aWYgKHRoaXMuX2xvZ0NhbGxiYWNrKSB7XG5cdFx0XHRjb25zdCBldmVudCA9IG5ldyBMb2dPdXRwdXRFdmVudChtc2csIGxldmVsKTtcblx0XHRcdHRoaXMuX2xvZ0NhbGxiYWNrKGV2ZW50KTtcblx0XHR9XG5cdH1cbn1cblxuZnVuY3Rpb24gbWtkaXJwUHJvbWlzZShmb2xkZXI6IHN0cmluZyk6IFByb21pc2U8dm9pZD4ge1xuXHRyZXR1cm4gbmV3IFByb21pc2UoKHJlc29sdmUsIHJlamVjdCkgPT4ge1xuXHRcdG1rZGlycChmb2xkZXIsIGVyciA9PiB7XG5cdFx0XHRpZiAoZXJyKSB7XG5cdFx0XHRcdHJlamVjdChlcnIpO1xuXHRcdFx0fSBlbHNlIHtcblx0XHRcdFx0cmVzb2x2ZSgpO1xuXHRcdFx0fVxuXHRcdH0pO1xuXHR9KTtcbn1cblxuZXhwb3J0IGNsYXNzIExvZ091dHB1dEV2ZW50IGV4dGVuZHMgT3V0cHV0RXZlbnQge1xuXHRjb25zdHJ1Y3Rvcihtc2c6IHN0cmluZywgbGV2ZWw6IExvZ0xldmVsKSB7XG5cdFx0Y29uc3QgY2F0ZWdvcnkgPVxuXHRcdFx0bGV2ZWwgPT09IExvZ0xldmVsLkVycm9yID8gJ3N0ZGVycicgOlxuXHRcdFx0bGV2ZWwgPT09IExvZ0xldmVsLldhcm4gPyAnY29uc29sZScgOlxuXHRcdFx0J3N0ZG91dCc7XG5cdFx0c3VwZXIobXNnLCBjYXRlZ29yeSk7XG5cdH1cbn1cblxuZXhwb3J0IGZ1bmN0aW9uIHRyaW1MYXN0TmV3bGluZShzdHI6IHN0cmluZyk6IHN0cmluZyB7XG5cdHJldHVybiBzdHIucmVwbGFjZSgvKFxcbnxcXHJcXG4pJC8sICcnKTtcbn1cblxuZnVuY3Rpb24gZ2V0Rm9ybWF0dGVkVGltZVN0cmluZygpOiBzdHJpbmcge1xuXHRsZXQgZCA9IG5ldyBEYXRlKCk7XG5cdGxldCBob3VyU3RyaW5nID0gX3BhZFplcm9lcygyLCBTdHJpbmcoZC5nZXRVVENIb3VycygpKSk7XG5cdGxldCBtaW51dGVTdHJpbmcgPSBfcGFkWmVyb2VzKDIsIFN0cmluZyhkLmdldFVUQ01pbnV0ZXMoKSkpO1xuXHRsZXQgc2Vjb25kU3RyaW5nID0gX3BhZFplcm9lcygyLCBTdHJpbmcoZC5nZXRVVENTZWNvbmRzKCkpKTtcblx0bGV0IG1pbGxpc2Vjb25kU3RyaW5nID0gX3BhZFplcm9lcygzLCBTdHJpbmcoZC5nZXRVVENNaWxsaXNlY29uZHMoKSkpO1xuXHRyZXR1cm4gaG91clN0cmluZyArICc6JyArIG1pbnV0ZVN0cmluZyArICc6JyArIHNlY29uZFN0cmluZyArICcuJyArIG1pbGxpc2Vjb25kU3RyaW5nICsgJyBVVEMnO1xufVxuXG5mdW5jdGlvbiBfcGFkWmVyb2VzKG1pbkRlc2lyZWRMZW5ndGg6IG51bWJlciwgbnVtYmVyVG9QYWQ6IHN0cmluZyk6IHN0cmluZyB7XG5cdGlmIChudW1iZXJUb1BhZC5sZW5ndGggPj0gbWluRGVzaXJlZExlbmd0aCkge1xuXHRcdHJldHVybiBudW1iZXJUb1BhZDtcblx0fSBlbHNlIHtcblx0XHRyZXR1cm4gU3RyaW5nKCcwJy5yZXBlYXQobWluRGVzaXJlZExlbmd0aCkgKyBudW1iZXJUb1BhZCkuc2xpY2UoLW1pbkRlc2lyZWRMZW5ndGgpO1xuXHR9XG59XG4iXX0=