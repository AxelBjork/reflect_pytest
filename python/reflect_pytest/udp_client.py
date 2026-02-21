"""UDP helpers for the SIL test suite.

Wire format (identical to C++ side):
  [uint16_t msgId LE][payload bytes]
"""

from __future__ import annotations

import socket
import struct

from reflect_pytest.generated import LogPayload, MsgId


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
        payload_bytes = log.pack_wire()
        msg_id_bytes = struct.pack("<H", int(MsgId.Log))
        self._sock.sendto(msg_id_bytes + payload_bytes, self._bridge)

    def recv_log(self) -> LogPayload:
        """Block until a LogMessage arrives (or timeout raises timeout)."""
        while True:
            data, _ = self._sock.recvfrom(4096)
            if len(data) < 2:
                continue
            msg_id = struct.unpack_from("<H", data)[0]
            if msg_id != int(MsgId.Log):
                continue
            try:
                # We strip the 2-byte msgId header to unpack the payload
                return LogPayload.unpack_wire(data[2:])
            except struct.error:
                continue

    def close(self) -> None:
        self._sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
