'use strict';

const asyncHook = require('../');
const assert = require('assert');
const fs = require('fs');

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

fs.access(__filename, function () {
  called = true;
});

process.once('exit', function () {
  assert.equal(called, true);
});
