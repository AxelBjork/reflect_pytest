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
    ThermalRequest = 8
    ThermalData = 9
    EnvironmentAck = 10
    EnvironmentRequest = 11
    EnvironmentData = 12
    AutoDriveCommand = 13
    AutoDriveStatus = 14
    MotorStatus = 15
    PhysicsTick = 16
    ResetRequest = 17
    InternalEnvRequest = 18
    InternalEnvData = 19

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

class DriveMode(IntEnum):
    """Control strategy for the autonomous service."""
    Eco = 0
    Performance = 1
    ManualTuning = 2

@dataclass
class MotorSubCmd:
    """One timed motor command step, embedded in MotorSequencePayload."""
    WIRE_SIZE = 6
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
    WIRE_SIZE = 288
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
    WIRE_SIZE = 1
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
    """State machine snapshot. Carries the current coarse lifecycle SystemState."""
    WIRE_SIZE = 1
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
    WIRE_SIZE = 35
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
            sub_size = MotorSubCmd.WIRE_SIZE
            item = MotorSubCmd.unpack_wire(data[offset:offset+sub_size])
            steps.append(item)
            offset += sub_size
        return cls(cmd_id=cmd_id, num_steps=num_steps, steps=steps)

@dataclass
class KinematicsRequestPayload:
    """One-byte sentinel. Send to request a KinematicsData snapshot. The payload value is ignored."""
    WIRE_SIZE = 1
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
    WIRE_SIZE = 16
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
    WIRE_SIZE = 1
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
    WIRE_SIZE = 13
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
class ThermalRequestPayload:
    """One-byte sentinel. Send to request a ThermalData snapshot. The payload value is ignored."""
    WIRE_SIZE = 1
    reserved: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<B", self.reserved))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "ThermalRequestPayload":
        offset = 0
        reserved = struct.unpack_from("<B", data, offset)[0]
        offset += struct.calcsize("<B")
        return cls(reserved=reserved)

@dataclass
class ThermalPayload:
    """Thermal snapshot sent in response to a ThermalRequest. Models temperature of motor and battery based on power metrics."""
    WIRE_SIZE = 8
    motor_temp_c: float
    battery_temp_c: float

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<ff", self.motor_temp_c, self.battery_temp_c))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "ThermalPayload":
        offset = 0
        motor_temp_c, battery_temp_c = struct.unpack_from("<ff", data, offset)
        offset += struct.calcsize("<ff")
        return cls(motor_temp_c=motor_temp_c, battery_temp_c=battery_temp_c)

@dataclass
class EnvironmentAckPayload:
    """ACK sent by the application when it accepts new environment data."""
    WIRE_SIZE = 4
    region_id: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<I", self.region_id))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "EnvironmentAckPayload":
        offset = 0
        region_id = struct.unpack_from("<I", data, offset)[0]
        offset += struct.calcsize("<I")
        return cls(region_id=region_id)

@dataclass
class Point2D:
    """A 2D coordinate."""
    WIRE_SIZE = 8
    x: float
    y: float

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<ff", self.x, self.y))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "Point2D":
        offset = 0
        x, y = struct.unpack_from("<ff", data, offset)
        offset += struct.calcsize("<ff")
        return cls(x=x, y=y)

@dataclass
class EnvironmentRequestPayload:
    """Request conditions for a specific location."""
    WIRE_SIZE = 8
    target_location: Point2D

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(self.target_location.pack_wire())
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "EnvironmentRequestPayload":
        offset = 0
        sub_size = Point2D.WIRE_SIZE
        target_location = Point2D.unpack_wire(data[offset:offset+sub_size])
        offset += sub_size
        return cls(target_location=target_location)

@dataclass
class BoundingBox2D:
    """An axis-aligned 2D bounding box."""
    WIRE_SIZE = 16
    min_pt: Point2D
    max_pt: Point2D

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(self.min_pt.pack_wire())
        data.extend(self.max_pt.pack_wire())
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "BoundingBox2D":
        offset = 0
        sub_size = Point2D.WIRE_SIZE
        min_pt = Point2D.unpack_wire(data[offset:offset+sub_size])
        offset += sub_size
        sub_size = Point2D.WIRE_SIZE
        max_pt = Point2D.unpack_wire(data[offset:offset+sub_size])
        offset += sub_size
        return cls(min_pt=min_pt, max_pt=max_pt)

@dataclass
class EnvironmentPayload:
    """Environmental conditions delivered to the application from the outside world."""
    WIRE_SIZE = 32
    region_id: int
    bounds: BoundingBox2D
    ambient_temp_c: float
    incline_percent: float
    surface_friction: float

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<I", self.region_id))
        data.extend(self.bounds.pack_wire())
        data.extend(struct.pack("<fff", self.ambient_temp_c, self.incline_percent, self.surface_friction))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "EnvironmentPayload":
        offset = 0
        region_id = struct.unpack_from("<I", data, offset)[0]
        offset += struct.calcsize("<I")
        sub_size = BoundingBox2D.WIRE_SIZE
        bounds = BoundingBox2D.unpack_wire(data[offset:offset+sub_size])
        offset += sub_size
        ambient_temp_c, incline_percent, surface_friction = struct.unpack_from("<fff", data, offset)
        offset += struct.calcsize("<fff")
        return cls(region_id=region_id, bounds=bounds, ambient_temp_c=ambient_temp_c, incline_percent=incline_percent, surface_friction=surface_friction)

@dataclass
class Vector3:
    """A 3D coordinate vector."""
    WIRE_SIZE = 12
    x: float
    y: float
    z: float

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<fff", self.x, self.y, self.z))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "Vector3":
        offset = 0
        x, y, z = struct.unpack_from("<fff", data, offset)
        offset += struct.calcsize("<fff")
        return cls(x=x, y=y, z=z)

@dataclass
class ManeuverNode:
    """A single target maneuver point."""
    WIRE_SIZE = 12
    target_pos: Point2D
    speed_limit_rpm: int
    timeout_ms: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(self.target_pos.pack_wire())
        data.extend(struct.pack("<hH", self.speed_limit_rpm, self.timeout_ms))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "ManeuverNode":
        offset = 0
        sub_size = Point2D.WIRE_SIZE
        target_pos = Point2D.unpack_wire(data[offset:offset+sub_size])
        offset += sub_size
        speed_limit_rpm, timeout_ms = struct.unpack_from("<hH", data, offset)
        offset += struct.calcsize("<hH")
        return cls(target_pos=target_pos, speed_limit_rpm=speed_limit_rpm, timeout_ms=timeout_ms)

@dataclass
class AutoDriveCommandTemplate_8:
    """High level autonomous driving route and configuration."""
    WIRE_SIZE = 171
    route_name: bytes
    mode: DriveMode
    p_gain: float
    use_environment_tuning: bool
    route_transform: list[Vector3]
    num_nodes: int
    route: list[ManeuverNode]

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<32sBf?", self.route_name, self.mode, self.p_gain, self.use_environment_tuning))
        for item in self.route_transform:
            if not hasattr(item, 'pack_wire'):
                if isinstance(item, tuple):
                    item = Vector3(*item)
                elif isinstance(item, dict):
                    item = Vector3(**item)
                else:
                    item = Vector3(item)
            data.extend(item.pack_wire())
        data.extend(struct.pack("<B", self.num_nodes))
        for item in self.route:
            if not hasattr(item, 'pack_wire'):
                if isinstance(item, tuple):
                    item = ManeuverNode(*item)
                elif isinstance(item, dict):
                    item = ManeuverNode(**item)
                else:
                    item = ManeuverNode(item)
            data.extend(item.pack_wire())
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "AutoDriveCommandTemplate_8":
        offset = 0
        route_name, mode, p_gain, use_environment_tuning = struct.unpack_from("<32sBf?", data, offset)
        offset += struct.calcsize("<32sBf?")
        route_transform = []
        for _ in range(3):
            sub_size = Vector3.WIRE_SIZE
            item = Vector3.unpack_wire(data[offset:offset+sub_size])
            route_transform.append(item)
            offset += sub_size
        num_nodes = struct.unpack_from("<B", data, offset)[0]
        offset += struct.calcsize("<B")
        route = []
        for _ in range(8):
            sub_size = ManeuverNode.WIRE_SIZE
            item = ManeuverNode.unpack_wire(data[offset:offset+sub_size])
            route.append(item)
            offset += sub_size
        return cls(route_name=route_name, mode=mode, p_gain=p_gain, use_environment_tuning=use_environment_tuning, route_transform=route_transform, num_nodes=num_nodes, route=route)

@dataclass
class ManeuverStats:
    """Efficiency metrics for a single traveled node."""
    WIRE_SIZE = 16
    initial_energy_mj: float
    final_energy_mj: float
    energy_per_meter_mj: float
    total_energy_used_mj: float

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<ffff", self.initial_energy_mj, self.final_energy_mj, self.energy_per_meter_mj, self.total_energy_used_mj))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "ManeuverStats":
        offset = 0
        initial_energy_mj, final_energy_mj, energy_per_meter_mj, total_energy_used_mj = struct.unpack_from("<ffff", data, offset)
        offset += struct.calcsize("<ffff")
        return cls(initial_energy_mj=initial_energy_mj, final_energy_mj=final_energy_mj, energy_per_meter_mj=energy_per_meter_mj, total_energy_used_mj=total_energy_used_mj)

@dataclass
class EnvId:
    """Environment ID Wrapper."""
    WIRE_SIZE = 4
    id: int

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<I", self.id))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "EnvId":
        offset = 0
        id = struct.unpack_from("<I", data, offset)[0]
        offset += struct.calcsize("<I")
        return cls(id=id)

@dataclass
class AutoDriveStatusTemplate_8__4:
    """Status and efficiency report from the AutonomousService."""
    WIRE_SIZE = 152
    cmd_id: int
    current_node_idx: int
    route_complete: bool
    num_stats: int
    node_stats: list[ManeuverStats]
    num_environments_used: int
    environment_ids: list[EnvId]

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<IB?B", self.cmd_id, self.current_node_idx, self.route_complete, self.num_stats))
        for item in self.node_stats:
            if not hasattr(item, 'pack_wire'):
                if isinstance(item, tuple):
                    item = ManeuverStats(*item)
                elif isinstance(item, dict):
                    item = ManeuverStats(**item)
                else:
                    item = ManeuverStats(item)
            data.extend(item.pack_wire())
        data.extend(struct.pack("<B", self.num_environments_used))
        for item in self.environment_ids:
            if not hasattr(item, 'pack_wire'):
                if isinstance(item, tuple):
                    item = EnvId(*item)
                elif isinstance(item, dict):
                    item = EnvId(**item)
                else:
                    item = EnvId(item)
            data.extend(item.pack_wire())
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "AutoDriveStatusTemplate_8__4":
        offset = 0
        cmd_id, current_node_idx, route_complete, num_stats = struct.unpack_from("<IB?B", data, offset)
        offset += struct.calcsize("<IB?B")
        node_stats = []
        for _ in range(8):
            sub_size = ManeuverStats.WIRE_SIZE
            item = ManeuverStats.unpack_wire(data[offset:offset+sub_size])
            node_stats.append(item)
            offset += sub_size
        num_environments_used = struct.unpack_from("<B", data, offset)[0]
        offset += struct.calcsize("<B")
        environment_ids = []
        for _ in range(4):
            sub_size = EnvId.WIRE_SIZE
            item = EnvId.unpack_wire(data[offset:offset+sub_size])
            environment_ids.append(item)
            offset += sub_size
        return cls(cmd_id=cmd_id, current_node_idx=current_node_idx, route_complete=route_complete, num_stats=num_stats, node_stats=node_stats, num_environments_used=num_environments_used, environment_ids=environment_ids)

@dataclass
class MotorStatusPayload:
    """Internal IPC: Periodic RPM and activity update from MotorService."""
    WIRE_SIZE = 7
    cmd_id: int
    speed_rpm: int
    is_active: bool

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<Ih?", self.cmd_id, self.speed_rpm, self.is_active))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "MotorStatusPayload":
        offset = 0
        cmd_id, speed_rpm, is_active = struct.unpack_from("<Ih?", data, offset)
        offset += struct.calcsize("<Ih?")
        return cls(cmd_id=cmd_id, speed_rpm=speed_rpm, is_active=is_active)

@dataclass
class PhysicsTickPayload:
    """Internal IPC: Broadcast at 100Hz during sequence execution to drive kinematics and power integration."""
    WIRE_SIZE = 10
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
class ResetRequestPayload:
    """One-byte sentinel. Send to request a full physics reset."""
    WIRE_SIZE = 1
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

@dataclass
class InternalEnvRequestPayload:
    WIRE_SIZE = 8
    x: float
    y: float

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(struct.pack("<ff", self.x, self.y))
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "InternalEnvRequestPayload":
        offset = 0
        x, y = struct.unpack_from("<ff", data, offset)
        offset += struct.calcsize("<ff")
        return cls(x=x, y=y)

@dataclass
class std__shared_ptr_const_EnvironmentPayload:
    WIRE_SIZE = 16
    pass

    def pack_wire(self) -> bytes:
        return b""

    @classmethod
    def unpack_wire(cls, data: bytes) -> "std__shared_ptr_const_EnvironmentPayload":
        return cls()

@dataclass
class InternalEnvDataPayload:
    WIRE_SIZE = 16
    ptr: std__shared_ptr_const_EnvironmentPayload

    def pack_wire(self) -> bytes:
        data = bytearray()
        data.extend(self.ptr.pack_wire())
        return bytes(data)

    @classmethod
    def unpack_wire(cls, data: bytes) -> "InternalEnvDataPayload":
        offset = 0
        sub_size = std__shared_ptr_const_EnvironmentPayload.WIRE_SIZE
        ptr = std__shared_ptr_const_EnvironmentPayload.unpack_wire(data[offset:offset+sub_size])
        offset += sub_size
        return cls(ptr=ptr)

MESSAGE_BY_ID = {
    MsgId.Log: LogPayload,
    MsgId.StateRequest: StateRequestPayload,
    MsgId.StateData: StatePayload,
    MsgId.MotorSequence: MotorSequencePayloadTemplate_5,
    MsgId.KinematicsRequest: KinematicsRequestPayload,
    MsgId.KinematicsData: KinematicsPayload,
    MsgId.PowerRequest: PowerRequestPayload,
    MsgId.PowerData: PowerPayload,
    MsgId.ThermalRequest: ThermalRequestPayload,
    MsgId.ThermalData: ThermalPayload,
    MsgId.EnvironmentAck: EnvironmentAckPayload,
    MsgId.EnvironmentRequest: EnvironmentRequestPayload,
    MsgId.EnvironmentData: EnvironmentPayload,
    MsgId.AutoDriveCommand: AutoDriveCommandTemplate_8,
    MsgId.AutoDriveStatus: AutoDriveStatusTemplate_8__4,
    MsgId.MotorStatus: MotorStatusPayload,
    MsgId.PhysicsTick: PhysicsTickPayload,
    MsgId.ResetRequest: ResetRequestPayload,
    MsgId.InternalEnvRequest: InternalEnvRequestPayload,
    MsgId.InternalEnvData: InternalEnvDataPayload,
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
    MsgId.ThermalRequest: 1,
    MsgId.ThermalData: 8,
    MsgId.EnvironmentAck: 4,
    MsgId.EnvironmentRequest: 8,
    MsgId.EnvironmentData: 32,
    MsgId.AutoDriveCommand: 171,
    MsgId.AutoDriveStatus: 152,
    MsgId.MotorStatus: 7,
    MsgId.PhysicsTick: 10,
    MsgId.ResetRequest: 1,
    MsgId.InternalEnvRequest: 8,
    MsgId.InternalEnvData: 16,
}

