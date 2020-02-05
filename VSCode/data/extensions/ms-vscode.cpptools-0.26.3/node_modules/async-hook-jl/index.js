'use strict';

const AsyncHook = require('./async-hook.js');

// If a another copy (same version or not) of stack-chain exists it will result
// in wrong stack traces (most likely dublicate callSites).
if (global._asyncHook) {
  // In case the version match, we can simply return the first initialized copy
  if (global._asyncHook.version === require('./package.json').version) {
    module.exports = global._asyncHook;
  }
  // The version don't match, this is really bad. Lets just throw
  else {
    throw new Error('Conflicting version of async-hook-jl found');
  }
} else {
  const stackChain = require('stack-chain');

  // Remove callSites from this module. AsyncWrap doesn't have any callSites
  // and the hooks are expected to be completely transparent.
  stackChain.filter.attach(function (error, frames) {
    return frames.filter(function (callSite) {
      const filename = callSite.getFileName();
      // filename is not always a string, for example in case of eval it is
      // undefined. So check if the filename is defined.
      return !(filename && filename.slice(0, __dirname.length) === __dirname);
    });
  });

  module.exports = global._asyncHook = new AsyncHook();
}