# Sensor inputs reference

[Home](../home.md) · Guide: [Sensor inputs](../sensor-inputs.md)

## Purpose

Scan the ADS1115 inputs, remember the latest counts, and own each
sensor's loop switch. The link reports that cache and the state. The
scan is a sequence of single driver calls so `loop()` can still see
MOD within `kModGateMaxMs`.

## Classes

`SensorInputs(IAds1115& adc, ISensorSwitchPort& switches, IInterruptMask& mask, IEeprom& eeprom, uint8_t count, int16_t connectedMinCounts, int32_t tripCounts, uint8_t tripSamples)`
binds the ports. Copy and assignment are deleted. The constructor
reads the fault image and does not write the switch pins.

`update()` polls the fault image, then advances one scan step.
`count()`, `hasSample(index)`,
`connected(index)`, `counts(index)`, `stateReady(index)`,
`state(index)`, and `readFault(dest)` are what the link reads.
`turnOn(index)`, `turnOff(index)`, and `reset(index)` are what set,
and reset, call.
An index outside the count has no sample, is not connected, reads as
0 counts, and the switch calls do nothing.

`connected(index)` is true only when the state is on.

A count above `kMaxSensorsPerModule` (16) is stored as 16.

`AvrSensorSwitchPort` drives `SENSOR_SW1_PIN`, `SENSOR_SW2_PIN`, and
`SENSOR_SW3_PIN`. `begin()` drives them low, then makes them outputs.
`FakeSensorSwitchPort` records the same calls. `AvrInterruptMask`
saves the global interrupt flag around a phase change.
`FakeInterruptMask` does nothing. `AvrEeprom` arms one ATmega328PB
EEPROM byte and returns. `FakeEeprom` keeps that byte pending until
`poll`.

## States

```text
count 0                 update returns; the ADC is not started
not begun               the next update calls begin()
Starting                the next update calls startSingleEnded(channel)
Waiting                 the next update calls conversionReady()
Reading                 the next update calls readCounts() and stores it
```

`begin()` returning false leaves the scan not begun, so the next pass
tries again. `startSingleEnded` returning false leaves the phase at
`Starting` on the same channel. The previous sample stays. A ready
poll that is false stays in `Waiting`.

After a stored sample the channel advances by one and wraps to 0.

```text
off                     switch open. Blank image at boot, set off, and reset end here
on, waiting             switch closed, no sample stored since the close
on                      switch closed, latest sample at or above the connected threshold and below a latched fault
disconnected            switch closed, latest sample since the close is below the connected threshold
fault                   switch open. Two consecutive samples at or above the trip, or a stored pair that is not blank. Stays until reset
```

Fault wins over a later low sample. Off wins over a sample that was
stored while the loop was closed. Set on and set off do nothing while
faulted. Set on while already on leaves the strikes and the sample.

`readFault` writes one record per input, in index order, and returns
the number of bytes. Each record is the reason, then the index.
Reason 0 is not latched. A latched trip is `kSensorFaultOvercurrent`
(1). Off, on, disconnected, and a single strike are reason 0. A
count of 0 writes nothing. One mask covers the whole list.

## Sequences

Publishing one sample clears that input's valid flag, writes the
counts, then sets the valid flag. The mask is held across that store
and any pin write. The ADC call itself stays outside the mask.

While the loop is closed, that sample sets the fresh flag. A sample
at or above `tripCounts` adds one strike. Any lower sample clears the
strikes. `tripSamples` strikes in a row latch fault and open the
switch. Samples taken while the loop is open do not strike. Polls
that do not store a conversion do not strike.

Index 0 is AIN0 (Sens1), index 1 is AIN1 (Sens2), and index 2 is AIN2
(Sens3). The same index is that sensor's switch.

The fault image starts at EEPROM address 0. Input `n` uses bytes
`2n` and `2n + 1`. This board's count is 3, so the image is 6 bytes.

```text
0xFF, 0xFF    no fault. A blank chip matches this
0x01, 0xFE    overcurrent. 0xFE is the complement of 0x01
any other     fault
```

The constructor applies that before `setup()`. `reset()` clears the
live latch before it returns. `update()` then programs the stored
pair, one byte per pass, and does not wait for the 3.4 ms program.
A new latch arms `0xFE` first, then `0x01`. A clear arms `0xFF` at
the reason byte first, then `0xFF` at the complement. A restored
pair that is not already `01 FE` is rewritten the same way as a
new latch. A pair left half-written boots as fault. The TWI handler
does not arm a byte.

## Errors

A missing chip keeps `begin()` false. No input has a sample, and the
link replies `Busy` for presence and reading. The count is still
reported.

A start that fails does not erase a sample already stored for another
input. The scan retries the channel that failed.

`counts()` for an input with no sample returns 0. The link treats
"no sample" as `Busy` rather than as a reading of 0.

## Configuration

The sketch passes `SENSOR_COUNT` (3),
`SENSOR_CONNECTED_MIN_COUNTS` (2400), `SENSOR_TRIP_COUNTS` (21956),
and `SENSOR_TRIP_SAMPLES` (2).

2400 counts is 0.300 V at GAIN_ONE, 125 µV per count. On the fitted
4.99 Ω shunt and INA186A1 (25 V/V) that is about 2.4 mA. A 4 mA loop
is 0.499 V, 3992 counts. A 20 mA loop is 2.495 V, 19960 counts. The
trip, 21956 counts, is 2.745 V, 22 mA. An open input sits near 0 V.
The INA186 linear window ends near 3.2 V. The PTC on each loop is the
short-circuit fuse. 22 mA does not open it.

The gain and the sample rate are arguments to `AvrAds1115`, listed on
the [architecture reference](architecture.md). The fault image uses
the EEPROM kept by the `fuses_bootloader` EEPROM-save fuse.

## Tests

`test/test_desktop/test_sensor_inputs.cpp` drives `FakeAds1115`.
`test/test_desktop/test_module_link.cpp` checks the frames the link
builds from the same cache.

| Test | Path |
| --- | --- |
| `testScanStoresEachChannelThenWraps` | Channel 0, then 1, then 2, then channel 0 again. 2399 is disconnected, 2400 is connected, and a negative code stays negative. A new conversion replaces channel 0 |
| `testBeginFailureRetriesAndDoesNotStart` | `begin()` false retries and does not start a conversion |
| `testStartFailureKeepsTheLastSampleAndRetriesTheChannel` | A refused start keeps channel 0 and retries channel 1 |
| `testWaitingDoesNotReadOrAdvance` | A conversion that is not ready is polled, not read |
| `testZeroCountDoesNotTouchTheAdc` | Count 0 does not call the ADC |
| `testCountAboveProtocolMaxIsClamped` | A count of 17 is stored as 16 |
| `testOffOnDisconnectedAndReset` | Boot is off and the switch stays open. On waits. 2399 is disconnected, 2400 is on, and a later 2399 is disconnected. Off and reset open the switch. A second on while disconnected leaves it closed |
| `testTwoHighSamplesLatchFaultOnThatChannelOnly` | One sample at 21956 stays on. Waits do not add a strike. The second sample faults channel 0 and leaves channel 1 on. A later sample does not write the pin again |
| `testLowSampleClearsTheStrike` | A sample below 21956 clears the strike, so the next single high sample stays on |
| `testHighSampleWhileOffDoesNotTrip` | Two high samples while open stay off. An index of 4 does not write the switch |
| `testFaultIgnoresOnAndOffUntilReset` | On and off while faulted leave the latch. A low sample stays fault. Reset returns to off, and a later on accepts 4800 |
| `testBlankEepromBootsOff` | A blank image boots off. Reset programs nothing |
| `testLatchedFaultRestoresAndStaysOpen` | Overcurrent on input 1 boots as fault, refuses set on, and leaves input 0 able to close. `readFault` is `00 00 01 01 00 02` |
| `testCorruptPairRestoresAsFault` | `01 00` boots as fault and the blank sibling stays off. `00 FF` boots as fault |
| `testTornPairBootsFault` | `FF FE` boots as fault and set on leaves the switch open |
| `testLatchWritesInverseBeforeReason` | RAM faults before a byte is armed. The next byte is `FE` at address 1. A held program blocks the reason byte. The committed pair boots as fault |
| `testResetClearsRamImmediatelyThenBlanksThePair` | Reset reports off while `01 FE` is still stored, and set on is accepted. The next bytes are `FF` at address 0, then `FF` at address 1. That image boots off |
| `testResetOfAClearInputDoesNotProgram` | Reset of a blank input, and a second reset after a committed clear, arm no byte |
| `testSecondLatchLeavesTheFirstImage` | Latching input 2 programs addresses 5 then 4. Input 0 keeps `01 FE` |
| `testResetThenLatchKeepsTheLatestReason` | A clear reversed by a new latch finishes as `01 FE` and boots as fault |
| `testBusyEepromDoesNotStallTheScan` | A held program still lets `begin` and `startSingleEnded` run |
| `testSensorCommandsReportCachedReadings` | Count 3. `Busy` before a sample. Loops closed, then 2399 disconnected, 2400 connected, and -4 disconnected, each on the wire |
| `testSensorStateCommands` | Get off, set on then `Busy`, disconnected, on, off, fault, ignored set, reset, and a later on. Bad index, fault or disconnected as a set value, and a wrong length are `BadLength` |
| `testGetFaultWhenIdle` | Three clear inputs are `00 00 00 01 00 02`. A count of 0 is an empty `Ok` |
| `testGetFaultReportsEveryInput` | One trip sample stays clear. The second sample on input 2 makes `00 00 00 01 01 02`. A disconnected input stays reason 0. Faulting input 0 makes `01 00 00 01 01 02`. Reset of 0, then of 2, clears those records |
| `testGetFaultRejectsPayload` | A payload on `GET_FAULT` is `BadLength`. The latched record stays `01 00` and the switch stays open |
| `testGetFaultRejectsTooManyInputs` | Eight inputs fill the payload. Nine are `Unsupported` |
| `testSensorCommandsRejectBadIndexAndLength` | Index 3, a short or long payload, a count payload, and index 0 when the count is 0 are `BadLength` |

```text
C:\Users\Nathan\.platformio\penv\Scripts\platformio.exe test -e native
```

## Source

- `lib/Logic/SensorInputs.h`
- `lib/Logic/SensorInputs.cpp`
- `lib/Interfaces/ISensorSwitchPort.h`
- `lib/Interfaces/IInterruptMask.h`
- `lib/Interfaces/IEeprom.h`
- `lib/Drivers/AvrSensorSwitchPort.h`
- `lib/Drivers/AvrSensorSwitchPort.cpp`
- `lib/Drivers/AvrInterruptMask.h`
- `lib/Drivers/AvrInterruptMask.cpp`
- `lib/Drivers/AvrEeprom.h`
- `lib/Drivers/AvrEeprom.cpp`
- `test/test_desktop/test_sensor_inputs.cpp`
- `test/test_desktop/fakes/FakeAds1115.h`
- `test/test_desktop/fakes/FakeSensorSwitchPort.h`
- `test/test_desktop/fakes/FakeInterruptMask.h`
- `test/test_desktop/fakes/FakeEeprom.h`
