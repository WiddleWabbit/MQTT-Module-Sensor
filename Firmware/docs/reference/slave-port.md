# Slave port reference

[Home](../home.md) · Guide: [Slave port](../slave-port.md)

## Purpose

One slave port, two adapters. The link enables TWI, commits an address,
and queues a reply. `AvrTwi1Slave` does that on TWI1.
`FakeModuleSlavePort` records the same calls.

## Classes

`IModuleSlavePort`:

| Method | Role |
| --- | --- |
| `enable(address7bit)` | Start listening at a 7-bit address |
| `disable()` | Stop acknowledging |
| `setAddress(address7bit)` | Commit a new address without leaving the bus |
| `setReply(frame, length)` | Copy the next reply. The pointer is valid only for this call |
| `setMasterWriteHandler(handler, context)` | One handler. A later call replaces it |

`AvrTwi1Slave` implements that on `Wire1`. `FakeModuleSlavePort`
records `enable`, `disable`, `setAddress`, and `setReply`. `deliver`
invokes the registered handler.

## States

The driver starts disabled. `enable` selects this instance, clears the
reply length, calls `Wire1.begin(address7bit)`, and registers
`onReceive` and `onRequest`. `disable` calls `Wire1.end()` and clears
the enabled flag. `setAddress` writes `TWAR1` only while enabled.

## Sequences

`setAddress` writes `TWAR1` as `address7bit << 1`. Bit 0 stays clear,
so general call stays off. The write is safe from the stop callback.
It does not call `Wire1.end()`.

`disable` stops TWI1. The pins return to inputs with the internal
pull-ups off. The motherboard's 4.7 kΩ resistors hold the bus.

`enable` turns on the internal pull-ups on both TWI1 pins, in parallel
with those resistors.

`_onReceive` reads the bytes `Wire1` has and calls the handler. A
missing instance or a missing handler returns. `_onRequest` clocks out
the stored reply. A reply length of 0 sends nothing.

MiniCore's pin header labels PE0 as `SDA1` and PE1 as `SCL1`. The TWI1
peripheral and this PCB use PE0 as SCL and PE1 as SDA. `Wire1.begin()`
still connects that peripheral. `SCL_PIN` and `SDA_PIN` in `main.cpp`
name those pins. `Wire1` takes no pin arguments.

## Errors

`setReply` with a null frame stores a length of 0, so the next read
sends nothing. A length above `kMaxFrameBytes` is stored as 19 bytes.

`setAddress` while disabled does not write `TWAR1`.

`_onReceive` passes the handler the number of bytes read from `Wire1`.
When `Wire1`'s count is larger than that, the count is the length
instead. Only the first `kMaxFrameBytes` are stored. The link then
applies its length check to that buffer.

## Configuration

The driver has no product settings. The address comes from the link:
`kUnconfiguredAddress`, then the assigned address. Pin constants are
on the [architecture reference](architecture.md).

## Tests

There is no separate driver test. The module-link tests drive
`FakeModuleSlavePort`. The cases are on the
[module link reference](module-link.md). The interrupt path is not
executed on the desktop. `pio run -e Upload_ISP` compiles
`AvrTwi1Slave`.

## Source

- `lib/Interfaces/IModuleSlavePort.h`
- `lib/Drivers/AvrTwi1Slave.h`
- `lib/Drivers/AvrTwi1Slave.cpp`
- `test/test_desktop/fakes/FakeModuleSlavePort.h`
