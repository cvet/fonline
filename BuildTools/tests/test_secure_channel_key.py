from __future__ import annotations

import importlib.util
import os
import stat
import sys
from pathlib import Path

import pytest

MODULE_PATH = Path(__file__).resolve().parents[1] / "secure_channel_key.py"
SPEC = importlib.util.spec_from_file_location("secure_channel_key", MODULE_PATH)
secure_channel_key = importlib.util.module_from_spec(SPEC)
sys.modules["secure_channel_key"] = secure_channel_key
SPEC.loader.exec_module(secure_channel_key)


def test_x25519_matches_rfc7748() -> None:
    alice_secret = bytes.fromhex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a")
    bob_secret = bytes.fromhex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb")
    alice_public = secure_channel_key.derive_public_key(alice_secret)
    bob_public = secure_channel_key.derive_public_key(bob_secret)

    assert alice_public.hex() == "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a"
    assert bob_public.hex() == "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f"
    shared = "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742"
    assert secure_channel_key.x25519(alice_secret, bob_public).hex() == shared
    assert secure_channel_key.x25519(bob_secret, alice_public).hex() == shared


def test_x25519_matches_rfc7748_iterated_vector() -> None:
    k = bytes.fromhex("0900000000000000000000000000000000000000000000000000000000000000")
    u = k

    k, u = secure_channel_key.x25519(k, u), k

    assert k.hex() == "422c8e7a6227d7bca1350b3e2bb7279f7897b87bb6854b783c60e80311ae3079"


def test_generate_writes_an_owner_only_file_and_never_overwrites(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    secret_file = tmp_path / "keys" / "channel.key"

    assert secure_channel_key.main(["generate", str(secret_file)]) == 0
    public_hex = capsys.readouterr().out.strip()

    assert len(public_hex) == 64
    assert secure_channel_key.read_public_key(secret_file).hex() == public_hex

    if os.name != "nt":
        assert stat.S_IMODE(secret_file.stat().st_mode) == 0o600

    original = secret_file.read_text(encoding="ascii")

    assert secure_channel_key.main(["generate", str(secret_file)]) == 1
    assert secret_file.read_text(encoding="ascii") == original

    assert secure_channel_key.main(["public", str(secret_file)]) == 0
    assert capsys.readouterr().out.strip() == public_hex


def test_public_rejects_a_malformed_key(tmp_path: Path) -> None:
    secret_file = tmp_path / "channel.key"
    secret_file.write_text("abc\n", encoding="ascii")

    assert secure_channel_key.main(["public", str(secret_file)]) == 1
