'use strict';

const asyncHook = require('../');
const assert = require('assert');

let called = false;

asyncHook.addHooks({
  init: function () {
    assert(false);
  },
  pre: function () {
    assert(false);
  },
  post: function () {
    assert(false);
  },
  destroy: function () {
    assert(false);
  }
});

asyncHook.enable();
asyncHook.disable();

process.nextTick(function (arg1, arg2) {
  called = true;
  assert.equal(arg1, 'a');
  assert.equal(arg2, 'b');
}, 'a', 'b');

process.once('exit', function () {
  assert.equal(called, true);
});
