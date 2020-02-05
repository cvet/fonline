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
// tslint:disable-next-line:no-require-imports no-var-requires
const tmp = require('tmp');
function createTemporaryFile(extension, temporaryDirectory) {
    return __awaiter(this, void 0, void 0, function* () {
        const options = { postfix: extension };
        if (temporaryDirectory) {
            options.dir = temporaryDirectory;
        }
        return new Promise((resolve, reject) => {
            // tslint:disable-next-line:no-any
            tmp.file(options, (err, tmpFile, _fd, cleanupCallback) => {
                if (err) {
                    return reject(err);
                }
                resolve({ filePath: tmpFile, cleanupCallback: cleanupCallback });
            });
        });
    });
}
exports.createTemporaryFile = createTemporaryFile;
function formatDate(date) {
    const lang = process.env.language;
    const dateOptions = { weekday: 'short', day: 'numeric', month: 'short', year: 'numeric', hour: 'numeric', minute: 'numeric' };
    return date.toLocaleString(lang, dateOptions);
}
exports.formatDate = formatDate;
function asyncFilter(arr, callback) {
    return __awaiter(this, void 0, void 0, function* () {
        return (yield Promise.all(arr.map((item) => __awaiter(this, void 0, void 0, function* () { return (yield callback(item)) ? item : undefined; })))).filter(i => i !== undefined);
    });
}
exports.asyncFilter = asyncFilter;
//# sourceMappingURL=helpers.js.map