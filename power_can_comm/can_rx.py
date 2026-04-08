import can
import struct

# Initialize bus
bus = can.interface.Bus(channel="can1", interface="socketcan")

print("Exoskeleton Telemetry Started...")

telemetry = {
    "v": 0.0, 
    "c": 0.0, 
    "closed": 0.0, 
    "sensor_status": 0.0, 
    "fault": 0, 
}

ids = [0x100, 0x101, 0x102, 0x103, 0x104] # Bus, M1, M2, M3, M4

for msg in bus:
    if msg.arbitration_id in ids:
        # data[0:6] contains data triplet packed as 16-bit signed integers (Big Endian)
        # '>hhh' tells Python to unpack 3 signed shorts
        try:
            v_r, c_r = struct.unpack('<hh', msg.data[0:3])
            c, s, f = struct.unpack('???', msg.data[5:7])

            # Divide by 100 because we multiplied by 100 in main.c
            telemetry["v"] = v_r 
            telemetry["c"] = c_r
            telemetry["closed"] = c
            telemetry["sensor_status"] = s
            telemetry["fault"] = f
        except Exception as e:
            print(f"Error in retrieving data: {e}")

    print(f" v: {telemetry['v']:6.2f} | c {telemetry['c']:6.2f} ")
    print(f"closed: {telemetry['closed']:6.2f} | sensor_status {telemetry['sensor_status']:6.2f} | fault: {telemetry['fault']:6.2f} ")