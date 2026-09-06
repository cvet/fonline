"""Tests for the download side of buildtools.

Everything these functions fetch comes from a machine somebody else runs, so the properties worth
pinning are the ones whose failure is silent: a CI credential must never travel to an upstream host, a
connection that drops mid-archive must not leave a half file that unpacks into a failure far from its
cause, and a workspace cache that is empty or unreachable must slow the build down rather than fail it.
"""
from __future__ import annotations

import os
import sys
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

import buildtools


class ServerStub(BaseHTTPRequestHandler):
    body = b"archive payload"
    truncate = False
    missing = False
    seen: list[tuple[str, str, str | None]] = []
    stored: dict[str, bytes] = {}

    def do_GET(self) -> None:  # noqa: N802
        ServerStub.seen.append(("GET", self.path, self.headers.get("Authorization")))

        if ServerStub.missing:
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return

        self.send_response(200)
        self.send_header("Content-Length", str(len(ServerStub.body)))
        self.end_headers()

        if ServerStub.truncate:
            self.wfile.write(ServerStub.body[: len(ServerStub.body) // 2])
            self.close_connection = True
            return

        self.wfile.write(ServerStub.body)

    def do_PUT(self) -> None:  # noqa: N802
        ServerStub.seen.append(("PUT", self.path, self.headers.get("Authorization")))
        length = int(self.headers.get("Content-Length", "0"))
        ServerStub.stored[self.path] = self.rfile.read(length)
        self.send_response(201)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, *_args) -> None:
        pass


class MirroredUrlTests(unittest.TestCase):
    def test_no_mirror_leaves_the_url_alone(self) -> None:
        with mock.patch.dict(os.environ, {buildtools.DOWNLOAD_MIRROR_VAR: ""}, clear=False):
            self.assertEqual(buildtools.mirrored_url("https://dl.google.com/ndk.zip"),
                             "https://dl.google.com/ndk.zip")

    def test_a_mirror_takes_the_host_into_the_path(self) -> None:
        with mock.patch.dict(os.environ, {buildtools.DOWNLOAD_MIRROR_VAR: "https://ci.example/cache/"}):
            self.assertEqual(buildtools.mirrored_url("https://dl.google.com/android/ndk.zip"),
                             "https://ci.example/cache/dl.google.com/android/ndk.zip")

    def test_a_query_is_passed_through_rather_than_dropped(self) -> None:
        """A mirror that refuses it fails visibly; dropping it would fetch the wrong file in silence."""
        with mock.patch.dict(os.environ, {buildtools.DOWNLOAD_MIRROR_VAR: "https://ci.example/cache"}):
            self.assertEqual(buildtools.mirrored_url("https://host/file?v=2"),
                             "https://ci.example/cache/host/file?v=2")

    def test_a_scheme_that_is_not_http_is_left_alone(self) -> None:
        with mock.patch.dict(os.environ, {buildtools.DOWNLOAD_MIRROR_VAR: "https://ci.example/cache"}):
            self.assertEqual(buildtools.mirrored_url("file:///tmp/archive.zip"), "file:///tmp/archive.zip")


class CiTransferTests(unittest.TestCase):
    def setUp(self) -> None:
        ServerStub.body = b"archive payload"
        ServerStub.truncate = False
        ServerStub.missing = False
        ServerStub.seen = []
        ServerStub.stored = {}
        self._server = ThreadingHTTPServer(("127.0.0.1", 0), ServerStub)
        threading.Thread(target=self._server.serve_forever, daemon=True).start()
        self.base = f"http://127.0.0.1:{self._server.server_address[1]}"
        self._dir = TemporaryDirectory()
        self.work = Path(self._dir.name)
        self.target = self.work / "archive"

    def tearDown(self) -> None:
        self._server.shutdown()
        self._server.server_close()
        self._dir.cleanup()

    def ours(self, **extra: str) -> dict[str, str]:
        env = {buildtools.DOWNLOAD_MIRROR_VAR: f"{self.base}/cache",
               buildtools.WORKSPACE_CACHE_VAR: f"{self.base}/workspace",
               buildtools.CI_TOKEN_VAR: "job-token"}
        env.update(extra)
        return env

    def test_the_token_reaches_our_own_host(self) -> None:
        with mock.patch.dict(os.environ, self.ours()):
            buildtools.fetch_url(f"{self.base}/cache/host/archive", self.target)

        self.assertEqual(self.target.read_bytes(), b"archive payload")
        self.assertEqual(ServerStub.seen[-1][2], "Bearer job-token")

    def test_the_token_never_reaches_a_host_that_is_not_ours(self) -> None:
        """The credential belongs to our host; an upstream address must not be handed it."""
        with mock.patch.dict(os.environ, self.ours(**{buildtools.DOWNLOAD_MIRROR_VAR: "https://ci.example/cache",
                                                      buildtools.WORKSPACE_CACHE_VAR: "https://ci.example/ws"})):
            buildtools.fetch_url(f"{self.base}/direct/archive", self.target)

        self.assertIsNone(ServerStub.seen[-1][2])

    def test_a_truncated_download_is_an_error(self) -> None:
        ServerStub.truncate = True

        with mock.patch.dict(os.environ, {buildtools.DOWNLOAD_MIRROR_VAR: ""}):
            with self.assertRaises(OSError):
                buildtools.fetch_url(f"{self.base}/archive", self.target)

    def test_a_workspace_cache_hit_is_taken_and_authorized(self) -> None:
        with mock.patch.dict(os.environ, self.ours()):
            self.assertTrue(buildtools.workspace_cache_fetch("xwin-1.tar.gz", self.target))

        self.assertEqual(self.target.read_bytes(), b"archive payload")
        self.assertEqual(ServerStub.seen[-1][:2], ("GET", "/workspace/xwin-1.tar.gz"))
        self.assertEqual(ServerStub.seen[-1][2], "Bearer job-token")

    def test_a_workspace_cache_miss_is_reported_rather_than_raised(self) -> None:
        """The cache exists to make the build faster, not to become another way for it to die."""
        ServerStub.missing = True

        with mock.patch.dict(os.environ, self.ours()):
            self.assertFalse(buildtools.workspace_cache_fetch("xwin-1.tar.gz", self.target))

        self.assertFalse(self.target.exists())

    def test_an_unreachable_workspace_cache_is_also_only_a_miss(self) -> None:
        with mock.patch.dict(os.environ, self.ours(**{buildtools.WORKSPACE_CACHE_VAR:
                                                      "http://127.0.0.1:1/workspace"})):
            self.assertFalse(buildtools.workspace_cache_fetch("xwin-1.tar.gz", self.target))

    def test_no_workspace_cache_configured_is_a_miss_without_a_request(self) -> None:
        with mock.patch.dict(os.environ, {buildtools.WORKSPACE_CACHE_VAR: ""}):
            self.assertFalse(buildtools.workspace_cache_fetch("xwin-1.tar.gz", self.target))

        self.assertEqual(ServerStub.seen, [])

    def test_a_prepared_workspace_is_stored(self) -> None:
        self.target.write_bytes(b"packed splat tree")

        with mock.patch.dict(os.environ, self.ours()):
            buildtools.workspace_cache_store("xwin-1.tar.gz", self.target)

        self.assertEqual(ServerStub.stored["/workspace/xwin-1.tar.gz"], b"packed splat tree")
        self.assertEqual(ServerStub.seen[-1][2], "Bearer job-token")

    def test_a_failing_store_does_not_fail_the_build(self) -> None:
        self.target.write_bytes(b"packed splat tree")

        with mock.patch.dict(os.environ, self.ours(**{buildtools.WORKSPACE_CACHE_VAR:
                                                      "http://127.0.0.1:1/workspace"})):
            buildtools.workspace_cache_store("xwin-1.tar.gz", self.target)


if __name__ == "__main__":
    unittest.main()
