from __future__ import annotations

import contextlib
import io
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_metadata  # noqa: E402


def _encode_metadata(sections: dict[str, list[list[str]]]) -> bytes:
    metadata_version = b"test-metadata-version"
    data = bytearray(
        struct.pack(
            "<IHH",
            docs_metadata.METADATA_FILE_MAGIC,
            docs_metadata.METADATA_FILE_VERSION,
            len(metadata_version),
        )
    )
    data.extend(metadata_version)
    data.extend(struct.pack("<H", len(sections)))
    for name, entries in sections.items():
        name_bytes = name.encode("utf-8")
        data.extend(struct.pack("<H", len(name_bytes)))
        data.extend(name_bytes)
        data.extend(struct.pack("<I", len(entries)))
        for entry in entries:
            data.extend(struct.pack("<I", len(entry)))
            for part in entry:
                part_bytes = part.encode("utf-8")
                data.extend(struct.pack("<H", len(part_bytes)))
                data.extend(part_bytes)
    return bytes(data)


def _write_metadata(root: Path, side: str, remote_calls: list[list[str]]) -> Path:
    path = root / f"Metadata.fometa-{side.lower()}"
    path.write_bytes(_encode_metadata({"Target": [[side]], "RemoteCall": remote_calls}))
    return path


class DocumentationMetadataTests(unittest.TestCase):
    def test_paired_remote_calls_decode_to_deterministic_json_and_markdown(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            server_calls = [
                [
                    "CoverageCall",
                    "Coverage.fos",
                    "In",
                    "int32",
                    "",
                    "amount",
                    "string",
                    "?",
                    "note",
                    "Limits",
                    "4096",
                    "32",
                ],
                ["ClientCoverage", "Coverage.fos", "Out", "Limits", "0", "0"],
            ]
            client_calls = [
                [
                    "CoverageCall",
                    "Coverage.fos",
                    "Out",
                    "int32",
                    "",
                    "amount",
                    "string",
                    "?",
                    "note",
                    "Limits",
                    "4096",
                    "32",
                ],
                ["ClientCoverage", "Coverage.fos", "In", "Limits", "0", "0"],
            ]
            server_path = _write_metadata(root, "Server", server_calls)
            client_path = _write_metadata(root, "Client", client_calls)

            first = docs_metadata.generate_remote_call_model(root, [server_path, client_path])
            second = docs_metadata.generate_remote_call_model(root, [client_path, server_path])
            self.assertEqual(first, second)
            self.assertEqual(first["summary"]["remote_call_count"], 2)
            self.assertEqual(first["summary"]["paired_remote_call_count"], 2)

            server_call = next(symbol for symbol in first["symbols"] if symbol["target"] == "server")
            self.assertEqual(server_call["id"], "script.remote-call.server.CoverageCall")
            self.assertEqual(server_call["arguments"][1], {"name": "note", "type": "string?", "nullable": True})
            self.assertEqual(server_call["handler_attribute"], "ServerRemoteCall")
            self.assertEqual(server_call["limits"], {"max_bytes": 4096, "max_collection_size": 32})
            self.assertEqual(
                server_call["handler_signature"],
                "void Coverage::CoverageCall(Player player, int32 amount, string? note)",
            )
            self.assertEqual(
                server_call["caller_surfaces"],
                ["Player.ServerCall.CoverageCall(int32 amount, string? note)"],
            )

            markdown = docs_metadata.render_remote_call_markdown(first)
            self.assertIn("script.remote-call.server.CoverageCall", markdown)
            self.assertIn("client/out, server/in", markdown)
            self.assertIn("MetadataBaker output", markdown)
            self.assertIn("MaxBytes 4096; MaxCollectionSize 32", markdown)

    def test_mismatched_or_unpaired_metadata_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            server_path = _write_metadata(
                root,
                "Server",
                [["CoverageCall", "Coverage.fos", "In", "int32", "", "value", "Limits", "1024", "8"]],
            )
            client_path = _write_metadata(
                root,
                "Client",
                [["CoverageCall", "Coverage.fos", "Out", "bool", "", "value", "Limits", "1024", "8"]],
            )

            with self.assertRaisesRegex(docs_metadata.MetadataDecodeError, "differs between baked inputs"):
                docs_metadata.generate_remote_call_model(root, [server_path, client_path])
            with self.assertRaisesRegex(docs_metadata.MetadataDecodeError, "not paired"):
                docs_metadata.generate_remote_call_model(root, [server_path])

            partial = docs_metadata.generate_remote_call_model(root, [server_path], require_paired=False)
            self.assertEqual(partial["summary"]["paired_remote_call_count"], 0)

    def test_remote_call_limits_trailer_is_required_and_validated(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            invalid_entries = [
                ["CoverageCall", "Coverage.fos", "In"],
                ["CoverageCall", "Coverage.fos", "In", "Limit", "1", "2"],
                ["CoverageCall", "Coverage.fos", "In", "Limits", "many", "2"],
                ["CoverageCall", "Coverage.fos", "In", "Limits", "1", "-2"],
            ]
            for entry in invalid_entries:
                with self.subTest(entry=entry):
                    path = _write_metadata(root, "Server", [entry])
                    with self.assertRaisesRegex(docs_metadata.MetadataDecodeError, "RemoteCall"):
                        docs_metadata.generate_remote_call_model(root, [path], require_paired=False)

    def test_decoder_rejects_truncation_trailing_bytes_and_invalid_utf8(self) -> None:
        valid = _encode_metadata({"Target": [["Server"]]})
        with self.assertRaisesRegex(docs_metadata.MetadataDecodeError, "Truncated metadata"):
            docs_metadata.decode_metadata(valid[:-1])
        with self.assertRaisesRegex(docs_metadata.MetadataDecodeError, "trailing bytes"):
            docs_metadata.decode_metadata(valid + b"x")

        header_size = struct.calcsize("<IHH") + len(b"test-metadata-version")
        invalid_utf8 = valid[:header_size] + struct.pack("<HH", 1, 1) + b"\xff" + struct.pack("<I", 0)
        with self.assertRaisesRegex(docs_metadata.MetadataDecodeError, "Invalid UTF-8"):
            docs_metadata.decode_metadata(invalid_utf8)

    def test_decoder_rejects_invalid_metadata_header(self) -> None:
        valid = _encode_metadata({"Target": [["Server"]]})
        with self.assertRaisesRegex(docs_metadata.MetadataDecodeError, "metadata file marker"):
            docs_metadata.decode_metadata(struct.pack("<I", 0) + valid[4:])
        with self.assertRaisesRegex(docs_metadata.MetadataDecodeError, "file version does not match"):
            docs_metadata.decode_metadata(valid[:4] + struct.pack("<H", 3) + valid[6:])
        with self.assertRaisesRegex(docs_metadata.MetadataDecodeError, "carries no version"):
            docs_metadata.decode_metadata(valid[:6] + struct.pack("<H", 0) + valid[8 + len(b"test-metadata-version") :])

    def test_write_and_check_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            server_path = _write_metadata(
                root, "Server", [["CoverageCall", "Coverage.fos", "In", "Limits", "0", "0"]]
            )
            client_path = _write_metadata(
                root, "Client", [["CoverageCall", "Coverage.fos", "Out", "Limits", "0", "0"]]
            )
            args = [
                "--root",
                str(root),
                "--metadata",
                str(server_path),
                "--metadata",
                str(client_path),
            ]

            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(docs_metadata.main([*args, "--write"]), 0)
                self.assertEqual(docs_metadata.main([*args, "--check"]), 0)
                output = root / docs_metadata.DEFAULT_JSON_OUTPUT
                model = json.loads(output.read_text(encoding="utf-8"))
                self.assertEqual(model["summary"]["remote_call_count"], 1)
                output.write_text("stale\n", encoding="utf-8")
                self.assertEqual(docs_metadata.main([*args, "--check"]), 1)


if __name__ == "__main__":
    unittest.main()
