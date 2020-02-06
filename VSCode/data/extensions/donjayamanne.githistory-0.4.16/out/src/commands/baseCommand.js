"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
class BaseCommand {
    constructor(data) {
        this.data = data;
        // tslint:disable-next-line:no-any no-banned-terms
        this._arguments = [];
        this._command = '';
        this._title = '';
        this._description = '';
    }
    // tslint:disable-next-line:no-banned-terms
    get arguments() {
        return this._arguments;
    }
    get command() {
        return this._command;
    }
    get title() {
        return this._title;
    }
    get label() {
        return this._title;
    }
    get description() {
        return this._description;
    }
    get detail() {
        return this._detail;
    }
    get tooltip() {
        return this._tooltip;
    }
    preExecute() {
        return __awaiter(this, void 0, void 0, function* () {
            return true;
        });
    }
    setTitle(value) {
        this._title = value;
    }
    setCommand(value) {
        this._command = value;
    }
    // tslint:disable-next-line:no-any
    setCommandArguments(args) {
        this._arguments = args;
    }
    setDescription(value) {
        this._description = value;
    }
    setDetail(value) {
        this._detail = value;
    }
    setTooltip(value) {
        this._tooltip = value;
    }
}
exports.BaseCommand = BaseCommand;
//# sourceMappingURL=baseCommand.js.map