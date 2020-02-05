"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
class DeferredImpl {
    // tslint:disable-next-line:no-any
    constructor(scope = null) {
        this.scope = scope;
        this._resolved = false;
        this._rejected = false;
        // tslint:disable-next-line:promise-must-complete
        this._promise = new Promise((res, rej) => {
            this._resolve = res;
            this._reject = rej;
        });
    }
    resolve(_value) {
        // @ts-ignore
        this._resolve.apply(this.scope ? this.scope : this, arguments);
        this._resolved = true;
    }
    // tslint:disable-next-line:no-any
    reject(_reason) {
        // @ts-ignore
        this._reject.apply(this.scope ? this.scope : this, arguments);
        this._rejected = true;
    }
    get promise() {
        return this._promise;
    }
    get resolved() {
        return this._resolved;
    }
    get rejected() {
        return this._rejected;
    }
    get completed() {
        return this._rejected || this._resolved;
    }
}
// tslint:disable-next-line:no-any
function createDeferred(scope = null) {
    return new DeferredImpl(scope);
}
exports.createDeferred = createDeferred;
//# sourceMappingURL=promiseHelper.js.map