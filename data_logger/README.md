# INA228 Data Logger

## Overview
This data logger captures real-time power measurements from an INA228 power monitoring IC connected to an STM32 Nucleo microcontroller. The Python script communicates via serial connection to collect voltage, current, and power data.

## Features
- **Real-time Data Collection**: Configurable sampling rate (Hz)
- **Flexible Duration**: User-specified collection time
- **Visualization**: Auto-generates plots of voltage, current, and power over time
- **Serial Communication**: Connects to Nucleo via USB serial interface

### Hardware
- STM32 NUCLEO-F446RE
- INA228 power monitoring IC
- USB mini-B cable for serial connection

## Usage

### 1. Hardware Setup
- Connect INA228 to Nucleo I2C interface (SCL - PB6, SDA - PB7) - configure to Fast Mode (400 kHz)
- Connect power to be measured through INA228
- Connect Nucleo to PC via USB

### 2. Firmware Upload
Upload the C firmware from `Src/main.c` to your Nucleo board using STM32CubeIDE

### 3. Run Data Logger
```bash
python "INA228 Data Logger.py"
```

### 4. Configure Collection
When prompted:
- **Sampling Rate**: Enter desired Hz (e.g., `100`)
- **Duration**: Enter total seconds to collect (e.g., `60`)

### 5. View Results
- Plots display automatically after collection completes

## Configuration
Edit these settings in `INA228 Data Logger.py`:
```python
SERIAL_PORT = "COM6"      # Change to your serial port
```