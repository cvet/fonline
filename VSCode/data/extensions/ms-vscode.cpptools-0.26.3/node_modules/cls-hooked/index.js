'use strict';

const semver = require('semver');

/**
 * In order to increase node version support, this loads the version of context
 * that is appropriate for the version of on nodejs that is running.
 * Node < v8 - uses AsyncWrap and async-hooks-jl
 * Node >= v8 - uses native async-hooks
 */
if(process && semver.gte(process.versions.node, '8.0.0')){
  module.exports = require('./context');
}else{
  module.exports = require('./context-legacy');
}
