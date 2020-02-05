'use strict';

const asyncHook = require('../');
const assert = require('assert');

let called = false;

let initCalls = 0;
let destroyCalls = 0;

let initUid = NaN;
let initHandle = {};
let initParent = {};
let initProvider = NaN;

let preHandle = {};
let preUid = NaN;
let postDidThrow = NaN;

let postHandle = {};
let postUid = NaN;

let destroyUid = NaN;

asyncHook.addHooks({
  init: function (uid, handle, provider, parent) {
    initUid = uid;
    initHandle = handle;
    initParent = parent;
    initProvider = provider;

    initCalls += 1;
  },
  pre: function (uid, handle) {
    preUid = uid;
    preHandle = handle;
  },
  post: function (uid, handle, didThrow) {
    postUid = uid;
    postHandle = handle;
    postDidThrow = didThrow;
  },
  destroy: function (uid) {
    destroyUid = uid;
    destroyCalls += 1;
  }
});

asyncHook.enable();

process.nextTick(function (arg1, arg2) {
  called = true;
  assert.equal(arg1, 'a');
  assert.equal(arg2, 'b');
}, 'a', 'b');

asyncHook.disable();

process.once('exit', function () {
  assert.equal(initUid, preUid);
  assert.equal(initUid, postUid);
  assert.equal(initUid, destroyUid);

  assert.equal(initHandle, preHandle);
  assert.equal(initHandle, postHandle);

  assert.equal(initHandle.constructor.name, 'NextTickWrap');
  assert.equal(initParent, null);
  assert.equal(initProvider, 0);

  assert.equal(postDidThrow, false);

  assert.equal(called, true);
  assert.equal(initCalls, 1);
  assert.equal(destroyCalls, 1);
});
