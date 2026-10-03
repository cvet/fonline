from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tarfile
import threading
import time
import zipfile
from pathlib import Path


TIMEOUT_SECONDS = 30
PACKAGE_ID = "PackageSmoke"
DEV_NAME = "FOPKG"
NICE_NAME = "FOnline Packaging Matrix"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def payload_inventory(root: Path) -> list[dict[str, object]]:
    return [
        {
            "path": path.relative_to(root).as_posix(),
            "size": path.stat().st_size,
            "sha256": sha256(path),
        }
        for path in sorted(root.rglob("*"))
        if path.is_file()
    ]


def archive_members(path: Path) -> list[str]:
    if path.suffix == ".zip":
        with zipfile.ZipFile(path) as archive:
            return sorted(name for name in archive.namelist() if not name.endswith("/"))
    with tarfile.open(path, "r:gz") as archive:
        return sorted(member.name for member in archive.getmembers() if member.isfile())


def verify_archive(payload_root: Path, archive_path: Path) -> None:
    expected = sorted(
        path.relative_to(payload_root).as_posix()
        for path in payload_root.rglob("*")
        if path.is_file()
    )
    prefix = f"{payload_root.name}/"
    actual = sorted(
        name[len(prefix) :] if name.startswith(prefix) else name
        for name in archive_members(archive_path)
    )
    if actual != expected:
        missing = sorted(set(expected) - set(actual))
        extra = sorted(set(actual) - set(expected))
        raise RuntimeError(f"Archive inventory mismatch for {archive_path}: missing={missing}, extra={extra}")


def run_packaged(binary: Path, marker: str) -> dict[str, object]:
    try:
        result = subprocess.run(
            [str(binary)],
            cwd=binary.parent,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=TIMEOUT_SECONDS,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        output = error.stdout or ""
        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")
        print(output, end="")
        raise RuntimeError(f"Packaged binary timed out after {TIMEOUT_SECONDS}s: {binary}") from error
    print(result.stdout, end="")
    if result.returncode != 0 or marker not in result.stdout:
        raise RuntimeError(
            f"Packaged binary failed: path={binary}, exit={result.returncode}, marker={marker!r}"
        )
    return {"binary": binary.name, "exit_code": result.returncode, "marker": marker}


class CapturedProcess:
    def __init__(self, binary: Path) -> None:
        self.binary = binary
        self.lines: list[str] = []
        self.lock = threading.Lock()
        self.process = subprocess.Popen(
            [str(binary)],
            cwd=binary.parent,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        self.thread = threading.Thread(target=self._capture, daemon=True)
        self.thread.start()

    def _capture(self) -> None:
        assert self.process.stdout is not None
        for line in self.process.stdout:
            with self.lock:
                self.lines.append(line)
            print(line, end="")

    def output(self) -> str:
        with self.lock:
            return "".join(self.lines)

    def wait_for_marker(self, marker: str, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if marker in self.output():
                return True
            if self.process.poll() is not None:
                return marker in self.output()
            time.sleep(0.05)
        return False

    def stop(self) -> None:
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
        self.thread.join(timeout=2)


def run_packaged_pair(server: Path, client: Path) -> list[dict[str, object]]:
    server_process = CapturedProcess(server)
    try:
        if not server_process.wait_for_marker("packaging_matrix_server_ready", TIMEOUT_SECONDS):
            raise RuntimeError(f"Packaged server did not become ready: {server}")
        client_result = run_packaged(client, "packaging_matrix_client_passed")
        try:
            server_code = server_process.process.wait(timeout=TIMEOUT_SECONDS)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"Packaged server did not stop cleanly: {server}") from error
        server_process.thread.join(timeout=2)
        if server_code != 0 or "packaging_matrix_server_passed" not in server_process.output():
            raise RuntimeError(f"Packaged server failed: path={server}, exit={server_code}")
        server_result = {
            "binary": server.name,
            "exit_code": server_code,
            "marker": "packaging_matrix_server_passed",
        }
        return [server_result, client_result]
    finally:
        server_process.stop()


def git_revision(engine_root: Path) -> str:
    result = subprocess.run(
        ["git", "-C", str(engine_root), "rev-parse", "HEAD"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        check=True,
    )
    return result.stdout.strip()


def verify(package_root: Path, engine_root: Path, platform: str) -> dict[str, object]:
    if platform not in {"Windows", "Linux"}:
        raise ValueError(f"Unsupported fixture platform: {platform}")
    platform_suffix = "" if platform == "Windows" else "-Linux"
    archive_suffix = ".zip" if platform == "Windows" else ".tar.gz"
    executable_suffix = ".exe" if platform == "Windows" else ""
    client_root = package_root / f"{DEV_NAME}-Client-{PACKAGE_ID}{platform_suffix}"
    server_root = package_root / f"{DEV_NAME}-Server-{PACKAGE_ID}{platform_suffix}"
    client_archive = Path(str(client_root) + archive_suffix)
    server_archive = Path(str(server_root) + archive_suffix)

    required_paths = [client_root, server_root, client_archive, server_archive]
    missing = [str(path) for path in required_paths if not path.exists()]
    if missing:
        raise RuntimeError(f"Missing package outputs: {', '.join(missing)}")

    client_headless = client_root / f"{NICE_NAME}_Headless{executable_suffix}"
    server_headless = server_root / f"{DEV_NAME}_ServerHeadless{executable_suffix}"
    service_role = "ServerService" if platform == "Windows" else "ServerDaemon"
    service_binary = server_root / f"{DEV_NAME}_{service_role}{executable_suffix}"
    for binary in (client_headless, server_headless, service_binary):
        if not binary.is_file():
            raise RuntimeError(f"Missing packaged role binary: {binary}")
        if platform == "Linux" and not os.access(binary, os.X_OK):
            raise RuntimeError(f"Packaged Linux binary is not executable: {binary}")

    verify_archive(client_root, client_archive)
    verify_archive(server_root, server_archive)
    runtime_checks = run_packaged_pair(server_headless, client_headless)
    artifacts = [
        {"path": path.name, "size": path.stat().st_size, "sha256": sha256(path)}
        for path in (client_archive, server_archive)
    ]
    manifest = {
        "schema_version": 1,
        "package_id": PACKAGE_ID,
        "platform": platform,
        "engine_revision": git_revision(engine_root),
        "artifacts": artifacts,
        "payloads": {
            "client": payload_inventory(client_root),
            "server": payload_inventory(server_root),
        },
        "runtime_checks": runtime_checks,
        "role_presence": [client_headless.name, server_headless.name, service_binary.name],
    }
    manifest_path = package_root / "packaging-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Packaging matrix passed; evidence: {manifest_path}")
    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Verify FOnline package outputs and packaged runtime startup.")
    parser.add_argument("--package-root", type=Path, required=True)
    parser.add_argument("--engine-root", type=Path, required=True)
    parser.add_argument("--platform", choices=("Windows", "Linux"), required=True)
    args = parser.parse_args(argv)
    try:
        verify(args.package_root.resolve(), args.engine_root.resolve(), args.platform)
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as error:
        print(f"[packaging-matrix] {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
