'use strict';

const asyncHook = require('../');
const assert = require('assert');

const timings = [];

asyncHook.addHooks({
  init: function (uid, handle) {
    timings.push(`init#${uid} - ${handle.constructor.name}`);
  },
  pre: function (uid) {
    timings.push(`pre#${uid}`);
  },
  post: function (uid) {
    timings.push(`post#${uid}`);
  },
  destroy: function (uid) {
    timings.push(`destroy#${uid}`);
  }
});

asyncHook.enable();
const timerId = setImmediate(() => {
  timings.push('callback');
  clearImmediate(timerId);
}, 100);
asyncHook.disable();

process.once('exit', function () {
  assert.deepEqual(timings, [
    'init#-1 - ImmediateWrap',
    'pre#-1',
    'callback',
    'post#-1',
    'destroy#-1'
  ]);
});
