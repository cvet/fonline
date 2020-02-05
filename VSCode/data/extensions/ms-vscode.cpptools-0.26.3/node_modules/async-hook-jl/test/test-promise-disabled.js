'use strict';

const asyncHook = require('../');
const assert = require('assert');

const thenHandlersCalledA = [false, false];
const thenHandlersCalledB = [false, false];
let catchHandlerCalledC = false;
let catchHandlerCalledD = false;

let fulfilledArgA = null;
let rejectedArgB = null;
let rejectedArgC = null;

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

Promise
  .resolve('a')
  .then(arg => {
    fulfilledArgA = arg;
    thenHandlersCalledA[0] = true;
  }, () => {
    thenHandlersCalledA[1] = true;
  });

Promise
  .reject('b')
  .then(() => {
    thenHandlersCalledB[0] = true;
  }, arg => {
    rejectedArgB = arg;
    thenHandlersCalledB[1] = true;
  });

Promise
  .reject('c')
  .catch(arg => {
    rejectedArgC = arg;
    catchHandlerCalledC = true;
  });

Promise
  .resolve('d')
  .catch(() => {
    catchHandlerCalledD = true;
  });

process.once('exit', function () {
  assert.deepStrictEqual(thenHandlersCalledA, [true, false]);
  assert.equal(fulfilledArgA, 'a');
  assert.deepStrictEqual(thenHandlersCalledB, [false, true]);
  assert.equal(rejectedArgB, 'b');
  assert.equal(catchHandlerCalledC, true);
  assert.equal(rejectedArgC, 'c');
  assert.equal(catchHandlerCalledD, false);
});
