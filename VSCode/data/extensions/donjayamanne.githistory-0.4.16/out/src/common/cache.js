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
// tslint:disable:no-any
const MAX_CACHE_ITEMS = 50;
const CacheItemUsageFrequency = new Map();
const CacheStores = new Map();
// tslint:disable-next-line:no-stateless-class
class CacheRegister {
    static get(storageKey, key) {
        const storage = CacheStores.get(storageKey);
        if (storage && storage.has(key)) {
            const entry = storage.get(key);
            if (!entry.expiryTime || entry.expiryTime < new Date().getTime()) {
                return { data: entry.data };
            }
            storage.delete(key);
        }
        return;
    }
    // tslint:disable-next-line:no-any
    static add(storageKey, key, data, expiryMs) {
        if (!CacheStores.has(storageKey)) {
            CacheStores.set(storageKey, new Map());
        }
        const storage = CacheStores.get(storageKey);
        const counter = CacheItemUsageFrequency.has(key) ? CacheItemUsageFrequency.get(key) : 0;
        // tslint:disable-next-line:no-increment-decrement
        CacheItemUsageFrequency.set(key, counter + 1);
        const expiryTime = typeof expiryMs === 'number' ? new Date().getTime() + expiryMs : undefined;
        storage.set(key, { data, expiryTime });
        setTimeout(() => CacheRegister.reclaimSpace(), 1000);
    }
    static reclaimSpace() {
        CacheStores.forEach(storage => {
            if (storage.size <= MAX_CACHE_ITEMS) {
                return;
            }
            const keyWithCounters = [];
            for (const key of storage.keys()) {
                const counter = CacheItemUsageFrequency.get(key);
                keyWithCounters.push({ key, counter });
            }
            keyWithCounters.sort((a, b) => a.counter - b.counter);
            while (storage.size > MAX_CACHE_ITEMS) {
                const key = keyWithCounters.shift().key;
                storage.delete(key);
            }
        });
    }
    dispose() {
        CacheStores.clear();
    }
}
exports.CacheRegister = CacheRegister;
function cache(storageKey, arg1, arg2) {
    // tslint:disable-next-line:no-any function-name no-function-expression
    return function (_target, propertyKey, descriptor) {
        const oldFn = descriptor.value;
        // tslint:disable-next-line:no-any
        descriptor.value = function (...args) {
            return __awaiter(this, void 0, void 0, function* () {
                const expiryMs = typeof arg1 === 'number' ? arg1 : (typeof arg2 === 'number' ? arg2 : -1);
                const cacheKeyPrefix = typeof arg1 === 'string' ? arg1 : (typeof arg2 === 'string' ? arg2 : '');
                // tslint:disable-next-line:no-invalid-this no-parameter-reassignment
                storageKey = typeof this.getHashCode === 'function' ? `${storageKey}${this.getHashCode()}` : storageKey;
                const key = `${storageKey}.${storageKey}.${cacheKeyPrefix}.${propertyKey}.${JSON.stringify(args)}`;
                const entry = CacheRegister.get(storageKey, key);
                if (entry) {
                    return entry.data;
                }
                // tslint:disable-next-line:no-invalid-this
                const result = oldFn.apply(this, args);
                if (result && result.then && result.catch) {
                    // tslint:disable-next-line:no-any
                    result.then((value) => {
                        // We could add the promise itself into the cache store.
                        // But lets leave this simple for now.
                        CacheRegister.add(storageKey, key, value, expiryMs);
                    });
                }
                return result;
            });
        };
        return descriptor;
    };
}
exports.cache = cache;
//# sourceMappingURL=cache.js.map