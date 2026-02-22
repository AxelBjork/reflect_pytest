"""Auto-generated IPC bindings using C++26 static reflection."""

import struct
from dataclasses import dataclass
from enum import IntEnum

class MsgId(IntEnum):
    """Top-level message type selector. The uint16_t wire value is the first two bytes of every UDP datagram."""
    Log = 0
    QueryState = 1
    MotorSequence = 2
    KinematicsRequest = 3
    KinematicsData = 4
    PowerRequest = 5
    PowerData = 6
    PhysicsTick = 7
    StateChange = 8

class Severity(IntEnum):
    """Severity level attached to every LogPayload message."""
    Debug = 0
    Info = 1
    Warn = 2
    Error = 3

class ComponentId(IntEnum):
    """Identifies the subsystem that emitted a LogPayload."""
    Main = 0
    Bus = 1
    Logger = 2
    Bridge = 3
    Test = 4
    Simulator = 5

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
        return struct.pack("<hI", self.speed_rpm, self.duration_us)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "MotorSubCmd":
        unpacked = struct.unpack("<hI", data)
        return cls(*unpacked)

@dataclass
class LogPayload:
    """Unidirectional log/trace message. Emitted by any component at any time; Python receives these passively from the bus."""
    text: bytes
    severity: Severity
    component: ComponentId

    def pack_wire(self) -> bytes:
        return struct.pack("<255sBB", self.text, self.severity, self.component)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "LogPayload":
        unpacked = struct.unpack("<255sBB", data)
        return cls(*unpacked)

@dataclass
class QueryStatePayload:
    """Carries the current SystemState. Python sends this as a request (with any state value); the simulator responds with the actual state."""
    state: SystemState

    def pack_wire(self) -> bytes:
        return struct.pack("<B", self.state)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "QueryStatePayload":
        unpacked = struct.unpack("<B", data)
        return cls(*unpacked)

@dataclass
class MotorSequencePayload:
    """Deliver a sequence of up to 10 timed motor sub-commands to the simulator. The simulator executes steps[0..num_steps-1] in real time; a new command preempts any currently running sequence."""
    cmd_id: int
    num_steps: int
    steps: bytes

    def pack_wire(self) -> bytes:
        return struct.pack("<IB60s", self.cmd_id, self.num_steps, self.steps)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "MotorSequencePayload":
        unpacked = struct.unpack("<IB60s", data)
        return cls(*unpacked)

@dataclass
class KinematicsRequestPayload:
    """One-byte sentinel. Send to request a KinematicsData snapshot. The payload value is ignored."""
    reserved: int

    def pack_wire(self) -> bytes:
        return struct.pack("<B", self.reserved)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "KinematicsRequestPayload":
        unpacked = struct.unpack("<B", data)
        return cls(*unpacked)

@dataclass
class KinematicsPayload:
    """Kinematics snapshot sent in response to a KinematicsRequest. Reflects physics state integrated since the start of the current sequence."""
    cmd_id: int
    elapsed_us: int
    position_m: float
    speed_mps: float

    def pack_wire(self) -> bytes:
        return struct.pack("<IIff", self.cmd_id, self.elapsed_us, self.position_m, self.speed_mps)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "KinematicsPayload":
        unpacked = struct.unpack("<IIff", data)
        return cls(*unpacked)

@dataclass
class PowerRequestPayload:
    """One-byte sentinel. Send to request a PowerData snapshot. The payload value is ignored."""
    reserved: int

    def pack_wire(self) -> bytes:
        return struct.pack("<B", self.reserved)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "PowerRequestPayload":
        unpacked = struct.unpack("<B", data)
        return cls(*unpacked)

@dataclass
class PowerPayload:
    """Power-model snapshot sent in response to a PowerRequest. Models a simple battery with internal resistance drain."""
    cmd_id: int
    voltage_v: float
    current_a: float
    state_of_charge: int

    def pack_wire(self) -> bytes:
        return struct.pack("<IffB", self.cmd_id, self.voltage_v, self.current_a, self.state_of_charge)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "PowerPayload":
        unpacked = struct.unpack("<IffB", data)
        return cls(*unpacked)

@dataclass
class PhysicsTickPayload:
    """Internal IPC: Broadcast at 100Hz during sequence execution to drive kinematics and power integration."""
    cmd_id: int
    speed_rpm: int
    dt_us: int

    def pack_wire(self) -> bytes:
        return struct.pack("<IhI", self.cmd_id, self.speed_rpm, self.dt_us)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "PhysicsTickPayload":
        unpacked = struct.unpack("<IhI", data)
        return cls(*unpacked)

@dataclass
class StateChangePayload:
    """Internal IPC: Broadcast when moving into or out of Executing state."""
    state: SystemState
    cmd_id: int

    def pack_wire(self) -> bytes:
        return struct.pack("<BI", self.state, self.cmd_id)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "StateChangePayload":
        unpacked = struct.unpack("<BI", data)
        return cls(*unpacked)

MESSAGE_BY_ID = {
    MsgId.Log: LogPayload,
    MsgId.QueryState: QueryStatePayload,
    MsgId.MotorSequence: MotorSequencePayload,
    MsgId.KinematicsRequest: KinematicsRequestPayload,
    MsgId.KinematicsData: KinematicsPayload,
    MsgId.PowerRequest: PowerRequestPayload,
    MsgId.PowerData: PowerPayload,
    MsgId.PhysicsTick: PhysicsTickPayload,
    MsgId.StateChange: StateChangePayload,
}

PAYLOAD_SIZE_BY_ID = {
    MsgId.Log: 257,
    MsgId.QueryState: 1,
    MsgId.MotorSequence: 65,
    MsgId.KinematicsRequest: 1,
    MsgId.KinematicsData: 16,
    MsgId.PowerRequest: 1,
    MsgId.PowerData: 13,
    MsgId.PhysicsTick: 10,
    MsgId.StateChange: 5,
}

