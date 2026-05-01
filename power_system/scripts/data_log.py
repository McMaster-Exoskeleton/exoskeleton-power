"""
data_log.py
 
UART data logger for STM32 bus sensor measurements.
Sends a START command to the MCU over a serial connection, receives
timestamped voltage, current, and power samples, and plots the results using matplotlib. 
Sampling rate and duration are inputted by the user at runtime.
"""

import serial
import time
import numpy as np
import matplotlib.pyplot as plt

############################################
# 1. Serial Configuration
############################################

SERIAL_PORT = "COM14"      # <-- change if needed
BAUD_RATE   = 115200
TIMEOUT_S   = 2

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TIMEOUT_S)
time.sleep(2)  # allow Nucleo reset

############################################
# 2. User Input
############################################

sampling_rate = int(input("Enter sampling rate (Hz): "))
total_time    = int(input("Enter total time (seconds): "))

command = f"START,{sampling_rate},{total_time}\n"
print("Sending:", command.strip())

ser.write(command.encode("ascii"))

############################################
# 3. Wait for MCU Acknowledgment
############################################

response = ser.readline().decode("ascii", errors="ignore").strip()
print("MCU response:", response)

if response != "OK":
    print("MCU did not acknowledge command.")
    ser.close()
    exit(1)

############################################
# 4. Receive Samples
############################################

FAULT_MESSAGES = {
    "FAULT_BUS_OVERCURRENT",
    "FAULT_BUS_OVERVOLTAGE",
    "FAULT_BUS_UNDERVOLTAGE",
    "FAULT_SENSOR_COMM",
    "FAULT_MOTOR_OVERCURRENT",
    "FAULT_UNKNOWN",
}

def is_fault(line):
    return line in FAULT_MESSAGES

voltages = []
currents = []
powers   = []

print("Receiving samples...")

# Print header for console output
print("\n" + "="*60)
print(f" {'Voltage (V)':<15} {'Current (A)':<15} {'Power (W)':<15}")
print("="*60)

while True:
    line_test = ser.readline()
    line = line_test.decode("ascii", errors="ignore").strip()

    if not line:
        continue
    
     # Check for fault message from MCU
    if is_fault(line):
        print(f"\n  MCU reported fault: {line}")
        print("Aborting data collection.")
        ser.close()
        exit(1)

    if line == "DONE":
        print("Sampling complete.")
        break

    try:
        v, i, p = map(float, line.split(","))
        voltages.append(v)
        currents.append(i)
        powers.append(p)
        
        # Print values to console
        print(f"{v:<15.6f} {i:<15.6f} {p:<15.6f}")
        
    except ValueError:
        # ignore malformed lines
        continue

ser.close()

############################################
# 5. Convert to NumPy
############################################

voltages = np.array(voltages)
currents = np.array(currents)
powers   = np.array(powers)

num_samples = len(voltages)

if num_samples == 0:
    print("No data received.")
    exit(1)

time_vector = np.arange(num_samples) / sampling_rate

############################################
# 6. Plot Results
############################################

plt.style.use("seaborn-v0_8")

plt.figure(figsize=(10, 5))
plt.plot(time_vector, voltages, label="Voltage (V)")
plt.plot(time_vector, currents, label="Current (A)")
plt.plot(time_vector, powers,   label="Power (W)")
plt.xlabel("Time (s)")
plt.ylabel("Value")
plt.title("Voltage, Current, and Power vs Time")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()

############################################
# 7. Summary
############################################

print(f"\nReceived {num_samples} samples.")
print("Plotting complete.")
