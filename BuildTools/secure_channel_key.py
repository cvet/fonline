#!/usr/bin/python3

"""Generates and inspects the static key of the secure network channel.

The server keeps the secret half, as 64 hex digits, in a file its config reads into
ServerNetwork.ChannelSecretKey; clients pin the public half in ClientNetwork.ChannelServerKeys.
"""

from __future__ import annotations

import argparse
import os
import secrets
import sys
from pathlib import Path

KEY_SIZE = 32
FIELD_PRIME = 2**255 - 19
LADDER_CONSTANT = 121665
BASE_POINT = (9).to_bytes(KEY_SIZE, "little")


def x25519(scalar: bytes, u_coordinate: bytes) -> bytes:
    """RFC 7748 X25519. Not constant time: it runs once, offline, on the host that keeps the key."""

    if len(scalar) != KEY_SIZE or len(u_coordinate) != KEY_SIZE:
        raise ValueError("X25519 inputs are 32 bytes")

    clamped = bytearray(scalar)
    clamped[0] &= 248
    clamped[31] &= 127
    clamped[31] |= 64
    k = int.from_bytes(clamped, "little")

    masked = bytearray(u_coordinate)
    masked[31] &= 127
    x1 = int.from_bytes(masked, "little") % FIELD_PRIME

    x2, z2, x3, z3 = 1, 0, x1, 1
    swap = 0

    for bit_index in reversed(range(255)):
        bit = (k >> bit_index) & 1
        swap ^= bit

        if swap:
            x2, x3 = x3, x2
            z2, z3 = z3, z2

        swap = bit

        a = (x2 + z2) % FIELD_PRIME
        aa = a * a % FIELD_PRIME
        b = (x2 - z2) % FIELD_PRIME
        bb = b * b % FIELD_PRIME
        e = (aa - bb) % FIELD_PRIME
        c = (x3 + z3) % FIELD_PRIME
        d = (x3 - z3) % FIELD_PRIME
        da = d * a % FIELD_PRIME
        cb = c * b % FIELD_PRIME
        x3 = (da + cb) * (da + cb) % FIELD_PRIME
        z3 = x1 * (da - cb) * (da - cb) % FIELD_PRIME
        x2 = aa * bb % FIELD_PRIME
        z2 = e * (aa + LADDER_CONSTANT * e) % FIELD_PRIME

    if swap:
        x2, x3 = x3, x2
        z2, z3 = z3, z2

    return (x2 * pow(z2, FIELD_PRIME - 2, FIELD_PRIME) % FIELD_PRIME).to_bytes(KEY_SIZE, "little")


def derive_public_key(secret_key: bytes) -> bytes:
    return x25519(secret_key, BASE_POINT)


def parse_key(text: str) -> bytes:
    text = text.strip()

    if len(text) != KEY_SIZE * 2:
        raise ValueError("A secure channel key is 64 hexadecimal digits")

    return bytes.fromhex(text)


def generate_secret_file(path: Path) -> bytes:
    """Writes a new secret key readable by its owner only and returns the public key. An existing file is kept."""

    secret_key = secrets.token_bytes(KEY_SIZE)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)

    with os.fdopen(descriptor, "w", encoding="ascii") as file:
        file.write(secret_key.hex() + "\n")

    return derive_public_key(secret_key)


def read_public_key(path: Path) -> bytes:
    return derive_public_key(parse_key(path.read_text(encoding="ascii")))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)

    generate = commands.add_parser("generate", help="create a secret key file and print its public key")
    generate.add_argument("secret_file", type=Path)

    public = commands.add_parser("public", help="print the public key of an existing secret key file")
    public.add_argument("secret_file", type=Path)

    args = parser.parse_args(argv)

    try:
        if args.command == "generate":
            public_key = generate_secret_file(args.secret_file)
        else:
            public_key = read_public_key(args.secret_file)
    except FileExistsError:
        print(f"{args.secret_file} already exists; a key in use is never overwritten", file=sys.stderr)
        return 1
    except (OSError, ValueError) as error:
        print(f"{args.secret_file}: {error}", file=sys.stderr)
        return 1

    print(public_key.hex())
    return 0


if __name__ == "__main__":
    sys.exit(main())
