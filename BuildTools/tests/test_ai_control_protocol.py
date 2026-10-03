from __future__ import annotations

import contextlib
import io
import json
import socket
import sys
import threading
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))
sys.path.insert(0, str(ENGINE_ROOT / "Examples/AiControlSample"))

import ai_control_client  # noqa: E402
import ai_control_sample  # noqa: E402
import run_protocol_smoke  # noqa: E402


class _OneResponsePeer:
    def __init__(self, response: bytes) -> None:
        self._response = response
        self._listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._listener.bind(("127.0.0.1", 0))
        self._listener.listen(1)
        self.port = int(self._listener.getsockname()[1])
        self._thread = threading.Thread(target=self._serve, daemon=True)

    def __enter__(self) -> _OneResponsePeer:
        self._thread.start()
        return self

    def __exit__(self, *_: object) -> None:
        self._listener.close()
        self._thread.join(timeout=2)

    def _serve(self) -> None:
        try:
            connection, _ = self._listener.accept()
            with connection:
                request = b""
                while not request.endswith(b"\n"):
                    block = connection.recv(4096)
                    if not block:
                        return
                    request += block
                connection.sendall(self._response)
        except OSError:
            return


class AiControlProtocolTests(unittest.TestCase):
    def test_end_to_end_protocol_smoke(self) -> None:
        report = run_protocol_smoke.run_smoke(timeout=5.0)

        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["check_count"], 12)
        self.assertIn("completion-event", report["checks"])
        self.assertIn("event-cursor", report["checks"])

    def test_reference_client_rejects_remote_endpoint_without_opt_in(self) -> None:
        with self.assertRaisesRegex(ValueError, "explicit allow_remote"):
            ai_control_client.AiControlClient("192.0.2.10", 43011)
        client = ai_control_client.AiControlClient(
            "192.0.2.10", 43011, allow_remote=True
        )
        client.close()

    def test_reference_cli_never_accepts_a_raw_token(self) -> None:
        help_text = ai_control_client.create_parser().format_help()

        self.assertIn("--token-env", help_text)
        self.assertNotIn("--token TOKEN", help_text)
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            ai_control_client.create_parser().parse_args(
                ["--token", "secret", "ping"]
            )

    def test_reference_client_rejects_mismatched_response_id(self) -> None:
        response = b'{"jsonrpc":"2.0","id":99,"result":{"ok":true}}\n'
        with _OneResponsePeer(response) as peer:
            with ai_control_client.AiControlClient(
                "127.0.0.1", peer.port
            ) as client:
                with self.assertRaisesRegex(
                    ai_control_client.ProtocolError, "id does not match"
                ):
                    client.ping()

    def test_reference_client_rejects_ambiguous_response(self) -> None:
        response = (
            b'{"jsonrpc":"2.0","id":1,"result":{},'
            b'"error":{"code":-1,"message":"bad"}}\n'
        )
        with _OneResponsePeer(response) as peer:
            with ai_control_client.AiControlClient(
                "127.0.0.1", peer.port
            ) as client:
                with self.assertRaisesRegex(
                    ai_control_client.ProtocolError, "exactly one"
                ):
                    client.status()

    def test_reference_client_rejects_malformed_and_oversized_responses(self) -> None:
        with _OneResponsePeer(b"not-json\n") as peer:
            with ai_control_client.AiControlClient(
                "127.0.0.1", peer.port
            ) as client:
                with self.assertRaisesRegex(
                    ai_control_client.ProtocolError, "not valid UTF-8 JSON"
                ):
                    client.observe()

        oversized = b"{" + b" " * ai_control_client.MAX_LINE_BYTES + b"}\n"
        with _OneResponsePeer(oversized) as peer:
            with ai_control_client.AiControlClient(
                "127.0.0.1", peer.port
            ) as client:
                with self.assertRaisesRegex(
                    ai_control_client.ProtocolError, "exceeds"
                ):
                    client.observe()

    def test_sample_state_bounds_commands_events_and_completion(self) -> None:
        state = ai_control_sample.SampleState("token", 1, 1)

        first_seq = state.enqueue({"type": "echo", "stringArg": "first"})
        self.assertEqual(first_seq, 1)
        self.assertIsNone(state.enqueue({"type": "echo"}))
        self.assertTrue(state.process_one())
        self.assertEqual(
            state.events_after(0, 100)["events"][0]["event"],
            {
                "type": "command_completed",
                "commandSeq": 1,
                "success": True,
                "message": "first",
            },
        )
        self.assertEqual(state.enqueue({"type": "fail"}), 2)
        self.assertTrue(state.process_one())
        retained = state.events_after(0, 100)["events"]
        self.assertEqual(len(retained), 1)
        self.assertEqual(retained[0]["seq"], 2)
        self.assertFalse(retained[0]["event"]["success"])

    def test_protocol_constants_and_error_codes_are_pinned(self) -> None:
        self.assertEqual(ai_control_client.JSONRPC_VERSION, "2.0")
        self.assertEqual(ai_control_client.PROTOCOL_VERSION, 1)
        self.assertEqual(ai_control_client.DEFAULT_HOST, "127.0.0.1")
        self.assertEqual(ai_control_client.DEFAULT_PORT, 43011)
        self.assertEqual(ai_control_client.MAX_LINE_BYTES, 1024 * 1024)
        self.assertEqual(
            ai_control_client.METHODS,
            ("auth", "ping", "status", "observe", "events", "act"),
        )
        self.assertEqual(
            {
                ai_control_sample.ERROR_PARSE,
                ai_control_sample.ERROR_INVALID_REQUEST,
                ai_control_sample.ERROR_METHOD_NOT_FOUND,
                ai_control_sample.ERROR_INVALID_PARAMS,
                ai_control_sample.ERROR_UNAUTHORIZED,
                ai_control_sample.ERROR_QUEUE_FULL,
            },
            {-32700, -32600, -32601, -32602, -32001, -32002},
        )


if __name__ == "__main__":
    unittest.main()
