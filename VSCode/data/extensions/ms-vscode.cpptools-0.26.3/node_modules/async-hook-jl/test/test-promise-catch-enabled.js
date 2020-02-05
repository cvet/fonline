'use strict';

const asyncHook = require('../');
const assert = require('assert');

let rejctedCalled = false;
let rejectedArg = null;

let initCalls = 0;
let destroyCalls = 0;

let initUid = NaN;
let initHandle = {};
let initParent = {};
let initProvider = NaN;

let preHandle = {};
let preUid = NaN;

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
  post: function (uid, handle) {
    postUid = uid;
    postHandle = handle;
  },
  destroy: function (uid) {
    destroyUid = uid;
    destroyCalls += 1;
  }
});

asyncHook.enable();

Promise
  .reject('a')
  .catch(arg => {
    rejctedCalled = true;
    rejectedArg = arg;
  });

asyncHook.disable();

process.once('exit', function () {
  assert.equal(initUid, preUid);
  assert.equal(initUid, postUid);
  assert.equal(initUid, destroyUid);

  assert.equal(initHandle, preHandle);
  assert.equal(initHandle, postHandle);

  assert.equal(initHandle.constructor.name, 'PromiseWrap');
  assert.equal(initParent, null);
  assert.equal(initProvider, 0);

  assert.equal(rejctedCalled, true);
  assert.equal(rejectedArg, 'a');
  assert.equal(initCalls, 1);
  assert.equal(destroyCalls, 1);
});
