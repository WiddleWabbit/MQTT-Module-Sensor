# MQTT Module - Current Sensors

*Currently In Testing - Not Yet Tested*

This module is a plug-in board for the [MQTT Modular Controller Board](https://github.com/WiddleWabbit/MQTT-ModularControllerBoard). It powers and reads up to three 4-20mA current sensors with high-side sensing at approximately 25V, providing 15-bit resolution measurements.

## Features

- Supports up to 3 x 4-20mA sensors (high-side sensing)
- 15-bit resolution via ADS1115 ADC
- On-board ~25V boost converter from 12V input (TPS61170)
- ATmega328PB microcontroller for local control and communication
- Precision 0.1% shunt resistors with low temperature coefficient (25ppm/°C)
- Basic short-circuit protection using PTCs and TVS diodes
- I2C communication with motherboard and internal ADC

## Architecture

The module is built around an **ATmega328PB-A** microcontroller. It interfaces with the motherboard via I2C (SDA on PE1, SCL on PE0) and uses a dedicated I2C bus for the **ADS1115** ADC (base address 0x48, SDA on PC4, SCL on PC5).

### Power
- Utilises 12V input from motherboard, boosted to ~25V using TI TPS61170 (WEBENCH design) with shielded inductor for reduced EMI
- Powers up to three 4-20mA sensors using this 25V.
- Also utilises the 3.3V from the motherboard.
- Maximum 25V current draw should be less than 200mA.
- Maximum 3.3V current draw should be less than 200mA.

### Sensor Interface
- TP5551 OpAmps in servo configuration for accurate current-to-voltage conversion
- 1.5uF capacitors for signal smoothing
- 0.1% precision shunt resistors with a low 25ppm/°C

### Protection
- PTC resettable fuses on sensors trigger at ~150mA
- TVS diodes for overvoltage clamping (trigger at 13V, clamp to 21.5V)
- Shunt rated for 21.5V for five seconds

### Pinout and Connections
- **Motherboard I2C**: SDA (PE1), SCL (PE0)
- **Internal ADC I2C**: SDA (PC4), SCL (PC5)
- **SNS pin** (motherboard sense): PD4
- **General use modular pin**: PD3
- **ISP Programming Header** (2x3, left to right, top to bottom): MISO, VCC, SCK, MOSI, RESET, GND

## Setup and Build

**Firmware**: Not yet implemented. TBA.

**Programming**:
1. Use the 2x3 ISP header to program the ATmega328PB.
2. Connect to an ISP programmer (e.g., USBasp or Arduino as ISP).
3. Firmware will handle sensor readings, ADC configuration, and I2C communication with the motherboard.

**Assembly**:
- Populate the board per the KiCad project files.
- Ensure proper orientation when plugging into the motherboard slots.

## License

This documentation is part of MQTT-Module-Sensor Project and is licensed under the CERN Open Hardware Licence Version 2 - Permissive (CERN-OHL-P-2.0). 
You may redistribute and modify this documentation under the terms of the CERN-OHL-P-2.0. 
This documentation is distributed WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING OF MERCHANTABILITY, SATISFACTORY QUALITY AND FITNESS FOR A PARTICULAR PURPOSE. See the CERN-OHL-P-2.0 for applicable conditions.
