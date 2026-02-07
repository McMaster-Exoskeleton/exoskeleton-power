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
# 4. Receive Samples from Both Sensors
############################################

# Sensor 1 data
voltages1 = []
currents1 = []
powers1   = []

# Sensor 2 data
voltages2 = []
currents2 = []
powers2   = []

# Wait for user to manually trigger data collection by entering '1'
start = 0
while(start != 1):
    start = int(input("Enter '1' to start sampling."))

print("Receiving samples...")

while True:
    line_test=ser.readline()
    print("Raw data: ", line_test, "\n")
    line = line_test.decode("ascii", errors="ignore").strip()

    if not line:
        continue

    if line == "DONE":
        print("Sampling complete.")
        break

    try:
        # Parse format: v1,c1,p1,v2,c2,p2
        
        values = list(map(float, line.split(",")))
        
        if len(values) == 6:
            v1, c1, p1, v2, c2, p2 = values
            
            # Sensor 1
            voltages1.append(v1)
            currents1.append(c1)
            powers1.append(p1)
            
            # Sensor 2
            voltages2.append(v2)
            currents2.append(c2)
            powers2.append(p2)
        else:
            print(f"Warning: Expected 6 values, got {len(values)}")
        
    except ValueError as e:
        print(f"Error parsing line: {line} -> {e}")
        continue

ser.close()

############################################
# 5. Convert to NumPy
############################################

voltages1 = np.array(voltages1)
currents1 = np.array(currents1)
powers1   = np.array(powers1)

voltages2 = np.array(voltages2)
currents2 = np.array(currents2)
powers2   = np.array(powers2)

num_samples = len(voltages1)

if num_samples == 0:
    print("No data received.")
    exit(1)

# Time axis for plotting
time_vector = np.arange(num_samples) / sampling_rate

############################################
# 6. Plot Results
############################################

plt.style.use("seaborn-v0_8")

# Voltages
plt.figure(figsize=(12, 6))
plt.plot(time_vector, voltages1, label="Sensor 1 Voltage", color='blue', linewidth=2)
plt.plot(time_vector, voltages2, label="Sensor 2 Voltage", color='red', linewidth=2)
plt.xlabel("Time (s)", fontsize=12)
plt.ylabel("Voltage (V)", fontsize=12)
plt.title("Voltage Comparison", fontsize=14, fontweight='bold')
plt.grid(True, alpha=0.3)
plt.legend(fontsize=11)
plt.tight_layout()
plt.show()

# Currents
plt.figure(figsize=(12, 6))
plt.plot(time_vector, currents1, label="Sensor 1 Current", color='green', linewidth=2)
plt.plot(time_vector, currents2, label="Sensor 2 Current", color='orange', linewidth=2)
plt.xlabel("Time (s)", fontsize=12)
plt.ylabel("Current (A)", fontsize=12)
plt.title("Current Comparison", fontsize=14, fontweight='bold')
plt.grid(True, alpha=0.3)
plt.legend(fontsize=11)
plt.tight_layout()
plt.show()

# Powers
plt.figure(figsize=(12, 6))
plt.plot(time_vector, powers1, label="Sensor 1 Power", color='purple', linewidth=2)
plt.plot(time_vector, powers2, label="Sensor 2 Power", color='brown', linewidth=2)
plt.xlabel("Time (s)", fontsize=12)
plt.ylabel("Power (W)", fontsize=12)
plt.title("Power Comparison", fontsize=14, fontweight='bold')
plt.grid(True, alpha=0.3)
plt.legend(fontsize=11)
plt.tight_layout()
plt.show()

# All parameters in subplots
fig, axes = plt.subplots(3, 1, figsize=(12, 10))

# Voltage subplot
axes[0].plot(time_vector, voltages1, label="Sensor 1", color='blue', linewidth=2)
axes[0].plot(time_vector, voltages2, label="Sensor 2", color='red', linewidth=2)
axes[0].set_ylabel("Voltage (V)", fontsize=11)
axes[0].set_title("Dual Sensor Monitoring", fontsize=14, fontweight='bold')
axes[0].grid(True, alpha=0.3)
axes[0].legend(fontsize=10)

# Current subplot
axes[1].plot(time_vector, currents1, label="Sensor 1", color='green', linewidth=2)
axes[1].plot(time_vector, currents2, label="Sensor 2", color='orange', linewidth=2)
axes[1].set_ylabel("Current (A)", fontsize=11)
axes[1].grid(True, alpha=0.3)
axes[1].legend(fontsize=10)

# Power subplot
axes[2].plot(time_vector, powers1, label="Sensor 1", color='purple', linewidth=2)
axes[2].plot(time_vector, powers2, label="Sensor 2", color='brown', linewidth=2)
axes[2].set_xlabel("Time (s)", fontsize=11)
axes[2].set_ylabel("Power (W)", fontsize=11)
axes[2].grid(True, alpha=0.3)
axes[2].legend(fontsize=10)

plt.tight_layout()
plt.show()

############################################
# 7. Summary Statistics
############################################

print(f"\n{'='*60}")
print(f"Data Collection Summary")
print(f"{'='*60}")
print(f"Total samples received: {num_samples}")
print(f"Sampling rate: {sampling_rate} Hz")
print(f"Duration: {time_vector[-1]:.2f} seconds")
print(f"\n{'Sensor 1 Statistics':^30} | {'Sensor 2 Statistics':^30}")
print(f"{'-'*30}|{'-'*30}")
print(f"Voltage: {voltages1.mean():.3f}V ± {voltages1.std():.3f}V | Voltage: {voltages2.mean():.3f}V ± {voltages2.std():.3f}V")
print(f"Current: {currents1.mean():.3f}A ± {currents1.std():.3f}A | Current: {currents2.mean():.3f}A ± {currents2.std():.3f}A")
print(f"Power:   {powers1.mean():.3f}W ± {powers1.std():.3f}W | Power:   {powers2.mean():.3f}W ± {powers2.std():.3f}W")
print(f"{'='*60}\n")

print("Plotting complete.")
