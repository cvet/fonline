'use strict';

const asyncHook = require('../');
const assert = require('assert');

let timerCalled = false;

let initCalls = 0;
let destroyCalls = 0;

let initUid = NaN;
let initHandleName = '';
let initParent = {};
let initProvider = NaN;

let destroyUid = NaN;

asyncHook.addHooks({
  init: function (uid, handle, provider, parent) {
    initUid = uid;
    initHandleName = handle.constructor.name;
    initParent = parent;
    initProvider = provider;

    initCalls += 1;
  },
  pre: function () {
    assert(false);
  },
  post: function () {
    assert(false);
  },
  destroy: function (uid) {
    destroyUid = uid;
    destroyCalls += 1;
  }
});

asyncHook.enable();

const timerId = setImmediate(function () {
  timerCalled = true;
});

clearImmediate(timerId);

asyncHook.disable();

process.once('exit', function () {
  assert.equal(initUid, destroyUid);

  assert.equal(initHandleName, 'ImmediateWrap');
  assert.equal(initParent, null);
  assert.equal(initProvider, 0);

  assert.equal(timerCalled, false);
  assert.equal(initCalls, 1);
  assert.equal(destroyCalls, 1);
});
