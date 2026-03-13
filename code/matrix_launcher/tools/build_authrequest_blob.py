#!/usr/bin/env python3
import argparse
import base64
import json
import os
import struct
import sys
import time

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa

# Practical shortcut source:
# - /home/morgan/work/mxo-hd/hds/utils/encryption/MxoRSA.cs
# - MxoRSA.decryptWithPrivkey() uses privKeyXML with this modulus/exponent pair
# This helper is explicitly diagnostic/server-guided, not a claim of perfect original
# launcher.exe field population.
SERVER_PUBLIC_MODULUS_B64 = (
    "qMIfEkrXWpRr44ecWMzJHV7Hjg9bnru2PZv3NydzOZ6uab52wET+RoHhIzv+zJb3"
    "zBhmETAtsrmNnBXiW7tfqPK0xf6lb9RbvupfnfYSHO5WaEcWEi0JjQRBevg9d8ql"
    "ETo9Hrfy9PEfpeK1T2WF+xxx73chvBTB12Paa7yT+Ik="
)
SERVER_PUBLIC_EXPONENT_B64 = "EQ=="  # 0x11


def build_public_key() -> rsa.RSAPublicKey:
    modulus = int.from_bytes(base64.b64decode(SERVER_PUBLIC_MODULUS_B64), "big")
    exponent = int.from_bytes(base64.b64decode(SERVER_PUBLIC_EXPONENT_B64), "big")
    return rsa.RSAPublicNumbers(exponent, modulus).public_key()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Build a diagnostic AS_AuthRequest RSA blob using the open-source server key "
            "shortcut."
        )
    )
    parser.add_argument("--username", required=True, help="Auth username")
    parser.add_argument(
        "--time",
        type=int,
        default=None,
        help="Unix timestamp to embed in the plaintext blob (default: current time)",
    )
    parser.add_argument(
        "--twofish-key-hex",
        default=None,
        help="Optional 16-byte auth TF key as 32 hex chars; otherwise generated randomly",
    )
    parser.add_argument(
        "--some-short",
        type=lambda value: int(value, 0),
        default=0x1B,
        help="Recovered ushort after rsaMethod in plaintext blob (default: 0x1b)",
    )
    parser.add_argument(
        "--rsa-method",
        type=lambda value: int(value, 0),
        default=4,
        help="Recovered rsaMethod in plaintext blob (default: 4)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit JSON instead of raw blob hex",
    )
    return parser.parse_args()


def parse_twofish_key(hex_text: str | None) -> bytes:
    if not hex_text:
        return os.urandom(16)
    raw = bytes.fromhex(hex_text)
    if len(raw) != 16:
        raise ValueError("--twofish-key-hex must decode to exactly 16 bytes")
    return raw


def build_plaintext_blob(username: str, embedded_time: int, twofish_key: bytes, rsa_method: int, some_short: int) -> bytes:
    username_bytes = username.encode("ascii") + b"\x00"
    return (
        b"\x00"
        + struct.pack("<I", rsa_method)
        + struct.pack("<H", some_short & 0xFFFF)
        + twofish_key
        + struct.pack("<I", embedded_time & 0xFFFFFFFF)
        + struct.pack("<H", len(username_bytes))
        + username_bytes
    )


def encrypt_blob(plaintext: bytes) -> bytes:
    public_key = build_public_key()
    return public_key.encrypt(
        plaintext,
        padding.OAEP(
            mgf=padding.MGF1(algorithm=hashes.SHA1()),
            algorithm=hashes.SHA1(),
            label=None,
        ),
    )


def main() -> int:
    args = parse_args()
    try:
        twofish_key = parse_twofish_key(args.twofish_key_hex)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    embedded_time = args.time if args.time is not None else int(time.time())
    plaintext = build_plaintext_blob(
        username=args.username,
        embedded_time=embedded_time,
        twofish_key=twofish_key,
        rsa_method=args.rsa_method,
        some_short=args.some_short,
    )
    ciphertext = encrypt_blob(plaintext)

    payload = {
        "username": args.username,
        "embeddedTime": embedded_time,
        "rsaMethod": args.rsa_method,
        "someShort": args.some_short,
        "twofishKeyHex": twofish_key.hex(),
        "plaintextHex": plaintext.hex(),
        "ciphertextHex": ciphertext.hex(),
        "ciphertextLen": len(ciphertext),
        "source": "mxo-hd MxoRSA.cs privKeyXML public modulus/exponent shortcut",
    }

    if args.json:
        json.dump(payload, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        sys.stdout.write(payload["ciphertextHex"])
        sys.stdout.write("\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
