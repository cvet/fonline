"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
var __metadata = (this && this.__metadata) || function (k, v) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(k, v);
};
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const vscode_1 = require("vscode");
const logger_1 = require("../logger");
let OutputPanelLogger = class OutputPanelLogger {
    constructor() {
        this.outputChannel = logger_1.getLogChannel();
        const logLevel = vscode_1.workspace.getConfiguration('gitHistory').get('logLevel');
        this.traceEnabled = (typeof logLevel === 'string' && logLevel.toUpperCase() === 'DEBUG');
    }
    // tslint:disable-next-line:no-any
    log(...args) {
        const formattedText = this.formatArgs(...args);
        this.outputChannel.appendLine(formattedText);
    }
    // tslint:disable-next-line:no-any
    error(...args) {
        const formattedText = this.formatArgs(...args);
        this.outputChannel.appendLine(formattedText);
        this.outputChannel.show();
    }
    // tslint:disable-next-line:no-any
    trace(...args) {
        if (!this.traceEnabled) {
            return;
        }
        const formattedText = this.formatArgs(...args);
        this.outputChannel.appendLine(formattedText);
    }
    // tslint:disable-next-line:no-any
    formatArgs(...args) {
        return args.map(arg => {
            if (arg instanceof Error) {
                // tslint:disable-next-line:no-any
                const error = {};
                Object.getOwnPropertyNames(arg).forEach(key => {
                    error[key] = arg[key];
                });
                return JSON.stringify(error);
            }
            else if (arg !== null && arg !== undefined && typeof arg === 'object') {
                return JSON.stringify(arg);
            }
            else if (typeof arg === 'string' && arg.startsWith('--format=')) {
                return '--pretty=oneline';
            }
            else {
                return `${arg}`;
            }
        })
            .join(' ');
    }
};
OutputPanelLogger = __decorate([
    inversify_1.injectable(),
    __metadata("design:paramtypes", [])
], OutputPanelLogger);
exports.OutputPanelLogger = OutputPanelLogger;
//# sourceMappingURL=uiLogger.js.map