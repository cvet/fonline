'use strict';

const asyncHook = require('../');
const assert = require('assert');

asyncHook.enable();

let throws = false;
try {
  process.nextTick(undefined, 1);
} catch (e) {
  assert.equal(e.message, 'callback is not a function');
  assert.equal(e.name, 'TypeError');
  throws = true;
}

asyncHook.disable();

process.once('exit', function () {
  assert(throws);
});
