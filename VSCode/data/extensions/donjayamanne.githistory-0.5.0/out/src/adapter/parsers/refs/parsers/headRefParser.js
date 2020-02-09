"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const types_1 = require("../../../../types");
const constants_1 = require("./../constants");
let HeadRefParser = class HeadRefParser {
    canParse(refContent) {
        return typeof refContent === 'string' && constants_1.HEAD_REF_PREFIXES.filter(prefix => refContent.startsWith(prefix)).length > 0;
    }
    parse(refContent) {
        const prefix = constants_1.HEAD_REF_PREFIXES.filter(item => refContent.startsWith(item))[0];
        // tslint:disable-next-line:no-unnecessary-local-variable
        const ref = {
            name: refContent.substring(prefix.length),
            type: types_1.RefType.Head
        };
        return ref;
    }
};
HeadRefParser = __decorate([
    inversify_1.injectable()
], HeadRefParser);
exports.HeadRefParser = HeadRefParser;
//# sourceMappingURL=headRefParser.js.map