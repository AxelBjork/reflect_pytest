"""Auto-generated IPC bindings using C++26 static reflection."""

import struct
from dataclasses import dataclass
from enum import IntEnum

class MsgId(IntEnum):
    """Top-level message type selector. The uint16_t wire value is the first two bytes of every UDP datagram."""
    Log = 0
    StateRequest = 1
    StateData = 2
    MotorSequence = 3
    KinematicsRequest = 4
    KinematicsData = 5
    PowerRequest = 6
    PowerData = 7
    PhysicsTick = 8
    StateChange = 9
    ResetRequest = 10

class Severity(IntEnum):
    """Severity level attached to every LogPayload message."""
    Debug = 0
    Info = 1
    Warn = 2
    Error = 3

class SystemState(IntEnum):
    """Coarse lifecycle state of the SIL simulator."""
    Init = 0
    Ready = 1
    Executing = 2
    Fault = 3

@dataclass
class MotorSubCmd:
    """One timed motor command step, embedded in MotorSequencePayload."""
    speed_rpm: int
    duration_us: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<hI", self.speed_rpm, self.duration_us))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "MotorSubCmd":
        offset = 0
        speed_rpm, duration_us = struct.unpack_from("<hI", data, offset)
        offset += struct.calcsize("<hI")
        return cls(speed_rpm=speed_rpm, duration_us=duration_us)

@dataclass
class LogPayload:
    """Unidirectional log/trace message. Emitted by any component at any time; Python receives these passively from the bus."""
    text: bytes
    severity: Severity
    component: bytes

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<255sB32s", self.text, self.severity, self.component))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "LogPayload":
        offset = 0
        text, severity, component = struct.unpack_from("<255sB32s", data, offset)
        offset += struct.calcsize("<255sB32s")
        return cls(text=text, severity=severity, component=component)

@dataclass
class StateRequestPayload:
    """One-byte sentinel. Send to request a StateData snapshot. The payload value is ignored."""
    reserved: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<B", self.reserved))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "StateRequestPayload":
        offset = 0
        reserved = struct.unpack_from("<B", data, offset)[0]
        offset += struct.calcsize("<B")
        return cls(reserved=reserved)

@dataclass
class StatePayload:
    """State machine snapshot sent in response to a StateRequest. Carries the current coarse lifecycle SystemState."""
    state: SystemState

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<B", self.state))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "StatePayload":
        offset = 0
        state = struct.unpack_from("<B", data, offset)[0]
        offset += struct.calcsize("<B")
        return cls(state=state)

@dataclass
class MotorSequencePayloadTemplate_5:
    """Deliver a sequence of up to 10 timed motor sub-commands to the simulator. The simulator executes steps[0..num_steps-1] in real time; a new command preempts any currently running sequence."""
    cmd_id: int
    num_steps: int
    steps: list[MotorSubCmd]

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<IB", self.cmd_id, self.num_steps))
        for item in self.steps:
            if not hasattr(item, 'pack_wire'):
                if isinstance(item, tuple):
                    item = MotorSubCmd(*item)
                elif isinstance(item, dict):
                    item = MotorSubCmd(**item)
                else:
                    item = MotorSubCmd(item)
            data.extend(item.pack_wire())
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "MotorSequencePayloadTemplate_5":
        offset = 0
        cmd_id, num_steps = struct.unpack_from("<IB", data, offset)
        offset += struct.calcsize("<IB")
        steps = []
        for _ in range(5):
            sub_size = len(MotorSubCmd().pack_wire()) if hasattr(MotorSubCmd, 'pack_wire') else struct.calcsize(MotorSubCmd.SUBCMD_FMT)
            item = MotorSubCmd.unpack_wire(data[offset:offset+sub_size])
            steps.append(item)
            offset += sub_size
        return cls(cmd_id=cmd_id, num_steps=num_steps, steps=steps)

@dataclass
class KinematicsRequestPayload:
    """One-byte sentinel. Send to request a KinematicsData snapshot. The payload value is ignored."""
    reserved: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<B", self.reserved))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "KinematicsRequestPayload":
        offset = 0
        reserved = struct.unpack_from("<B", data, offset)[0]
        offset += struct.calcsize("<B")
        return cls(reserved=reserved)

@dataclass
class KinematicsPayload:
    """Kinematics snapshot sent in response to a KinematicsRequest. Reflects physics state integrated since the start of the current sequence."""
    cmd_id: int
    elapsed_us: int
    position_m: float
    speed_mps: float

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<IIff", self.cmd_id, self.elapsed_us, self.position_m, self.speed_mps))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "KinematicsPayload":
        offset = 0
        cmd_id, elapsed_us, position_m, speed_mps = struct.unpack_from("<IIff", data, offset)
        offset += struct.calcsize("<IIff")
        return cls(cmd_id=cmd_id, elapsed_us=elapsed_us, position_m=position_m, speed_mps=speed_mps)

@dataclass
class PowerRequestPayload:
    """One-byte sentinel. Send to request a PowerData snapshot. The payload value is ignored."""
    reserved: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<B", self.reserved))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "PowerRequestPayload":
        offset = 0
        reserved = struct.unpack_from("<B", data, offset)[0]
        offset += struct.calcsize("<B")
        return cls(reserved=reserved)

@dataclass
class PowerPayload:
    """Power-model snapshot sent in response to a PowerRequest. Models a simple battery with internal resistance drain."""
    cmd_id: int
    voltage_v: float
    current_a: float
    state_of_charge: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<IffB", self.cmd_id, self.voltage_v, self.current_a, self.state_of_charge))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "PowerPayload":
        offset = 0
        cmd_id, voltage_v, current_a, state_of_charge = struct.unpack_from("<IffB", data, offset)
        offset += struct.calcsize("<IffB")
        return cls(cmd_id=cmd_id, voltage_v=voltage_v, current_a=current_a, state_of_charge=state_of_charge)

@dataclass
class PhysicsTickPayload:
    """Internal IPC: Broadcast at 100Hz during sequence execution to drive kinematics and power integration."""
    cmd_id: int
    speed_rpm: int
    dt_us: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<IhI", self.cmd_id, self.speed_rpm, self.dt_us))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "PhysicsTickPayload":
        offset = 0
        cmd_id, speed_rpm, dt_us = struct.unpack_from("<IhI", data, offset)
        offset += struct.calcsize("<IhI")
        return cls(cmd_id=cmd_id, speed_rpm=speed_rpm, dt_us=dt_us)

@dataclass
class StateChangePayload:
    """Internal IPC: Broadcast when moving into or out of Executing state."""
    state: SystemState
    cmd_id: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<BI", self.state, self.cmd_id))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "StateChangePayload":
        offset = 0
        state, cmd_id = struct.unpack_from("<BI", data, offset)
        offset += struct.calcsize("<BI")
        return cls(state=state, cmd_id=cmd_id)

@dataclass
class ResetRequestPayload:
    """One-byte sentinel. Send to request a full physics reset."""
    reserved: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<B", self.reserved))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "ResetRequestPayload":
        offset = 0
        reserved = struct.unpack_from("<B", data, offset)[0]
        offset += struct.calcsize("<B")
        return cls(reserved=reserved)

MESSAGE_BY_ID = {
    MsgId.Log: LogPayload,
    MsgId.StateRequest: StateRequestPayload,
    MsgId.StateData: StatePayload,
    MsgId.MotorSequence: MotorSequencePayloadTemplate_5,
    MsgId.KinematicsRequest: KinematicsRequestPayload,
    MsgId.KinematicsData: KinematicsPayload,
    MsgId.PowerRequest: PowerRequestPayload,
    MsgId.PowerData: PowerPayload,
    MsgId.PhysicsTick: PhysicsTickPayload,
    MsgId.StateChange: StateChangePayload,
    MsgId.ResetRequest: ResetRequestPayload,
}

PAYLOAD_SIZE_BY_ID = {
    MsgId.Log: 288,
    MsgId.StateRequest: 1,
    MsgId.StateData: 1,
    MsgId.MotorSequence: 35,
    MsgId.KinematicsRequest: 1,
    MsgId.KinematicsData: 16,
    MsgId.PowerRequest: 1,
    MsgId.PowerData: 13,
    MsgId.PhysicsTick: 10,
    MsgId.StateChange: 5,
    MsgId.ResetRequest: 1,
}

