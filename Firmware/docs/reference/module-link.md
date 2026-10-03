# Module link reference

[Home](../home.md) · Guide: [Module link](../module-link.md)

## Purpose

Slave side of the module protocol for one sensor module. It accepts an
address while MOD is low, then identifies as the type and firmware
version it was given, with protocol version 1 and no sensors.

Frame layout, CRC-8/SMBus, and command bytes are in
`lib/Interfaces/ModuleProtocol.h`. If this page and that header
disagree, the header wins.

## Classes

`ModuleLink(IModuleSlavePort& port, uint16_t typeId, uint16_t firmwareVersion)`
binds the port and registers the master-write handler. The slave stays
off. Copy and assignment are deleted.

`update(bool modIsLow)` is the call `loop` makes.

The port methods the link uses are `enable`, `disable`, `setAddress`,
`setReply`, and `setMasterWriteHandler`. `setReply` copies the bytes
before it returns.

## States

`kUnconfiguredAddress` is `0x0A`.

```text
boot, MOD high          slave off; stored address is 0x0A
MOD low                 enable 0x0A once
MOD low again           no second enable
MOD high, no address    disable once
SET_ADDRESS accepted    setAddress(new); stay enabled; assigned
MOD high after that     stay enabled at the new address
```

## Sequences

A master write is checked in this order:

1. A null buffer, or fewer than `kMinFrameBytes` (3), replies `BadLength`.
2. The length byte must be from 2 through `2 + kMaxPayload`, and the
   buffer size must be that length plus one. Otherwise `BadLength`.
3. CRC-8/SMBus over the bytes the length covers must match the CRC
   byte. Otherwise `BadCrc`, and the address stays as it is.
4. The command runs.

A reply is `kMaxFrameBytes` (19) long. The real frame comes first. The
rest is `0xFF`. `SET_ADDRESS`, accepted or refused as an address, queues
no reply.

On the AVR the handler runs from the TWI interrupt. On the desktop,
`FakeModuleSlavePort::deliver` calls it. It finishes inside
`kClockStretchMaxMs`.

## Commands

| Command | When | Result |
| --- | --- | --- |
| `PING` | empty payload | status `Ok`, empty payload |
| `PING` | any payload | `BadLength` |
| `GET_IDENTITY` | empty payload | type, protocol 1, firmware version |
| `GET_IDENTITY` | any payload | `BadLength` |
| `SET_ADDRESS` | enabled, not yet assigned, one payload byte, address `0x10`–`0x6F` | `setAddress`, no reply |
| `SET_ADDRESS` | payload length is not 1 | `BadLength`, no commit |
| `SET_ADDRESS` | disabled, already assigned, or address outside `0x10`–`0x6F` | no reply, no commit |
| `GET_SENSOR_COUNT` | empty payload | status `Ok`, count `0` |
| `GET_SENSOR_COUNT` | any payload | `BadLength` |
| `GET_SENSOR_CONNECTED`, `GET_SENSOR_READING` | every request | `BadLength` |
| any other command, including `ECHO` | | `UnknownCmd` |

Identity payload, big-endian: type high, type low, `kProtocolVersion`,
firmware high, firmware low. `kIdentityPayloadLen` is 5.

`SET_ADDRESS` commits from the stop callback, before the motherboard
releases MOD. A later `SET_ADDRESS` is ignored. `0x00` is not an
assignable address. `isAssignableAddress` accepts `kMinAssignedAddress`
through `kMaxAssignedAddress`.

## Errors

| Status | Constant | When |
| --- | --- | --- |
| `Ok` | `kStatusOk` (`0x00`) | PING, identity, or sensor count accepted |
| `BadCrc` | `kStatusBadCrc` (`0x01`) | CRC byte does not match |
| `UnknownCmd` | `kStatusUnknownCmd` (`0x02`) | Command this type does not implement |
| `BadLength` | `kStatusBadLength` (`0x03`) | Short frame, length mismatch, or a payload length the command rejects |

`BadLength` is also the reply for presence and reading. This module
has no inputs to report. A refused `SET_ADDRESS` leaves the address in
place and queues no status. The header also defines `Busy` and
`Unsupported`. This link does not send them.

## Configuration

`typeId` and `firmwareVersion` are constructor arguments. The sketch
passes `MODULE_TYPE` and `FIRMWARE_VERSION`, listed on the
[architecture reference](architecture.md). `GET_IDENTITY` reports the
values it was given.

The sensor count is 0 in this module. It is not a `main.cpp` setting.

## Tests

`test/test_desktop/test_module_link.cpp` drives `FakeModuleSlavePort`.

| Test | Path |
| --- | --- |
| `testBootWithModHighLeavesSlaveDisabled` | MOD high at boot leaves the slave off |
| `testModLowEnablesUnconfiguredAddressAndModHighDisables` | `0x0A` follows MOD until assignment; a second call does not enable or disable again |
| `testPingReplyIsOkAndPadded` | `Ok` PING, CRC, 19-byte `0xFF` pad |
| `testIdentityReportsSensorModule` | Type `0x0200`, protocol 1, firmware 1 |
| `testSuppliedFirmwareVersion` | Identity uses the constructor values (`0x1234`, firmware 2) |
| `testSetAddressCommitsAndSurvivesModHigh` | A write while off does not commit. Then commit `0x10`, ignore `0x11`, stay on after MOD rises, and still answer PING |
| `testSetAddressRejectsBadCrcAndIllegalAddresses` | Bad CRC, `0x00`, `0x0A`, and `0x70` stay at `0x0A` |
| `testSensorCountIsZeroAndReadingIsBadLength` | Count 0; reading and presence are `BadLength` |
| `testUnknownCommandAndMalformedFrames` | `ECHO`, truncated frame, extra byte, bad CRC, PING with a payload, empty `SET_ADDRESS` |
| `testEnumerationSequence` | PING, assign `0x12`, MOD high, identity, count 0 |

```text
C:\Users\Nathan\.platformio\penv\Scripts\platformio.exe test -e native
```

## Source

- `lib/Logic/ModuleLink.h`
- `lib/Logic/ModuleLink.cpp`
- `test/test_desktop/test_module_link.cpp`
- `test/test_desktop/fakes/FakeModuleSlavePort.h`
