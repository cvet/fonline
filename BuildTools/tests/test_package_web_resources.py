from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import package as _package  # noqa: E402


@pytest.fixture
def pinned_packager() -> Path:
    configured = os.environ.get("FO_TEST_EMSCRIPTEN_ROOT")
    if not configured:
        pytest.skip("FO_TEST_EMSCRIPTEN_ROOT must point to the prepared pinned Emscripten source")
    root = Path(configured)
    expected = (BUILDTOOLS_DIR.parent / "ThirdParty" / "emscripten").read_text().strip()
    assert json.loads((root / "emscripten-version.txt").read_text()) == expected
    return root / "tools" / "file_packager.py"


def _metadata(loader: Path) -> list[dict]:
    source = loader.read_text(encoding="utf-8")
    return [json.JSONDecoder().raw_decode(source[match.end():])[0]
            for match in re.finditer(r'\bloadPackage\((?=\{"files")', source)]


@pytest.mark.parametrize("missing_bundle", [False, True])
def test_web_bundle_boundaries_and_paths_survive_pinned_packager(
    tmp_path: Path, pinned_packager: Path, capfd: pytest.CaptureFixture[str], missing_bundle: bool
) -> None:
    inputs = tmp_path / "input @ space"
    inputs.mkdir()
    output = tmp_path / "output"
    output.mkdir()
    contents = {
        "/ManagedRuntime/lib/netcoreapp/Core.dll": b"a" * 32,
        "/Resources/a @ 'кириллица'.zip": b"b" * 32,
        "/Resources/b.zip": b"c" * 33,
        "/Resources/empty.zip": b"",
    }
    files = []
    for index, (virtual, data) in enumerate(contents.items()):
        source = inputs / f"{index} @ 'кириллица'.bin"
        source.write_bytes(data)
        files.append((source, virtual))

    _package.package_web_resources(output, pinned_packager, files[::-1], 64)
    first_bundles = {path.name: path.read_bytes() for path in output.glob("*.data")}
    _package.package_web_resources(output, pinned_packager, files, 64)

    assert {path.name: path.read_bytes() for path in output.glob("*.data")} == first_bundles
    metadata = _metadata(output / "Resources.js")
    assert [sum(file["end"] - file["start"] for file in bundle["files"]) for bundle in metadata] == [64, 33]
    assert [file["filename"] for bundle in metadata for file in bundle["files"]] == sorted(contents)
    assert len(list(output.glob("*.js"))) == 1
    assert "warning:" not in capfd.readouterr().err

    node = shutil.which("node")
    assert node, "Node.js is required by the pinned file packager"
    check = tmp_path / "check.cjs"
    check.write_text(r"""
const fs = require('fs');
const vm = require('vm');
const path = require('path');
const crypto = require('crypto');
global.assert = require('assert');
const codec = require(process.argv[2]);
const output = process.argv[3];
const missingBundle = process.argv[4] === 'missing';
const hashes = {};
const dependencies = new Set();
const pending = [];
const Module = {
  preRun: [],
  FS_createPath: undefined,
  addRunDependency: name => dependencies.add(name),
  removeRunDependency: name => assert(dependencies.delete(name)),
  LZ4: {loadPackage: async ({metadata, compressedData: data}) => {
    const total = Math.max(...metadata.files.map(file => file.end));
    const unpacked = Buffer.alloc(total);
    for (let i = 0; i < data.offsets.length; ++i) {
      const chunk = data.data.subarray(data.offsets[i], data.offsets[i] + data.sizes[i]);
      const decoded = new Uint8Array(codec.CHUNK_SIZE);
      const size = data.successes[i] ? codec.uncompress(chunk, decoded) : chunk.length;
      unpacked.set((data.successes[i] ? decoded : chunk).subarray(0, size), i * codec.CHUNK_SIZE);
    }
    for (const file of metadata.files) {
      assert(!(file.filename in hashes));
      hashes[file.filename] = crypto.createHash('sha256').update(unpacked.subarray(file.start, file.end)).digest('hex');
    }
  }}
};
const context = {Module, Uint8Array, ArrayBuffer,
  fetch: name => new Promise(resolve => pending.push(() => {
    resolve(missingBundle && name.endsWith('-1.data')
      ? new Response(null, {status: 404}) : new Response(fs.readFileSync(path.join(output, name))));
  }))
};
vm.runInNewContext(fs.readFileSync(path.join(output, 'Resources.js'), 'utf8'), context);
assert.equal(pending.length, 2);
Module.FS_createPath = () => {};
const loads = Module.preRun.map(start => start(Module));
assert.equal(dependencies.size, 2);
(async () => {
  pending[0]();
  await loads[0];
  assert.equal(dependencies.size, 1);
  pending[1]();
  if (missingBundle) {
    await assert.rejects(Promise.all(loads), /404/);
    assert.equal(dependencies.size, 1);
    process.stdout.write(JSON.stringify({blocked: true, hashes}));
    return;
  }
  await Promise.all(loads);
  assert.equal(dependencies.size, 0);
  process.stdout.write(JSON.stringify(hashes));
})().catch(error => { console.error(error); process.exitCode = 1; });
""", encoding="utf-8")
    result = subprocess.run(
        [node, str(check), str(pinned_packager.parents[1] / "third_party" / "mini-lz4.js"), str(output),
         "missing" if missing_bundle else "complete"],
        capture_output=True, text=True, check=True,
    )
    decoded = json.loads(result.stdout)
    if missing_bundle:
        assert decoded["blocked"] is True
        assert sorted(decoded["hashes"]) == sorted(contents)[:2]
    else:
        assert decoded == {name: hashlib.sha256(data).hexdigest() for name, data in contents.items()}


@pytest.mark.parametrize("kind", ["oversized", "duplicate", "empty"])
def test_invalid_web_preloads_fail_before_packaging(tmp_path: Path, kind: str) -> None:
    source = tmp_path / "asset"
    source.write_bytes(b"12345")
    files = [(source, "/Resources/asset")]
    expected = "exceeds bundle limit"
    limit = 4
    if kind == "duplicate":
        files *= 2
        limit = 8
        expected = "Duplicate Web asset path"
    elif kind == "empty":
        files = []
        expected = "requires preloaded files"
    with pytest.raises(AssertionError, match=expected):
        _package.package_web_resources(tmp_path, tmp_path / "must-not-run.py", files, limit)
    assert not (tmp_path / "Resources.js").exists()


def test_packager_stderr_and_nonzero_exit_are_preserved(tmp_path: Path, capfd: pytest.CaptureFixture[str]) -> None:
    source = tmp_path / "asset"
    source.write_bytes(b"asset")
    failing_packager = tmp_path / "failing_packager.py"
    failing_packager.write_text(
        "import sys\nprint('warning: fixture warning', file=sys.stderr)\n"
        "print('error: fixture failure', file=sys.stderr)\nsys.exit(7)\n",
        encoding="utf-8",
    )
    with pytest.raises(AssertionError, match="file_packager.py failed for Resources-0: 7"):
        _package.package_web_resources(tmp_path, failing_packager, [(source, "/Resources/asset")])
    assert capfd.readouterr().err == "warning: fixture warning\nerror: fixture failure\n"
    assert not (tmp_path / "Resources.js").exists()


def test_pinned_quiet_preserves_other_warnings_and_errors(tmp_path: Path, pinned_packager: Path) -> None:
    files = []
    for index in range(2):
        source = tmp_path / f"{index}.bin"
        source.write_bytes(b"a" * (700 * 1024))
        files.append(source.as_posix() + f"@/Resources/{index}")
    arguments = [sys.executable, str(pinned_packager), str(tmp_path / "warning.data"),
                 "--preload", *files, "--quiet", "--js-output=" + str(tmp_path / "warning.js")]
    result = subprocess.run(arguments, capture_output=True, text=True,
                            env={**os.environ, "EM_FILE_PACKAGER_MAX_CHUNK_SIZE_MB": "1"})
    assert result.returncode == 0
    assert "splitting bundle into 2 chunks" in result.stderr
    assert "Remember to build the main file" not in result.stderr
    failed = subprocess.run(arguments + ["--invalid-option"], capture_output=True, text=True)
    assert failed.returncode != 0
    assert "Unknown parameter" in failed.stderr
