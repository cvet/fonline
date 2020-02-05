'use strict';

const asyncWrap = process.binding('async_wrap');
const TIMERWRAP = asyncWrap.Providers.TIMERWRAP;

const patchs = {
  'nextTick': require('./patches/next-tick.js'),
  'promise': require('./patches/promise.js'),
  'timers': require('./patches/timers.js')
};

const ignoreUIDs = new Set();

function State() {
  this.enabled = false;
  this.counter = 0;
}

function Hooks() {
  const initFns = this.initFns = [];
  const preFns = this.preFns = [];
  const postFns = this.postFns = [];
  const destroyFns = this.destroyFns = [];

  this.init = function (uid, provider, parentUid, parentHandle) {
    // Ignore TIMERWRAP, since setTimeout etc. is monkey patched
    if (provider === TIMERWRAP) {
      ignoreUIDs.add(uid);
      return;
    }

    // call hooks
    for (const hook of initFns) {
      hook(uid, this, provider, parentUid, parentHandle);
    }
  };

  this.pre = function (uid) {
    if (ignoreUIDs.has(uid)) return;

    // call hooks
    for (const hook of preFns) {
      hook(uid, this);
    }
  };

  this.post = function (uid, didThrow) {
    if (ignoreUIDs.has(uid)) return;

    // call hooks
    for (const hook of postFns) {
      hook(uid, this, didThrow);
    }
  };

  this.destroy = function (uid) {
    // Cleanup the ignore list if this uid should be ignored
    if (ignoreUIDs.has(uid)) {
      ignoreUIDs.delete(uid);
      return;
    }

    // call hooks
    for (const hook of destroyFns) {
      hook(uid);
    }
  };
}

Hooks.prototype.add = function (hooks) {
  if (hooks.init) this.initFns.push(hooks.init);
  if (hooks.pre) this.preFns.push(hooks.pre);
  if (hooks.post) this.postFns.push(hooks.post);
  if (hooks.destroy) this.destroyFns.push(hooks.destroy);
};

function removeElement(array, item) {
  const index = array.indexOf(item);
  if (index === -1) return;
  array.splice(index, 1);
}

Hooks.prototype.remove = function (hooks) {
  if (hooks.init) removeElement(this.initFns, hooks.init);
  if (hooks.pre) removeElement(this.preFns, hooks.pre);
  if (hooks.post) removeElement(this.postFns, hooks.post);
  if (hooks.destroy) removeElement(this.destroyFns, hooks.destroy);
};

function AsyncHook() {
  this._state = new State();
  this._hooks = new Hooks();

  // expose version for conflict detection
  this.version = require('./package.json').version;

  // expose the Providers map
  this.providers = asyncWrap.Providers;

  // apply patches
  for (const key of Object.keys(patchs)) {
    patchs[key].call(this);
  }

  // setup async wrap
  if (process.env.hasOwnProperty('NODE_ASYNC_HOOK_WARNING')) {
    console.warn('warning: you are using async-hook-jl which is unstable.');
  }
  asyncWrap.setupHooks({
    init: this._hooks.init,
    pre: this._hooks.pre,
    post: this._hooks.post,
    destroy: this._hooks.destroy
  });
}
module.exports = AsyncHook;

AsyncHook.prototype.addHooks = function (hooks) {
  this._hooks.add(hooks);
};

AsyncHook.prototype.removeHooks = function (hooks) {
  this._hooks.remove(hooks);
};

AsyncHook.prototype.enable = function () {
  this._state.enabled = true;
  asyncWrap.enable();
};

AsyncHook.prototype.disable = function () {
  this._state.enabled = false;
  asyncWrap.disable();
};