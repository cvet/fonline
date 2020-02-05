'use strict';

const asyncHook = require('../');
const assert = require('assert');
const net = require('net');

let called = false;
let firstHook = true;

let initCalls = 0;

let serverHandle = {};
let serverUid = NaN;

let clientParentHandle = {};
let clientParentUid = NaN;


asyncHook.addHooks({
  init: function (uid, handle, provider, parentUid, parentHandle) {
    if (provider != asyncHook.providers.TCPWRAP) return;

    if (firstHook) {
      firstHook = false;

      serverUid = uid;
      serverHandle = handle;

      assert.equal(parentUid, null);
      assert.equal(parentHandle, null);
    } else {
      clientParentHandle = parentHandle;
      clientParentUid = parentUid;
    }

    initCalls += 1;
  }
});

asyncHook.enable();

const server = net.createServer(function (socket) {
  socket.end();
  server.close();
  called = true;
}).listen(function () {
  net.connect(this.address());
});

asyncHook.disable();

process.once('exit', function () {
  assert.equal(serverHandle, clientParentHandle);
  assert.equal(serverUid, clientParentUid);

  assert.equal(called, true);
  assert.equal(initCalls, 2);
});
