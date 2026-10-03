# ADC port reference

[Home](../home.md) · Guide: [ADC port](../adc-port.md)

## Purpose

One ADC port, two adapters. The scan starts a conversion, polls it,
and reads the counts. `AvrAds1115` does that on TWI0 through Adafruit
ADS1X15. `FakeAds1115` records the same calls.

## Classes

`IAds1115`:

| Method | Role |
| --- | --- |
| `begin()` | Start the bus and probe the chip |
| `startSingleEnded(channel)` | Start one single-shot conversion. Channel 0 is AIN0 |
| `conversionReady()` | True when the OS bit says the conversion is done |
| `readCounts()` | Signed result of the last conversion |

`AvrAds1115(address7bit, gain, dataRate)` stores those settings and
leaves the bus idle. `kGainOne` is `0x0200` (±4.096 V, 125 uV per
count). `kRate860Sps` is `0x00E0`. Copy and assignment are deleted.

`FakeAds1115` records `begin`, `startSingleEnded`, `conversionReady`,
and `readCounts`. `readCounts` returns `counts[lastChannel]`.

## States

The driver starts with the bus idle. `begin()` sets the gain and the
data rate, calls `Wire.begin()`, and probes the address. A gain other
than `kGainOne`, or a rate other than `kRate860Sps`, returns false and
does not mark the bus ready. Later calls return false, or 0 counts,
until a `begin()` succeeds.

## Sequences

`startSingleEnded` rejects a channel above 3. Adafruit's
`startADCReading` writes the config register and both threshold
registers. The threshold writes arm ALERT/RDY. That pin is
unconnected on this board. `conversionReady()` polls the config
register OS bit.

`Wire.begin()` turns on the internal pull-ups on PC4 and PC5, in
parallel with the board's pull-ups. `ADC_SDA_PIN` is 18 and
`ADC_SCL_PIN` is 19. `Wire` takes no pin arguments.

The ADS1115 address is `ADS1115_ADDRESS` (`0x48`). ADDR is strapped
to GND.

## Errors

`begin()` false means the chip did not acknowledge, or the gain or
rate was refused. `startSingleEnded` false means the bus is not ready
or the channel is above 3. `readCounts()` before a successful `begin()`
returns 0.

Adafruit's start and register reads do not report a bus error of
their own. A chip that stops acknowledging after `begin()` leaves
`conversionReady()` false, so the scan stays on that channel.

## Configuration

Passed from `main.cpp`: address `0x48`, `kGainOne`, `kRate860Sps`.
Single-ended readings use the positive half of the code, 0..32767.
The connected threshold is a `SensorInputs` setting, on the
[sensor inputs reference](sensor-inputs.md).

## Tests

There is no separate driver test. The sensor-input tests drive
`FakeAds1115`. The cases are on the
[sensor inputs reference](sensor-inputs.md). The TWI0 path is not
executed on the desktop. `pio run -e Upload_ISP` compiles
`AvrAds1115`.

## Source

- `lib/Interfaces/IAds1115.h`
- `lib/Drivers/AvrAds1115.h`
- `lib/Drivers/AvrAds1115.cpp`
- `test/test_desktop/fakes/FakeAds1115.h`
