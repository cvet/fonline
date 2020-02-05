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
class SmartStepper {
    constructor(_enabled) {
        this._enabled = _enabled;
    }
    shouldSmartStep(stackFrame, pathTransformer, sourceMapTransformer) {
        return __awaiter(this, void 0, void 0, function* () {
            if (!this._enabled)
                return false;
            const clientPath = pathTransformer.getClientPathFromTargetPath(stackFrame.source.path) || stackFrame.source.path;
            const mapping = yield sourceMapTransformer.mapToAuthored(clientPath, stackFrame.line, stackFrame.column);
            if (mapping) {
                return false;
            }
            if ((yield sourceMapTransformer.allSources(clientPath)).length) {
                return true;
            }
            return false;
        });
    }
}
exports.SmartStepper = SmartStepper;

//# sourceMappingURL=smartStep.js.map
