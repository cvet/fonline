'use strict';
const assert = require('assert');

require('../');

let e;
eval('(function() { e = new Error(); })()');

assert.equal(e.stack.split('\n').length, 10);
