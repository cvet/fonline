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
var __param = (this && this.__param) || function (paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
};
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const types_1 = require("../../../common/types");
const types_2 = require("../../../types");
let FileStatStatusParser = class FileStatStatusParser {
    constructor(loggers) {
        this.loggers = loggers;
    }
    canParse(status) {
        const parsedStatus = this.parse(status);
        return parsedStatus !== undefined && parsedStatus !== null;
    }
    parse(status) {
        status = status || '';
        status = status.length === 0 ? '' : status.trim().substring(0, 1);
        switch (status) {
            case 'A':
                return types_2.Status.Added;
            case 'M':
                return types_2.Status.Modified;
            case 'D':
                return types_2.Status.Deleted;
            case 'C':
                return types_2.Status.Copied;
            case 'R':
                return types_2.Status.Renamed;
            case 'T':
                return types_2.Status.TypeChanged;
            case 'X':
                return types_2.Status.Unknown;
            case 'U':
                return types_2.Status.Unmerged;
            case 'B':
                return types_2.Status.Broken;
            default: {
                this.loggers.forEach(logger => logger.error(`Unrecognized file stat status '${status}`));
                return;
            }
        }
    }
};
FileStatStatusParser = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.multiInject(types_1.ILogService)),
    __metadata("design:paramtypes", [Array])
], FileStatStatusParser);
exports.FileStatStatusParser = FileStatStatusParser;
//# sourceMappingURL=parser.js.map