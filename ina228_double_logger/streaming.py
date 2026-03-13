import serial
import time
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

############################################
# Configuration
############################################

SERIAL_PORT = "COM6"
BAUD_RATE = 115200
TIMEOUT_S = 2
MAX_POINTS = 500  # How many points to show on screen at once

############################################
# Setup Serial
############################################

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TIMEOUT_S)
time.sleep(2)

############################################
# User Input
############################################

sampling_rate = int(input("Enter sampling rate (Hz): "))
total_time = int(input("Enter total time (seconds): "))

command = f"START,{sampling_rate},{total_time}\n"
print("Sending:", command.strip())
ser.write(command.encode("ascii"))

response = ser.readline().decode("ascii", errors="ignore").strip()
print("MCU response:", response)

if response != "OK":
    print("MCU did not acknowledge command.")
    ser.close()
    exit(1)

############################################
# Data Storage (Circular Buffers)
############################################

# Use deque for efficient real-time updates (auto-removes old data)
time_data = deque(maxlen=MAX_POINTS)
voltages1 = deque(maxlen=MAX_POINTS)
currents1 = deque(maxlen=MAX_POINTS)
powers1 = deque(maxlen=MAX_POINTS)
voltages2 = deque(maxlen=MAX_POINTS)
currents2 = deque(maxlen=MAX_POINTS)
powers2 = deque(maxlen=MAX_POINTS)

# Also store all data for final analysis
all_voltages1 = []
all_currents1 = []
all_powers1 = []
all_voltages2 = []
all_currents2 = []
all_powers2 = []

current_time = 0
is_done = False

############################################
# Create Figure
############################################

fig, axes = plt.subplots(3, 1, figsize=(12, 10))
fig.suptitle('Real-Time Dual Sensor Monitoring', fontsize=16, fontweight='bold')

# Voltage plot
line_v1, = axes[0].plot([], [], 'b-', label='Sensor 1', linewidth=2)
line_v2, = axes[0].plot([], [], 'r-', label='Sensor 2', linewidth=2)
axes[0].set_ylabel('Voltage (V)', fontsize=11)
axes[0].set_xlim(0, 10)
axes[0].set_ylim(0, 60)
axes[0].legend(loc='upper right')
axes[0].grid(True, alpha=0.3)

# Current plot
line_c1, = axes[1].plot([], [], 'g-', label='Sensor 1', linewidth=2)
line_c2, = axes[1].plot([], [], 'orange', label='Sensor 2', linewidth=2)
axes[1].set_ylabel('Current (A)', fontsize=11)
axes[1].set_xlim(0, 10)
axes[1].set_ylim(-1, 10)
axes[1].legend(loc='upper right')
axes[1].grid(True, alpha=0.3)

# Power plot
line_p1, = axes[2].plot([], [], 'purple', label='Sensor 1', linewidth=2)
line_p2, = axes[2].plot([], [], 'brown', label='Sensor 2', linewidth=2)
axes[2].set_xlabel('Time (s)', fontsize=11)
axes[2].set_ylabel('Power (W)', fontsize=11)
axes[2].set_xlim(0, 10)
axes[2].set_ylim(0, 500)
axes[2].legend(loc='upper right')
axes[2].grid(True, alpha=0.3)

plt.tight_layout()

############################################
# Animation Update Function
############################################

def update_plot(frame):
    global current_time, is_done
    
    if is_done:
        return line_v1, line_v2, line_c1, line_c2, line_p1, line_p2
    
    # Read available data from serial
    while ser.in_waiting > 0:
        line = ser.readline().decode("ascii", errors="ignore").strip()
        
        if not line:
            continue
            
        if line == "DONE":
            is_done = True
            print("\nSampling complete!")
            break
            
        try:
            values = list(map(float, line.split(",")))
            
            if len(values) == 6:
                v1, c1, p1, v2, c2, p2 = values
                
                # Add to deques (for plotting)
                time_data.append(current_time)
                voltages1.append(v1)
                currents1.append(c1)
                powers1.append(p1)
                voltages2.append(v2)
                currents2.append(c2)
                powers2.append(p2)
                
                # Also store complete data
                all_voltages1.append(v1)
                all_currents1.append(c1)
                all_powers1.append(p1)
                all_voltages2.append(v2)
                all_currents2.append(c2)
                all_powers2.append(p2)
                
                current_time += 1.0 / sampling_rate
                
        except ValueError:
            continue
    
    # Update plot data
    if len(time_data) > 0:
        time_array = list(time_data)
        
        line_v1.set_data(time_array, list(voltages1))
        line_v2.set_data(time_array, list(voltages2))
        line_c1.set_data(time_array, list(currents1))
        line_c2.set_data(time_array, list(currents2))
        line_p1.set_data(time_array, list(powers1))
        line_p2.set_data(time_array, list(powers2))
        
        # Auto-scale x-axis
        if len(time_array) > 0:
            axes[0].set_xlim(max(0, time_array[-1] - 10), time_array[-1] + 0.5)
            axes[1].set_xlim(max(0, time_array[-1] - 10), time_array[-1] + 0.5)
            axes[2].set_xlim(max(0, time_array[-1] - 10), time_array[-1] + 0.5)
    
    return line_v1, line_v2, line_c1, line_c2, line_p1, line_p2

############################################
# Start Animation
############################################

print("Starting real-time plot...")
ani = animation.FuncAnimation(
    fig, 
    update_plot, 
    interval=50,      # Update every 50ms
    blit=True,
    cache_frame_data=False
)

plt.show()

ser.close()

############################################
# Final Summary Plot (All Data)
############################################

if len(all_voltages1) > 0:
    print(f"\nReceived {len(all_voltages1)} total samples")
    
    # Create final summary plot with all data
    time_vector = np.arange(len(all_voltages1)) / sampling_rate
    
    fig2, axes2 = plt.subplots(3, 1, figsize=(12, 10))
    fig2.suptitle('Complete Data Summary', fontsize=16, fontweight='bold')
    
    axes2[0].plot(time_vector, all_voltages1, 'b-', label='Sensor 1', linewidth=1.5)
    axes2[0].plot(time_vector, all_voltages2, 'r-', label='Sensor 2', linewidth=1.5)
    axes2[0].set_ylabel('Voltage (V)')
    axes2[0].legend()
    axes2[0].grid(True, alpha=0.3)
    
    axes2[1].plot(time_vector, all_currents1, 'g-', label='Sensor 1', linewidth=1.5)
    axes2[1].plot(time_vector, all_currents2, 'orange', label='Sensor 2', linewidth=1.5)
    axes2[1].set_ylabel('Current (A)')
    axes2[1].legend()
    axes2[1].grid(True, alpha=0.3)
    
    axes2[2].plot(time_vector, all_powers1, 'purple', label='Sensor 1', linewidth=1.5)
    axes2[2].plot(time_vector, all_powers2, 'brown', label='Sensor 2', linewidth=1.5)
    axes2[2].set_xlabel('Time (s)')
    axes2[2].set_ylabel('Power (W)')
    axes2[2].legend()
    axes2[2].grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.show()