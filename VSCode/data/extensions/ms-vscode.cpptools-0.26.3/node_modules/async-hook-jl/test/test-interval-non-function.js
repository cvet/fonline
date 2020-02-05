'use strict';

const asyncHook = require('../');
const assert = require('assert');

asyncHook.enable();

let throws = false;
try {
  setInterval(undefined, 1);
} catch (e) {
  assert.equal(e.message, '"callback" argument must be a function');
  assert.equal(e.name, 'TypeError');
  throws = true;
}

asyncHook.disable();

process.once('exit', function () {
  assert(throws);
});
