# Module link reference

[Home](../home.md) · Guide: [Module link](../module-link.md)

## Purpose

Slave side of the module protocol for one sensor module. It accepts an
address while MOD is low, then identifies as the type and firmware
version it was given, with protocol version 1. Sensor commands read the `SensorInputs` cache and its loop state.

Frame layout, CRC-8/SMBus, and command bytes are in
`lib/Interfaces/ModuleProtocol.h`. If this page and that header
disagree, the header wins.

## Classes

`ModuleLink(IModuleSlavePort& port, SensorInputs& sensors, uint16_t typeId, uint16_t firmwareVersion)`
binds the port and the sensor cache, and registers the master-write
handler. The slave stays off. Copy and assignment are deleted.

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
| `GET_FAULT` | empty payload, and one record per input fits in `kMaxPayload` | `Ok` and one record per input, in index order. Each record is reason, then index. Reason 0 is not latched. A latched trip is `kSensorFaultOvercurrent` (1). A count of 0 is an empty payload |
| `GET_FAULT` | the list would pass `kMaxPayload` | `Unsupported`. No partial list |
| `GET_FAULT` | any payload | `BadLength`. The loops are unchanged |
| `SET_ADDRESS` | enabled, not yet assigned, one payload byte, address `0x10`–`0x6F` | `setAddress`, no reply |
| `SET_ADDRESS` | payload length is not 1 | `BadLength`, no commit |
| `SET_ADDRESS` | disabled, already assigned, or address outside `0x10`–`0x6F` | no reply, no commit |
| `GET_SENSOR_COUNT` | empty payload | status `Ok`, the cache's count |
| `GET_SENSOR_COUNT` | any payload | `BadLength` |
| `GET_SENSOR_CONNECTED` | one index byte, index inside the count, sample stored | `Ok`, index, connected `0` or `1` |
| `GET_SENSOR_READING` | one index byte, index inside the count, sample stored | `Ok`, index, connected, raw counts as big-endian `int32` |
| `GET_SENSOR_CONNECTED`, `GET_SENSOR_READING` | index inside the count, no sample yet | `Busy` |
| `GET_SENSOR_CONNECTED`, `GET_SENSOR_READING` | payload length is not 1, or index at or above the count | `BadLength` |
| `GET_SENSOR_STATE` | one index byte, index inside the count, off, fault, or a sample stored since the loop closed | `Ok`, index, state |
| `GET_SENSOR_STATE` | loop closed and no sample stored since that close | `Busy` |
| `SET_SENSOR` | index and `off` or `on`, index inside the count | the resulting state, or `Busy` when the loop just closed |
| `SET_SENSOR` | state byte is `fault` or `disconnected`, wrong length, or index outside the count | `BadLength`, no change |
| `RESET_SENSOR` | one index byte, index inside the count | `Ok`, index, `off` |
| `GET_SENSOR_STATE`, `SET_SENSOR`, `RESET_SENSOR` | payload length does not match, or index at or above the count | `BadLength` |
| any other command, including `ECHO` | | `UnknownCmd` |

Identity payload, big-endian: type high, type low, `kProtocolVersion`,
firmware high, firmware low. `kIdentityPayloadLen` is 5. The high
firmware byte is the board revision and the low byte is the firmware
revision. The sketch values are on the
[architecture reference](architecture.md).

`SET_ADDRESS` commits from the stop callback, before the motherboard
releases MOD. A later `SET_ADDRESS` is ignored. `0x00` is not an
assignable address. `isAssignableAddress` accepts `kMinAssignedAddress`
through `kMaxAssignedAddress`.

## Errors

| Status | Constant | When |
| --- | --- | --- |
| `Ok` | `kStatusOk` (`0x00`) | PING, identity, `GET_FAULT`, sensor count, or a stored sample |
| `BadCrc` | `kStatusBadCrc` (`0x01`) | CRC byte does not match |
| `UnknownCmd` | `kStatusUnknownCmd` (`0x02`) | Command this type does not implement |
| `BadLength` | `kStatusBadLength` (`0x03`) | Short frame, length mismatch, a payload length the command rejects, or a sensor index outside the count |
| `Busy` | `kStatusBusy` (`0x04`) | Presence or reading for an input that has no stored sample yet, or a state request while a closed loop is waiting for its next sample |
| `Unsupported` | `kStatusUnsupported` (`0x05`) | `GET_FAULT` when one record per input would pass `kMaxPayload` |

A refused `SET_ADDRESS` leaves the address in place and queues no
status. The reading payload is `kSensorReadingPayloadLen` (6): index,
connected, then the raw counts. The connected payload is
`kSensorConnectedPayloadLen` (2): index, then 0 or 1. The count
payload is one byte.

## Configuration

`typeId` and `firmwareVersion` are constructor arguments. The sketch
passes `MODULE_TYPE` and `FIRMWARE_VERSION`, listed on the
[architecture reference](architecture.md). `FIRMWARE_VERSION` packs
the board revision in the high byte and the firmware revision in the
low byte. `GET_IDENTITY` reports the values it was given.

The `GET_FAULT` bytes come from `SensorInputs::readFault`. Each
record is `kFaultRecordLen` (2). Eight inputs fill `kMaxPayload`.
Which reason an input gets is on the
[sensor inputs reference](sensor-inputs.md).

The sensor count, the connected threshold, the trip, and the raw
counts come from `SensorInputs`. The sketch passes `SENSOR_COUNT`,
`SENSOR_CONNECTED_MIN_COUNTS`, `SENSOR_TRIP_COUNTS`, and
`SENSOR_TRIP_SAMPLES` into that object. The reading on the wire is
the ADS1115 code, not a scaled current. The connected byte is 1 only
while the state is on.

State bytes are `kSensorStateOff` (0), `kSensorStateOn` (1),
`kSensorStateFault` (2), and `kSensorStateDisconnected` (3). A fault
ignores set on and set off. Reset clears the live latch before the
reply is queued and leaves that sensor off. The stored pair catches
up from the scan. Set accepts only off or on.

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
| `testSensorCommandsReportCachedReadings` | Count 3. `Busy` before a sample. Loops closed, then 2399 is disconnected, 2400 is connected, and -4 is disconnected. Channel 0 still reads 2399 after the others are stored |
| `testSensorStateCommands` | Off, set on then `Busy`, disconnected, on, off, two trip samples then fault, set ignored while faulted, reset, and a later on. Bad index, a fault or disconnected set value, and a wrong length stay open |
| `testGetFaultWhenIdle` | Three clear inputs are `00 00 00 01 00 02`. A count of 0 is an empty `Ok` |
| `testGetFaultReportsEveryInput` | One trip sample stays clear. The second sample on input 2 makes `00 00 00 01 01 02`. A disconnected input stays reason 0. Faulting input 0 makes `01 00 00 01 01 02`. Reset of 0, then of 2, clears those records |
| `testGetFaultRejectsPayload` | A payload on `GET_FAULT` is `BadLength`. The latched record stays `01 00` and the switch stays open |
| `testGetFaultRejectsTooManyInputs` | Eight inputs fill the payload. Nine are `Unsupported` |
| `testRestoredFaultIsReportedAndIgnoresSetOn` | A stored overcurrent on input 1 is fault on `GET_SENSOR_STATE` and `GET_FAULT`. Set on leaves that switch open |
| `testSensorCommandsRejectBadIndexAndLength` | Index 3, a short or long reading payload, a count payload, and index 0 on a count of 0 are `BadLength` |
| `testUnknownCommandAndMalformedFrames` | `ECHO`, truncated frame, extra byte, bad CRC, PING with a payload, empty `SET_ADDRESS` |
| `testEnumerationSequence` | PING, assign `0x12`, MOD high, identity, count 3 |

```text
C:\Users\Nathan\.platformio\penv\Scripts\platformio.exe test -e native
```

## Source

- `lib/Logic/ModuleLink.h`
- `lib/Logic/ModuleLink.cpp`
- `test/test_desktop/test_module_link.cpp`
- `test/test_desktop/fakes/FakeModuleSlavePort.h`
