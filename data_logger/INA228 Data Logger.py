"""
INA228 Data Logger
==================

Title: Serial Data Logger for INA228 Power Monitoring

Description:
    This script communicates with an STM32 Nucleo microcontroller via serial
    to collect power measurements from an INA228 power monitor IC. It logs
    voltage, current, and power data at a user-specified sampling rate and
    duration, then visualizes the results.

Usage:
    1. Connect STM32 Nucleo to PC via USB (serial interface)
    2. Update SERIAL_PORT if needed (default: COM6)
    3. Run: python "INA228 Data Logger.py"
    4. Enter desired sampling rate (Hz) when prompted
    5. Enter desired total collection time (seconds) when prompted
    6. Data will be collected and saved to 'ina228_data.csv'
    7. Plot window will display the collected measurements

Requirements:
    - pyserial
    - numpy
    - matplotlib

Data Format:
    The output CSV file contains columns: Time (s), Voltage (V), Current (mA), Power (mW)

Author: Exoskeleton Power Team
Date: January 2026
"""

import serial
import time
import numpy as np
import matplotlib.pyplot as plt

############################################
# 1. Serial Configuration
############################################

SERIAL_PORT = "COM6"      # <-- change if needed
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

voltages = []
currents = []
powers   = []

print("Receiving samples...")

start = 0
while(start != 1):
    start = int(input("Enter '1' to start sampling."))

# Print header for console output
print("\n" + "="*60)
print(f"{'Sample':<8} {'Voltage (V)':<15} {'Current (A)':<15} {'Power (W)':<15}")
print("="*60)

while True:
    line_test = ser.readline()
    print("Test values: ", line_test, "\n")
    line = line_test.decode("ascii", errors="ignore").strip()

    if not line:
        continue

    if line == "DONE":
        print("Sampling complete.")
        break

    try:
        v, i, p = map(float, line.split(","))
        voltages.append(v)
        currents.append(i)
        powers.append(p)
        
        # Print values to console
        sample_count += 1
        print(f"{sample_count:<8} {v:<15.6f} {i:<15.6f} {p:<15.6f}")
        
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

# Voltage
plt.figure(figsize=(10, 5))
plt.plot(time_vector, voltages, label="Voltage (V)")
plt.xlabel("Time (s)")
plt.ylabel("Voltage (V)")
plt.title("Voltage vs Time")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()

# Current
plt.figure(figsize=(10, 5))
plt.plot(time_vector, currents, label="Current (A)")
plt.xlabel("Time (s)")
plt.ylabel("Current (A)")
plt.title("Current vs Time")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()

# Power
plt.figure(figsize=(10, 5))
plt.plot(time_vector, powers, label="Power (W)")
plt.xlabel("Time (s)")
plt.ylabel("Power (W)")
plt.title("Power vs Time")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()

# Combined
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
