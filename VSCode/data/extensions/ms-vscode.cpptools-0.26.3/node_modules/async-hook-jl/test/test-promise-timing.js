'use strict';

const asyncHook = require('../');
const assert = require('assert');

const eventOrder = [];

asyncHook.addHooks({
  init: function (uid, handle) {
    eventOrder.push(`init#${uid} ${handle.constructor.name}`);
  },
  pre: function (uid) {
    eventOrder.push(`pre#${uid}`);
  },
  post: function (uid) {
    eventOrder.push(`post#${uid}`);
  },
  destroy: function (uid) {
    eventOrder.push(`destroy#${uid}`);
  }
});

asyncHook.enable();

new Promise(function (s) {
  setTimeout(s, 100); // 1
})
.then(function () {
  return new Promise((s) => setTimeout(s, 100)); // 4
})
.then();

process.once('exit', function () {
  const nodeMajor = parseInt(process.versions.node.split('.')[0], 10);

  if (nodeMajor >= 6) {
    assert.deepEqual(eventOrder, [
      'init#-1 TimeoutWrap',
      'init#-2 PromiseWrap',
      'init#-3 PromiseWrap',
      'pre#-1',
      'post#-1',
      'destroy#-1',
      'pre#-2',
      'init#-4 TimeoutWrap',
      'post#-2',
      'destroy#-2',
      'init#-5 PromiseWrap',
      'pre#-4',
      'post#-4',
      'destroy#-4',
      'pre#-5',
      'post#-5',
      'destroy#-5',
      'destroy#-3'
    ]);
  } else {
    assert.deepEqual(eventOrder, [
      'init#-1 TimeoutWrap',
      'init#-2 PromiseWrap',
      'init#-3 PromiseWrap',
      'pre#-1',
      'post#-1',
      'destroy#-1',
      'pre#-2',
      'init#-4 TimeoutWrap',
      'post#-2',
      'destroy#-2',
      'pre#-4',
      'post#-4',
      'destroy#-4',
      'destroy#-3'
    ]);
  }
});
