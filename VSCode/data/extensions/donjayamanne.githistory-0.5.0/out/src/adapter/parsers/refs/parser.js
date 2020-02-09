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
function __export(m) {
    for (var p in m) if (!exports.hasOwnProperty(p)) exports[p] = m[p];
}
Object.defineProperty(exports, "__esModule", { value: true });
__export(require("./types"));
const inversify_1 = require("inversify");
const types_1 = require("../../../common/types");
const types_2 = require("./types");
let RefsParser = class RefsParser {
    constructor(parsers, loggers) {
        this.parsers = parsers;
        this.loggers = loggers;
    }
    /**
     * Parses refs returned by the following two commands
     * git branch --all (only considers)
     * git show-refs
     * git log --format=%D
     * @param {string} refContent the ref content as string to be parsed
     * @returns {Ref[]} A reference which can either be a branch, tag or origin
     */
    parse(refContent) {
        return (refContent || '').split(',')
            .map(ref => ref.trim())
            .filter(line => line.length > 0)
            .map(ref => {
            const parser = this.parsers.find(item => item.canParse(ref));
            if (!parser) {
                this.loggers.forEach(logger => logger.error(`No parser found for ref '${ref}'`));
                return;
            }
            return parser.parse(ref);
        })
            .filter(ref => ref !== undefined && ref !== null)
            .map(ref => ref);
    }
};
RefsParser = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.multiInject(types_2.IRefParser)),
    __param(1, inversify_1.multiInject(types_1.ILogService)),
    __metadata("design:paramtypes", [Array, Array])
], RefsParser);
exports.RefsParser = RefsParser;
//# sourceMappingURL=parser.js.map