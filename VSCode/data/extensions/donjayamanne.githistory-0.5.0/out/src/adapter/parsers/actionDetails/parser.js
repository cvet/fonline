"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
let ActionDetailsParser = class ActionDetailsParser {
    parse(name, email, unixTime) {
        name = (name || '').trim();
        unixTime = (unixTime || '').trim();
        if (unixTime.length === 0) {
            return;
        }
        const time = parseInt(unixTime, 10);
        const date = new Date(time * 1000);
        return {
            date, email, name
        };
    }
};
ActionDetailsParser = __decorate([
    inversify_1.injectable()
], ActionDetailsParser);
exports.ActionDetailsParser = ActionDetailsParser;
//# sourceMappingURL=parser.js.map