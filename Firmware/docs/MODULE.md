# Module link

`ModuleLink` is the slave side of the motherboard module protocol. It
exists so this board can be plugged in, accept an address, and identify
as a sensor module. Readings are later work. The count is 0, so the
motherboard stores that count and does not poll inputs.

The on-wire constants are in `lib/Interfaces/ModuleProtocol.h`, copied
from the motherboard firmware. Framing and CRC are defined there.

## Pins

Declared in `src/main.cpp`, in the same style as the motherboard sketch.

| Constant | Arduino pin | Port | Role |
| --- | ---: | --- | --- |
| `SCL_PIN` | 23 | PE0 | Motherboard SCL, TWI1 |
| `SDA_PIN` | 24 | PE1 | Motherboard SDA, TWI1 |
| `MOD_PIN` | 3 | PD3 | Enumeration select, input pull-up |
| `SNS_PIN` | 4 | PD4 | Seated detect, driven low |

MiniCore's pin header labels PE0 as `SDA1` and PE1 as `SCL1`. The TWI1
peripheral and this PCB use PE0 as SCL and PE1 as SDA. `Wire1.begin()`
still connects that peripheral. Its init only enables the internal
pull-ups on both pins, in parallel with the motherboard's 4.7 kΩ
resistors. `Wire1.end()` clears those pull-ups and leaves the pins as
inputs, so the bus stays released.

`SCL_PIN` and `SDA_PIN` document the wiring. `Wire1` does not take a pin
argument because TWI1's pins are fixed.

## Public API

`ModuleLink(IModuleSlavePort& port)`

Binds the port. The slave is disabled. `address()` is `0x0A`.

`update(bool modIsLow)`

Before assignment, MOD low calls `enable(0x0A)` once. MOD high calls
`disable()`. After assignment, `update` does not touch the port.

`onMasterWrite(const uint8_t* data, size_t length)`

Called from the driver when a write ends (stop, or the repeated start
before a read). A short frame or a length that does not match the length
byte queues `BadLength`. A bad CRC queues `BadCrc` and does not change
the address. A valid command queues a 19-byte reply: the real frame, then
`0xFF`. `SET_ADDRESS` is write-only and queues no reply.

`address()` and `slaveEnabled()`

Observed by tests and available to the sketch.

`kFirmwareVersion`

Firmware version reported in `GET_IDENTITY`. It is 1.

`IModuleSlavePort` is `enable`, `disable`, `setAddress`, and `setReply`.
`setReply` copies the bytes before it returns. `AvrTwi1Slave` is the
hardware port. One instance is attached, because Wire's callbacks are
plain function pointers.

## States

```text
boot, MOD high          slave off, address 0x0A
MOD low                 enable 0x0A
MOD high, no address    disable
SET_ADDRESS accepted    setAddress(new), stay enabled
MOD high after that     stay enabled at the new address
```

`SET_ADDRESS` is accepted only while the slave is enabled, the module
does not already have an address, the frame is 4 bytes with a good CRC,
and the address is in `0x10`–`0x6F`. The driver writes `TWAR1` from that
callback, which runs on stop, so the address is committed before the
motherboard can release MOD. General call `0x00` is not acknowledged.

## Commands

| Command | Result |
| --- | --- |
| PING | status Ok, empty payload |
| GET_IDENTITY | type `0x0200`, protocol 1, firmware 1 |
| SET_ADDRESS | commit after stop, or ignore a bad address |
| GET_SENSOR_COUNT | count `0` |
| GET_SENSOR_CONNECTED, GET_SENSOR_READING | `BadLength` |
| anything else, including ECHO | `UnknownCmd` |

A PING, identity, or count request that carries a payload is `BadLength`.
The reply is what the motherboard's 19-byte `writeRead` expects. Parsing
runs inside the TWI interrupt and only walks a short frame, inside the
40 ms clock-stretch limit.

## Tests

`test/test_desktop/test_module_link.cpp` drives `FakeModuleSlavePort`.

| Test | What it locks |
| --- | --- |
| `testBootWithModHighLeavesSlaveDisabled` | Boot with MOD high does not enable the slave |
| `testModLowEnablesUnconfiguredAddressAndModHighDisables` | `0x0A` follows MOD until assignment |
| `testPingReplyIsOkAndPadded` | Ok PING, CRC, 19-byte `0xFF` pad |
| `testIdentityReportsSensorModule` | Type `0x0200`, protocol 1, firmware 1 |
| `testSetAddressCommitsAndSurvivesModHigh` | Commit `0x10`, ignore a later address, keep answering after MOD rises |
| `testSetAddressRejectsBadCrcAndIllegalAddresses` | Bad CRC, `0x00`, `0x0A`, and `0x70` stay at `0x0A` |
| `testSensorCountIsZeroAndReadingIsBadLength` | Count 0; reading and presence are `BadLength` |
| `testUnknownCommandAndMalformedFrames` | ECHO, truncated frame, extra byte, bad CRC, PING with a payload, empty SET_ADDRESS |
| `testEnumerationSequence` | PING, assign `0x12`, MOD high, identity, count 0 |

```text
pio test -e native
```
