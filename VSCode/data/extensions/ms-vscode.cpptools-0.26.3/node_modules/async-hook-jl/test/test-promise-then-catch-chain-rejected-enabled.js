'use strict';

const asyncHook = require('../');
const assert = require('assert');

let fulfilledCalled = false;
let rejectedCalled = false;
let rejectedArg = null;

const initUid = [];
const initHandle = [];
const initProvider = [];
const initParent = [];

const preUid = [];
const preHandle = [];

const postUid = [];
const postHandle = [];

const destroyUid = [];

asyncHook.addHooks({
  init: function (uid, handle, provider, parent) {
    initUid.push(uid);
    initHandle.push(handle);
    initProvider.push(provider);
    initParent.push(parent);
  },
  pre: function (uid, handle) {
    preUid.push(uid);
    preHandle.push(handle);
  },
  post: function (uid, handle) {
    postUid.push(uid);
    postHandle.push(handle);
  },
  destroy: function (uid) {
    destroyUid.push(uid);
  }
});

asyncHook.enable();

Promise
  .reject('a')
  .then(() => {
    fulfilledCalled = true;
  })
  .catch(arg => {
    rejectedCalled = true;
    rejectedArg = arg;
  });


asyncHook.disable();

process.once('exit', function () {
  assert.equal(initUid.length, 2, 'both handlers should init');
  assert.equal(preUid.length, 1, 'only the .catch should pre');
  assert.equal(postUid.length, 1, 'only the .catch should post');
  assert.equal(destroyUid.length, 2, 'both handlers should destroy');

  assert.equal(initUid[0], destroyUid[0]);

  assert.equal(initUid[1], preUid[0]);
  assert.equal(initUid[1], postUid[0]);
  assert.equal(initUid[1], destroyUid[1]);
  assert.equal(initHandle[1], preHandle[0]);
  assert.equal(initHandle[1], postHandle[0]);

  for (let i = 0; i < 2; ++i) {
    assert.equal(initHandle[i].constructor.name, 'PromiseWrap');
    assert.equal(initParent[i], null);
    assert.equal(initProvider[i], 0);
  }

  assert.equal(fulfilledCalled, false);
  assert.equal(rejectedCalled, true);
  assert.equal(rejectedArg, 'a');
});
