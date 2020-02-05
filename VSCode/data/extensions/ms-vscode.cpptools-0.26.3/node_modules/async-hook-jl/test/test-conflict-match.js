'use strict';

const assert = require('assert');

const existing = global._asyncHook = {
  version: require('../package.json').version
};

const asyncHook = require('../');

assert.equal(asyncHook, existing);
