import can
import struct
# Library API: 
# https://python-can.readthedocs.io/en/stable/bus.html#
# https://docs.python.org/3/library/struct.html

# Initialize bus
bus = can.interface.Bus(channel="can1", interface="socketcan")
print("Exoskeleton Telemetry Started...")

# Sensor data payload structure
telemetry = {
    "voltage": 0.0,
    "current": 0.0,
    "closed": False,
    "sensor_healthy": False,
    "fault": False
}

# INA228 sensor locations
sensor = {
    0x100: "BUS", 
    0x101: "M1", 
    0x102: "M2", 
    0x103: "M3", 
    0x104: "M4"
}

for msg in bus:
    if msg.arbitration_id in sensor:
        # Unpack 7 bytes: 2 int16s + 3 bools
        # '<hh???' = little-endian, 2x signed short, 3x bool
        try:
            
            v_raw, c_raw, closed, healthy, fault = struct.unpack('<hh???', msg.data[0:7])

            # Divide by 100 because we multiplied by 100 in main.c
            telemetry["voltage"]        = v_raw / 100.0 
            telemetry["current"]        = c_raw / 100.0
            telemetry["closed"]         = closed
            telemetry["sensor_healthy"] = healthy
            telemetry["fault"]          = fault

        except Exception as e:
            print(f"Error in retrieving data: {e}")
    
    # Map map CAN ID to sensor (default is ??? if not found)
    label = sensor.get(msg.arbitration_id, "???") 
    
    print(f"[{label}] V: {telemetry['voltage']:4.2f}V | I: {telemetry['current']:4.2f}A | "
          f"Closed: {int(telemetry['closed'])} | Healthy: {int(telemetry['sensor_healthy'])} | Fault: {int(telemetry['fault'])}")
