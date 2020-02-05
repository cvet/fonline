'use strict';

const asyncHook = require('../');
const assert = require('assert');

let timerACalled = false;
let timerBCalled = false;

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

const timerId = setImmediate(function () {
  timerACalled = true;
});

clearImmediate(timerId);

setImmediate(function (arg1, arg2) {
  timerBCalled = true;
  assert.equal(arg1, 'a');
  assert.equal(arg2, 'b');
}, 'a', 'b');

process.once('exit', function () {
  assert.equal(timerACalled, false);
  assert.equal(timerBCalled, true);
});
