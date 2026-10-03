# Sensor inputs reference

[Home](../home.md) · Guide: [Sensor inputs](../sensor-inputs.md)

## Purpose

Scan the ADS1115 inputs and remember the latest counts. The link
reports that cache. The scan is a sequence of single driver calls so
`loop()` can still see MOD within `kModGateMaxMs`.

## Classes

`SensorInputs(IAds1115& adc, uint8_t count, int16_t connectedMinCounts)`
binds the port. Copy and assignment are deleted.

`update()` advances one step. `count()`, `hasSample(index)`,
`connected(index)`, and `counts(index)` are what the link reads. An
index outside the count has no sample, is not connected, and reads as
0 counts.

A count above `kMaxSensorsPerModule` (16) is stored as 16.

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

## Sequences

Publishing one sample clears that input's valid flag, writes the
counts and the connected flag, then sets the valid flag. The connected
flag is 1 when the signed counts are greater than or equal to
`connectedMinCounts`.

Index 0 is AIN0 (Sens1), index 1 is AIN1 (Sens2), and index 2 is AIN2
(Sens3).

## Errors

A missing chip keeps `begin()` false. No input has a sample, and the
link replies `Busy` for presence and reading. The count is still
reported.

A start that fails does not erase a sample already stored for another
input. The scan retries the channel that failed.

`counts()` for an input with no sample returns 0. The link treats
"no sample" as `Busy` rather than as a reading of 0.

## Configuration

The sketch passes `SENSOR_COUNT` (3) and
`SENSOR_CONNECTED_MIN_COUNTS` (2400). 2400 counts is 0.300 V at
GAIN_ONE, 125 uV per count, which is 2 mA through the 150 ohm shunt.
A 4 mA loop is 0.600 V, 4800 counts. A 20 mA loop is 3.000 V, 24000
counts. An open input sits near 0 V.

The gain and the sample rate are arguments to `AvrAds1115`, listed on
the [architecture reference](architecture.md).

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
| `testSensorCommandsReportCachedReadings` | Count 3. `Busy` before a sample. Then 2399 disconnected, 2400 connected, and -4 disconnected, each on the wire |
| `testSensorCommandsRejectBadIndexAndLength` | Index 3, a short or long payload, a count payload, and index 0 when the count is 0 are `BadLength` |

```text
C:\Users\Nathan\.platformio\penv\Scripts\platformio.exe test -e native
```

## Source

- `lib/Logic/SensorInputs.h`
- `lib/Logic/SensorInputs.cpp`
- `test/test_desktop/test_sensor_inputs.cpp`
- `test/test_desktop/fakes/FakeAds1115.h`
