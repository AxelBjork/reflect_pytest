"""UDP helpers for the SIL test suite.

Wire format (identical to C++ side):
  [uint16_t msgId LE][payload bytes]

LogPayload layout (257 bytes, host byte-order):
  char[255]  text       — null-terminated message
  uint8_t    severity   — 0=Debug 1=Info 2=Warn 3=Error
  uint8_t    component  — 0=Main 1=Bus 2=Logger 3=Bridge 4=Test
"""

from __future__ import annotations

import socket
import struct
from dataclasses import dataclass
from enum import IntEnum

# ── Enums (must match ipc/messages.hpp) ───────────────────────────────────────

class MsgId(IntEnum):
    MotorCmd   = 0x0001
    SensorData = 0x0002
    Log        = 0x0003


class Severity(IntEnum):
    Debug = 0
    Info  = 1
    Warn  = 2
    Error = 3


class ComponentId(IntEnum):
    Main   = 0
    Bus    = 1
    Logger = 2
    Bridge = 3
    Test   = 4


# ── LogPayload ─────────────────────────────────────────────────────────────────

# struct format: little-endian, 255-byte text + 2 × uint8
_LOG_STRUCT = struct.Struct("<255sBB")
assert _LOG_STRUCT.size == 257, "LogPayload size mismatch"

# Wire packet = msgId (uint16 LE) + payload
_WIRE_LOG_STRUCT = struct.Struct("<H255sBB")
assert _WIRE_LOG_STRUCT.size == 259


@dataclass
class LogPayload:
    text:      str
    severity:  Severity   = Severity.Info
    component: ComponentId = ComponentId.Test

    def pack_wire(self) -> bytes:
        """Serialise to the complete wire format (msgId + payload)."""
        text_bytes = self.text.encode()[:254].ljust(255, b"\x00")
        return _WIRE_LOG_STRUCT.pack(
            int(MsgId.Log),
            text_bytes,
            int(self.severity),
            int(self.component),
        )

    @staticmethod
    def unpack_wire(data: bytes) -> "LogPayload":
        """Deserialise from raw wire bytes (msgId + payload)."""
        if len(data) != _WIRE_LOG_STRUCT.size:
            raise ValueError(f"Expected {_WIRE_LOG_STRUCT.size} bytes, got {len(data)}")
        msg_id, text_bytes, severity, component = _WIRE_LOG_STRUCT.unpack(data)
        text = text_bytes.rstrip(b"\x00").decode(errors="replace")
        return LogPayload(
            text=text,
            severity=Severity(severity),
            component=ComponentId(component),
        )


# ── UdpClient ─────────────────────────────────────────────────────────────────

class UdpClient:
    """Simple UDP socket that talks to the C++ UDP bridge."""

    BRIDGE_PORT = 9000  # bridge listens here
    CLIENT_PORT = 9001  # we bind here so bridge knows where to send back

    def __init__(self,
                 bridge_host: str = "127.0.0.1",
                 bridge_port: int = BRIDGE_PORT,
                 client_port: int = CLIENT_PORT):
        self._bridge = (bridge_host, bridge_port)
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.bind(("", client_port))
        self._sock.settimeout(5.0)

    def register(self) -> None:
        """Send an empty packet so the bridge records our address."""
        self._sock.sendto(b"", self._bridge)

    def send_log(self, log: LogPayload) -> None:
        self._sock.sendto(log.pack_wire(), self._bridge)

    def recv_log(self) -> LogPayload:
        """Block until a LogMessage arrives (or timeout raises socket.timeout)."""
        while True:
            data, _ = self._sock.recvfrom(4096)
            if len(data) != _WIRE_LOG_STRUCT.size:
                continue  # not a LogPayload, skip
            msg_id = struct.unpack_from("<H", data)[0]
            if msg_id != int(MsgId.Log):
                continue
            return LogPayload.unpack_wire(data)

    def close(self) -> None:
        self._sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
