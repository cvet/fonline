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

const timerAId = setInterval(function () {
  timerACalled = true;
});
clearInterval(timerAId);

const timerBId = setInterval(function (arg1, arg2) {
  timerBCalled = true;
  assert.equal(arg1, 'a');
  assert.equal(arg2, 'b');
  clearInterval(timerBId);
}, 0, 'a', 'b');

process.once('exit', function () {
  assert.equal(timerACalled, false);
  assert.equal(timerBCalled, true);
});
