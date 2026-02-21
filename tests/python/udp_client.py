"""UDP helpers for the SIL test suite.

Wire format (identical to C++ side):
  [uint16_t msgId LE][payload bytes]
"""

from __future__ import annotations

import socket
import struct

from reflect_pytest.generated import (
    MESSAGE_BY_ID,
    PAYLOAD_SIZE_BY_ID,
    MsgId,
)


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
        self._sock.settimeout(1.0)

    def register(self) -> None:
        """Send an empty packet so the bridge records our address."""
        self._sock.sendto(b"", self._bridge)

    def send_msg(self, msg_id: MsgId, payload) -> None:
        payload_bytes = payload.pack_wire()
        msg_id_bytes = struct.pack("<H", int(msg_id))
        self._sock.sendto(msg_id_bytes + payload_bytes, self._bridge)
        print(f"[UDP TX] {msg_id.name}: {payload}")

    def recv_msg(self, expected_id: MsgId = None):
        """Block until matching Message arrives (or timeout raises)."""
        while True:
            data, _ = self._sock.recvfrom(4096)
            if len(data) < 2:
                continue

            msg_id = struct.unpack_from("<H", data)[0]
            try:
                msg_enum = MsgId(msg_id)
            except ValueError:
                continue  # Unknown message

            if expected_id is not None and msg_enum != expected_id:
                continue

            # Ensure we have the correct amount of data for the payload
            expected_size = PAYLOAD_SIZE_BY_ID.get(msg_enum)
            if expected_size is None or len(data) - 2 != expected_size:
                continue

            try:
                # We strip the 2-byte msgId header to unpack the payload
                payload_class = MESSAGE_BY_ID[msg_enum]
                payload_obj = payload_class.unpack_wire(data[2:])
                print(f"[UDP RX] {msg_enum.name}: {payload_obj}")
                return payload_obj
            except struct.error:
                continue

    def close(self) -> None:
        self._sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
