# Sensor module firmware

The sensor daughter is one module. `src/main.cpp` constructs the two
bus adapters, the sensor scan, and the link. It calls `update()` on
the scan and on the link. It does not run the steps inside either one.

## How the firmware runs

Each of these is a short page: what that part creates, and what one
pass does.

1. [Architecture](architecture.md) — the loop, and who owns whom.
2. [Module link](module-link.md) — the MOD gate, then one master write.
3. [Sensor inputs](sensor-inputs.md) — one ADS1115 step, then the cache.
4. [Slave port](slave-port.md) — TWI1 on the AVR, and the desktop fake.
5. [ADC port](adc-port.md) — TWI0 to the ADS1115, and the desktop fake.

## When you need a byte, a state, or a test

The [reference index](reference/index.md) is the lookup. Each page
there has the states, the sequences, the errors, and the desktop test
that covers them.

## Wire contract

`lib/Interfaces/ModuleProtocol.h` is the copy of the motherboard
header. Frame layout, CRC-8/SMBus, command bytes, `kModGateMaxMs`, and
`kClockStretchMaxMs` live there. If a page in this tree and that header
disagree, the header wins. Change the motherboard header, then copy it
here.

This tree documents the slave. The motherboard's master sequences stay
in the motherboard firmware.
