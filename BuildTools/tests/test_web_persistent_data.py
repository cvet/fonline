from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess

import pytest


BOOTSTRAP = Path(__file__).resolve().parents[1] / "web/persistent-data.js"


@pytest.mark.parametrize("browser", [False, True])
@pytest.mark.parametrize("hydrate_error", [False, True])
@pytest.mark.parametrize("provided_arguments,explicit_root", [
    (False, None),
    (True, None),
    (True, ""),
    (True, "/custom/browser-data"),
])
@pytest.mark.parametrize("root_option", ["-Common.UserWritablePath", " \t--Common.UserWritablePath \r\n"])
def test_web_storage_is_ready_before_main_without_overriding_explicit_roots(
    tmp_path: Path,
    browser: bool,
    explicit_root: str | None,
    hydrate_error: bool,
    provided_arguments: bool,
    root_option: str,
) -> None:
    node = shutil.which("node")
    assert node, "Node.js is required for the Web bootstrap lifecycle regression"
    fixture = tmp_path / "storage.cjs"
    fixture.write_text(r"""
const assert = require('assert/strict');
const fs = require('fs');
const vm = require('vm');
const options = JSON.parse(process.argv[3]);
const args = ['--Client.Language', 'engl'];
if (options.explicit_root !== null) {
    args.push(options.root_option, options.explicit_root);
}
const originalArgs = [...args];
const dependencies = new Set();
const mounts = [];
const errors = [];
let completion;
let previousCalls = 0;
const Module = {arguments: args, preRun: () => ++previousCalls};
if (!options.provided_arguments) {
    delete Module.arguments;
}
const idbfs = {};
const context = {
    Module,
    IDBFS: idbfs,
    console: {error: (...values) => errors.push(values)},
    addRunDependency: name => {
        assert(!dependencies.has(name));
        dependencies.add(name);
    },
    removeRunDependency: name => assert(dependencies.delete(name)),
    FS: {
        mkdirTree: path => assert.equal(path, '/PersistentData'),
        mount: (backend, settings, path) => {
            assert.equal(backend, idbfs);
            assert.equal(path, '/PersistentData');
            assert.equal(settings.autoPersist, true);
            mounts.push(path);
        },
        syncfs: (populate, callback) => {
            assert.equal(populate, true);
            assert.equal(dependencies.size, 1, 'main must be blocked before asynchronous hydration starts');
            completion = callback;
        }
    }
};
if (options.browser) {
    context.window = {};
}
vm.runInNewContext(fs.readFileSync(process.argv[2], 'utf8'), context);
const capturedArgs = Module.arguments || [];
assert.equal(Module.preRun.length, 2);
for (const callback of Module.preRun) {
    callback();
}
assert.equal(previousCalls, 1, 'a shell-provided preRun function must survive');
if (!options.browser) {
    assert.equal(mounts.length, 0);
    if (options.provided_arguments) {
        assert.deepEqual(Module.arguments, originalArgs);
    } else {
        assert.equal(Module.arguments, undefined);
    }
    assert.equal(dependencies.size, 0);
    process.exit(0);
}
assert.equal(mounts.length, 1);
assert.equal(Module.syncfsDone, 0);
assert.equal(dependencies.size, 1, 'settings must not load until browser files are available');
const expectedRoot = options.explicit_root === null ? '/PersistentData' : options.explicit_root;
assert.equal(Module.arguments.at(-1), expectedRoot);
assert.equal(capturedArgs.at(-1), expectedRoot, 'Emscripten must capture the same argument array before preRun');
if (options.provided_arguments) {
    assert.deepEqual(Module.arguments.slice(0, 2), originalArgs.slice(0, 2));
}
Module.preRun[1]();
assert.equal(mounts.length, 1, 'repeated initialization must not remount or reschedule hydration');
completion(options.hydrate_error ? new Error('storage unavailable') : null);
assert.equal(Module.syncfsDone, 1);
assert.equal(dependencies.size, 0);
assert.equal(errors.length, options.hydrate_error ? 1 : 0);
""", encoding="utf-8")
    result = subprocess.run(
        [node, str(fixture), str(BOOTSTRAP), json.dumps({
            "browser": browser,
            "explicit_root": explicit_root,
            "hydrate_error": hydrate_error,
            "provided_arguments": provided_arguments,
            "root_option": root_option,
        })],
        text=True,
        capture_output=True,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
