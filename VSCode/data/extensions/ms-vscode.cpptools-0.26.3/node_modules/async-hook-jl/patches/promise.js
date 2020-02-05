'use strict';

function PromiseWrap() {}

module.exports = function patchPromise() {
  const hooks = this._hooks;
  const state = this._state;

  const Promise = global.Promise;

  /* As per ECMAScript 2015, .catch must be implemented by calling .then, as
   * such we need needn't patch .catch as well. see:
   * http://www.ecma-international.org/ecma-262/6.0/#sec-promise.prototype.catch
   */
  const oldThen = Promise.prototype.then;
  Promise.prototype.then = wrappedThen;

  function makeWrappedHandler(fn, handle, uid, isOnFulfilled) {
    if ('function' !== typeof fn) {
      return isOnFulfilled
        ? makeUnhandledResolutionHandler(uid)
        : makeUnhandledRejectionHandler(uid);
    }

    return function wrappedHandler() {
      hooks.pre.call(handle, uid);
      try {
        return fn.apply(this, arguments);
      } finally {
        hooks.post.call(handle, uid, false);
        hooks.destroy.call(null, uid);
      }
    };
  }

  function makeUnhandledResolutionHandler(uid) {
    return function unhandledResolutionHandler(val) {
      hooks.destroy.call(null, uid);
      return val;
    };
  }

  function makeUnhandledRejectionHandler(uid) {
    return function unhandledRejectedHandler(val) {
      hooks.destroy.call(null, uid);
      throw val;
    };
  }

  function wrappedThen(onFulfilled, onRejected) {
    if (!state.enabled) return oldThen.call(this, onFulfilled, onRejected);

    const handle = new PromiseWrap();
    const uid = --state.counter;

    hooks.init.call(handle, uid, 0, null, null);

    return oldThen.call(
      this,
      makeWrappedHandler(onFulfilled, handle, uid, true),
      makeWrappedHandler(onRejected, handle, uid, false)
    );
  }
};
