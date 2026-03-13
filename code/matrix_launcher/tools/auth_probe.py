#!/usr/bin/env python3
import argparse
import json
import pathlib
import socket
import struct
import sys
import time
from typing import Any

THIS_DIR = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))
import build_authrequest_blob as auth_blob  # noqa: E402

AUTH_OPCODE_NAMES = {
    0x06: "AS_GetPublicKeyRequest",
    0x07: "AS_GetPublicKeyReply",
    0x08: "AS_AuthRequest",
    0x09: "AS_AuthChallenge",
    0x0A: "AS_AuthChallengeResponse",
    0x0B: "AS_AuthReply",
    0x35: "AS_GetWorldListRequest",
    0x36: "AS_GetWorldListReply",
}


def auth_opcode_name(opcode: int) -> str:
    return AUTH_OPCODE_NAMES.get(opcode, f"UNKNOWN_AUTH_0x{opcode:02X}")


def frame_payload(payload: bytes) -> bytes:
    payload_len = len(payload)
    if payload_len > 0x7FFF:
        raise ValueError("payload too large for current auth framing helper")
    if payload_len < 0x80:
        return bytes([payload_len]) + payload
    return bytes([0x80 | ((payload_len >> 8) & 0x7F), payload_len & 0xFF]) + payload


def recv_exact(sock: socket.socket, byte_count: int) -> bytes:
    chunks: list[bytes] = []
    remaining = byte_count
    while remaining > 0:
        chunk = sock.recv(remaining)
        if not chunk:
            raise ConnectionError(f"socket closed while waiting for {remaining} more byte(s)")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def recv_packet(sock: socket.socket) -> tuple[int, int, bytes]:
    first = recv_exact(sock, 1)[0]
    if first & 0x80:
        second = recv_exact(sock, 1)[0]
        payload_len = ((first & 0x7F) << 8) | second
        header_len = 2
    else:
        payload_len = first
        header_len = 1
    payload = recv_exact(sock, payload_len)
    return header_len, payload_len, payload


def parse_get_public_key_reply(payload: bytes) -> dict[str, Any]:
    if len(payload) < 14:
        raise ValueError("payload too short for AS_GetPublicKeyReply")
    if payload[0] != 0x07:
        raise ValueError(f"unexpected opcode 0x{payload[0]:02x} for GetPublicKeyReply")
    status, current_time, public_key_id = struct.unpack_from("<III", payload, 1)
    key_size = payload[13]
    tail = payload[14:]
    return {
        "opcode": payload[0],
        "message": auth_opcode_name(payload[0]),
        "status": status,
        "currentTime": current_time,
        "publicKeyId": public_key_id,
        "keySize": key_size,
        "tailHex": tail.hex(),
        "payloadHex": payload.hex(),
    }


def build_get_public_key_request(launcher_version: int, current_public_key_id: int) -> bytes:
    payload = bytes([0x06]) + struct.pack("<II", launcher_version, current_public_key_id)
    return frame_payload(payload)


def parse_md5_hex(text: str | None) -> bytes:
    if not text:
        return bytes(16)
    raw = bytes.fromhex(text)
    if len(raw) != 16:
        raise ValueError("MD5 override must decode to exactly 16 bytes")
    return raw


def build_auth_request_packet(
    public_key_id: int,
    username: str,
    login_type: int,
    key_config_md5: bytes,
    ui_config_md5: bytes,
    embedded_time: int | None,
    twofish_key_hex: str | None,
) -> tuple[bytes, dict[str, Any]]:
    twofish_key = auth_blob.parse_twofish_key(twofish_key_hex)
    blob_time = embedded_time if embedded_time is not None else int(time.time())
    plaintext = auth_blob.build_plaintext_blob(
        username=username,
        embedded_time=blob_time,
        twofish_key=twofish_key,
        rsa_method=4,
        some_short=0x1B,
    )
    ciphertext = auth_blob.encrypt_blob(plaintext)

    payload = bytearray()
    payload.append(0x08)
    payload += struct.pack("<I", public_key_id)
    payload.append(login_type & 0xFF)
    payload += b"\x00\x00"
    payload += key_config_md5
    payload += ui_config_md5
    payload += struct.pack("<H", len(ciphertext))
    payload += ciphertext

    framed = frame_payload(bytes(payload))
    metadata = {
        "publicKeyId": public_key_id,
        "loginType": login_type & 0xFF,
        "payloadLen": len(payload),
        "headerLen": len(framed) - len(payload),
        "byteCount": len(framed),
        "blobLen": len(ciphertext),
        "blobTime": blob_time,
        "twofishKeyHex": twofish_key.hex(),
        "plaintextBlobHex": plaintext.hex(),
        "ciphertextBlobHex": ciphertext.hex(),
    }
    return framed, metadata


def packet_summary(header_len: int, payload_len: int, payload: bytes) -> dict[str, Any]:
    opcode = payload[0] if payload else None
    return {
        "headerLen": header_len,
        "payloadLen": payload_len,
        "byteCount": header_len + payload_len,
        "opcode": opcode,
        "message": auth_opcode_name(opcode) if opcode is not None else "<empty>",
        "payloadHex": payload.hex(),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Small auth-only launcher diagnostic probe")
    parser.add_argument("--host", default="auth.lith.thematrixonline.net")
    parser.add_argument("--port", type=int, default=11000)
    parser.add_argument("--username", required=True)
    parser.add_argument("--password", default="", help="Reserved for later challenge-response work")
    parser.add_argument("--launcher-version", type=lambda s: int(s, 0), default=76005)
    parser.add_argument("--current-public-key-id", type=lambda s: int(s, 0), default=0)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--send-authrequest", action="store_true")
    parser.add_argument("--login-type", type=lambda s: int(s, 0), default=1)
    parser.add_argument("--keyconfig-md5", default=None)
    parser.add_argument("--uiconfig-md5", default=None)
    parser.add_argument("--twofish-key-hex", default=None)
    parser.add_argument("--time", type=int, default=None)
    parser.add_argument("--json", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    results: dict[str, Any] = {
        "host": args.host,
        "port": args.port,
        "launcherVersion": args.launcher_version,
        "currentPublicKeyId": args.current_public_key_id,
        "username": args.username,
    }

    key_config_md5 = parse_md5_hex(args.keyconfig_md5)
    ui_config_md5 = parse_md5_hex(args.uiconfig_md5)

    with socket.create_connection((args.host, args.port), timeout=args.timeout) as sock:
        sock.settimeout(args.timeout)

        get_pub_packet = build_get_public_key_request(args.launcher_version, args.current_public_key_id)
        sock.sendall(get_pub_packet)
        results["sentGetPublicKeyRequest"] = {
            "rawCode": 0x06,
            "message": auth_opcode_name(0x06),
            "byteCount": len(get_pub_packet),
            "payloadLen": len(get_pub_packet) - 1 if get_pub_packet[0] < 0x80 else len(get_pub_packet) - 2,
            "packetHex": get_pub_packet.hex(),
        }

        header_len, payload_len, payload = recv_packet(sock)
        results["receivedAfterGetPublicKey"] = packet_summary(header_len, payload_len, payload)
        if payload and payload[0] == 0x07:
            results["parsedGetPublicKeyReply"] = parse_get_public_key_reply(payload)

        if args.send_authrequest:
            parsed_reply = results.get("parsedGetPublicKeyReply")
            if not parsed_reply:
                raise RuntimeError("cannot build AS_AuthRequest without a parsed AS_GetPublicKeyReply")

            auth_packet, metadata = build_auth_request_packet(
                public_key_id=int(parsed_reply["publicKeyId"]),
                username=args.username,
                login_type=args.login_type,
                key_config_md5=key_config_md5,
                ui_config_md5=ui_config_md5,
                embedded_time=args.time,
                twofish_key_hex=args.twofish_key_hex,
            )
            sock.sendall(auth_packet)
            results["sentAuthRequest"] = {
                "rawCode": 0x08,
                "message": auth_opcode_name(0x08),
                **metadata,
            }

            try:
                header_len2, payload_len2, payload2 = recv_packet(sock)
                results["receivedAfterAuthRequest"] = packet_summary(header_len2, payload_len2, payload2)
            except socket.timeout:
                results["receivedAfterAuthRequest"] = {
                    "timeout": True,
                    "message": "No reply within timeout after AS_AuthRequest",
                }

    if args.json:
        json.dump(results, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        print(json.dumps(results, indent=2))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
