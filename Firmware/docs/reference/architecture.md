# Architecture reference

[Home](../home.md) · Guide: [Architecture](../architecture.md)

## Purpose

`src/main.cpp` wires this daughterboard. It owns the pins and the
product settings, constructs the adapters, the scan, and the link, and
calls the scan and the link once per pass.

## Classes

`setup` drives `SNS_PIN` and configures `MOD_PIN`. `loop` calls
`SensorInputs::update`, reads `MOD_PIN`, and calls `ModuleLink::update`.
The objects are `moduleSlave` (`AvrTwi1Slave`), `ads` (`AvrAds1115`),
`sensorInputs` (`SensorInputs`), and `moduleLink` (`ModuleLink`).

## States

The sketch has no mode of its own. Listening, assignment, and replies
are the link's states, on the [module link reference](module-link.md).
The scan's phases are on the
[sensor inputs reference](sensor-inputs.md).

## Sequences

1. Construction registers the master-write handler. The slave stays off.
2. `setup` sets `SNS_PIN` as an output driven low, and `MOD_PIN` as an input with pull-up.
3. Each `loop` advances the scan one driver call, reads `MOD_PIN`, and calls the link. Both calls return without waiting.

The host holds MOD low for `kModGateMaxMs` (5 ms). Because `loop` does
not block, the unconfigured address is enabled on the pass that sees
MOD low.

## Errors

Protocol status bytes come from the link. A handler that runs in the
TWI interrupt finishes inside `kClockStretchMaxMs` (40 ms). Both limits
are in `ModuleProtocol.h`.

## Configuration

Declared in `src/main.cpp`.

| Constant | Value | Port | Role |
| --- | --- | --- | --- |
| `SCL_PIN` | 23 | PE0 | Motherboard SCL, TWI1 |
| `SDA_PIN` | 24 | PE1 | Motherboard SDA, TWI1 |
| `MOD_PIN` | 3 | PD3 | Enumeration select, input pull-up |
| `SNS_PIN` | 4 | PD4 | Seated detect, driven low |
| `ADC_SDA_PIN` | 18 | PC4 | ADS1115 SDA, TWI0. `Wire` takes no pin argument |
| `ADC_SCL_PIN` | 19 | PC5 | ADS1115 SCL, TWI0. `Wire` takes no pin argument |
| `MODULE_TYPE` | `kTypeSensorModule` | | Reported by `GET_IDENTITY` |
| `FIRMWARE_VERSION` | 1 | | Reported by `GET_IDENTITY` |
| `ADS1115_ADDRESS` | `0x48` | | ADDR strapped to GND |
| `SENSOR_COUNT` | 3 | | Inputs reported by `GET_SENSOR_COUNT` |
| `SENSOR_CONNECTED_MIN_COUNTS` | 2400 | | Connected at or above this raw count |

`ads` is constructed with `AvrAds1115::kGainOne` (`0x0200`, ±4.096 V)
and `AvrAds1115::kRate860Sps` (`0x00E0`).

`kTypeSensorModule` is `0x0200` in `ModuleProtocol.h`. MiniCore's names
for the TWI1 pins are reversed. That constraint is on the
[slave port reference](slave-port.md).

The image is an ATmega328PB at 8 MHz from the internal oscillator.
`Upload_ISP` programs it with stk500v1 at 19200 on COM3.
`fuses_bootloader` sets no bootloader, brown-out 2.7 V, and EEPROM
save. The avrdude lines are in the firmware `README.md`.

## Tests

Desktop cases and the command that runs them are on the
[module link reference](module-link.md) and the
[sensor inputs reference](sensor-inputs.md).

The image build is:

```text
pio run -e Upload_ISP
```

That environment does not run the desktop tests. The TWI interrupt
path and the ADS1115 bus are not executed on the desktop.

## Source

- `src/main.cpp`
- `platformio.ini`

Sources in a library root are compiled when one of its headers is
included. `platformio.ini` does not list `lib_deps` for that reason.
