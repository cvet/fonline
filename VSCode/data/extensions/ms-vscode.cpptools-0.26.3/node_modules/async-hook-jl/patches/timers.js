'use strict';

const timers = require('timers');

function TimeoutWrap() {}
function IntervalWrap() {}
function ImmediateWrap() {}

const timeoutMap = new Map();
const intervalMap = new Map();
const ImmediateMap = new Map();

let activeCallback = null;
let clearedInCallback = false;

module.exports = function patch() {
  patchTimer(this._hooks, this._state, 'setTimeout', 'clearTimeout', TimeoutWrap, timeoutMap, true);
  patchTimer(this._hooks, this._state, 'setInterval', 'clearInterval', IntervalWrap, intervalMap, false);
  patchTimer(this._hooks, this._state, 'setImmediate', 'clearImmediate', ImmediateWrap, ImmediateMap, true);

  global.setTimeout = timers.setTimeout;
  global.setInterval = timers.setInterval;
  global.setImmediate = timers.setImmediate;

  global.clearTimeout = timers.clearTimeout;
  global.clearInterval = timers.clearInterval;
  global.clearImmediate = timers.clearImmediate;
};

function patchTimer(hooks, state, setFn, clearFn, Handle, timerMap, singleCall) {
  const oldSetFn = timers[setFn];
  const oldClearFn = timers[clearFn];

  // overwrite set[Timeout]
  timers[setFn] = function () {
    if (!state.enabled) return oldSetFn.apply(timers, arguments);

    const args = new Array(arguments.length);
    for (let i = 0; i < arguments.length; i++) {
      args[i] = arguments[i];
    }
    const callback = args[0];

    if (typeof callback !== 'function') {
      throw new TypeError('"callback" argument must be a function');
    }

    const handle = new Handle();
    const uid = --state.counter;
    let timerId = undefined;

    // call the init hook
    hooks.init.call(handle, uid, 0, null, null);

    // overwrite callback
    args[0] = function () {
      // call the pre hook
      activeCallback = timerId;
      hooks.pre.call(handle, uid);

      let didThrow = true;
      try {
        callback.apply(this, arguments);
        didThrow = false;
      } finally {
        // If `callback` threw and there is an uncaughtException handler
        // then call the `post` and `destroy` hook after the uncaughtException
        // user handlers have been invoked.
        if (didThrow && process.listenerCount('uncaughtException') > 0) {
          process.once('uncaughtException', function () {
            // call the post hook
            hooks.post.call(handle, uid, true);
            // setInterval won't continue
            timerMap.delete(timerId);
            hooks.destroy.call(null, uid);
          });
        }
      }

      // callback done successfully
      hooks.post.call(handle, uid, false);
      activeCallback = null;

      // call the destroy hook if the callback will only be called once
      if (singleCall || clearedInCallback) {
        clearedInCallback = false;
        timerMap.delete(timerId);
        hooks.destroy.call(null, uid);
      }
    };

    timerId = oldSetFn.apply(timers, args);
    // Bind the timerId and uid for later use, in case the clear* function is
    // called.
    timerMap.set(timerId, uid);

    return timerId;
  };

  // overwrite clear[Timeout]
  timers[clearFn] = function (timerId) {
    // If clear* was called within the timer callback, then delay the destroy
    // event to after the post event has been called.
    if (activeCallback === timerId && timerId !== null) {
      clearedInCallback = true;
    }
    // clear should call the destroy hook. Note if timerId doesn't exists
    // it is because asyncWrap wasn't enabled at the time.
    else if (timerMap.has(timerId)) {
      const uid = timerMap.get(timerId);
      timerMap.delete(timerId);
      hooks.destroy.call(null, uid);
    }

    oldClearFn.apply(timers, arguments);
  };
}
