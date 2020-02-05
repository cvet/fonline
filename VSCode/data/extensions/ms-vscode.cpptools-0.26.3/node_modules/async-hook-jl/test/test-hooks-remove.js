'use strict';

const asyncHook = require('../');
const assert = require('assert');
const fs = require('fs');

let called = false;

let initCalls = 0;
let preCalls = 0;
let postCalls = 0;
let destroyCalls = 0;

const hooks = {
  init: function () {
    initCalls += 1;
  },
  pre: function () {
    preCalls += 1;
  },
  post: function () {
    postCalls += 1;
  },
  destroy: function () {
    destroyCalls += 1;
  }
};

asyncHook.addHooks(hooks);
asyncHook.addHooks(hooks);
asyncHook.removeHooks(hooks);

asyncHook.enable();

fs.access(__filename, function () {
  called = true;
});

asyncHook.disable();

process.once('exit', function () {
  assert.equal(called, true);

  assert.equal(initCalls, 1);
  assert.equal(preCalls, 1);
  assert.equal(postCalls, 1);
  assert.equal(destroyCalls, 1);
});
