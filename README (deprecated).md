# Piksi Multi GPS

This is a standalone C++ application for reading data from a SwiftNav Piksi Multi GPS sensor using the `libsbp` library. It prints GPS data such as UTC time, LLH, ECEF, RTK baseline, velocity, and base station information in a continuous loop.

## Prerequisites

- **libsbp**: SwiftNav's SBP library (v2.7.4 or compatible).
- **libserialport**: Library for serial port communication.
- **CMake**: Build system (version 3.10 or higher).
- **C++17**: Compiler supporting C++17 standard.

## Installation

1. **Install libsbp**:
   ```bash
   git clone --branch v2.7.4 https://github.com/swift-nav/libsbp.git
   cd libsbp/c
   mkdir build
   cd build
   cmake ..
   make
   sudo make install
   ```

2. **Install libserialport**:
   On Ubuntu:
   ```bash
   sudo apt-get install libserialport-dev
   ```

3. **Clone and Build the Project**:
   ```bash
   git clone <repository_url>
   cd PiksiMultiGPS
   mkdir build
   cd build
   cmake ..
   make
   ```

## Usage

1. **Connect the Piksi Multi GPS** to your computer (e.g., via `/dev/ttyUSB0`).
2. **Run the Application**:
   ```bash
   ./piksi_gps
   ```
   The program will initialize the GPS, wait for the first heartbeat, and then print GPS data in a loop.

## Configuration

- **Serial Port**: Default is `/dev/ttyUSB0`. Modify in `main.cpp` if needed.
- **Baud Rate**: Default is 115200. Modify in `main.cpp` if needed.

## Output

The program outputs:
- UTC time (hours, minutes, seconds, milliseconds)
- LLH (latitude, longitude, height) with accuracy
- ECEF position with accuracy
- RTK baseline (NED) with accuracy
- Velocity (NED) with accuracy
- Base station LLH and ECEF with accuracy
- Number of satellites and GPS status (RTK or non-RTK)

## Notes

- Ensure the Piksi Multi GPS is properly connected and configured.
- The application assumes `libsbp` is installed in `/usr/local/lib` and `/usr/local/include`.
- Press `Ctrl+C` to stop the program, which will close the serial port cleanly.