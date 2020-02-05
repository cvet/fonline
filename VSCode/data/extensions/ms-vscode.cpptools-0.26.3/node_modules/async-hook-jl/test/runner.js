'use strict';

const fs = require('fs');
const path = require('path');
const async = require('async');
const spawn = require('child_process').spawn;
const clc = require('cli-color');

const allFailed = clc.red.bold;
const allPassed = clc.green;

const failed = clc.yellow;
const passed = (t) => t;

const files = fs.readdirSync(__dirname).filter(function (filename) {
  return filename.slice(0, 5) === 'test-';
});

async.mapSeries(files, runTest, function (err, passed) {
  if (err) throw err;

  let failed = 0;
  for (const ok of passed) {
    if (!ok) failed += 1;
  }

  if (failed > 0) {
    console.log(allFailed('failed') + ` - ${failed} tests failed`);
  } else {
    console.log(allPassed('passed') + ` - ${passed.length} tests passed`);
  }
});

function runTest(filename, done) {
  process.stdout.write(` - running ${filename} ...`);

  const p = spawn(process.execPath, [path.resolve(__dirname, filename)], {
    stdio: ['ignore', 1, 2],
    env: {
      'NODE_ASYNC_HOOK_NO_WARNING': '1'
    }
  });

  p.once('close', function (statusCode) {
    const ok = (statusCode === 0);

    if (ok) {
      console.log(' ' + passed('ok'));
    } else {
      console.log(' - ' + failed('failed'));
    }

    done(null, ok);
  });
}
