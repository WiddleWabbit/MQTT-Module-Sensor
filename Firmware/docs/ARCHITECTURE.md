# Sensor module firmware

This firmware runs on the ATmega328PB sensor daughterboard. The ESP32
motherboard is the I2C master. This module is a slave. Plugging it in is
enough for the motherboard to assign an address and identify a sensor
module with no inputs yet.

## Layers

Dependencies point at interfaces. `src/main.cpp` is the only composition
point.

- `lib/Interfaces/` holds `ModuleProtocol.h` and `IModuleSlavePort`.
  `ModuleProtocol.h` is a copy of the motherboard header, with `stddef.h`
  and `stdint.h` so AVR can compile it. Frame layout, CRC-8/SMBus, and
  command bytes live there.
- `lib/Logic/ModuleLink` decides when the slave listens, checks frames,
  and builds replies. It does not include Arduino.
- `lib/Drivers/AvrTwi1Slave` is the TWI1 adapter. Desktop tests use
  `FakeModuleSlavePort` instead.
- `src/main.cpp` owns the pin numbers, drives SNS low, and calls
  `ModuleLink::update` from `loop()`.

## What loop does

`setup()` drives SNS (PD4) low so the motherboard sense line says the
module is seated, and holds MOD (PD3) as an input with pull-up. The slave
stays disabled.

`loop()` reads MOD. While the module has no address, MOD low enables
address `0x0A` and MOD high disables it. That happens on the next loop
pass, inside the motherboard's 5 ms gate, because `loop()` does not block.
After a valid `SET_ADDRESS`, the assigned address stays enabled when MOD
goes high.

The motherboard bus is schematic I2C0. On this chip that is TWI1, Arduino
`Wire1`: PE0 is SCL and PE1 is SDA. The ADC bus is schematic I2C1, TWI0 on
PC4/PC5, and this firmware does not touch it.

## Desktop tests

Protocol behaviour is tested on the desktop with Unity. The fake records
`enable`, `disable`, `setAddress`, and `setReply`.

```text
pio test -e native
```

The AVR image is built with `pio run -e Upload_ISP`. That build checks the
Wire1 driver. The interrupt path itself is not executed on the desktop.

## Not in this version

The module reports a sensor count of 0. Presence and reading commands
return `BadLength`. The ADS1115 is not started.
