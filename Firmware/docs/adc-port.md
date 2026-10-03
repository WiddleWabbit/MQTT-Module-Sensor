# ADC port

[Home](home.md) · Reference: [ADC port](reference/adc-port.md)

## Summary

`IAds1115` is the seam between the sensor scan and the ADS1115.
`AvrAds1115` is the TWI0 adapter. It uses the Adafruit ADS1X15 library
on `Wire`. Desktop tests use `FakeAds1115`.

## Where it sits

`main.cpp` constructs `AvrAds1115` with `ADS1115_ADDRESS`,
`AvrAds1115::kGainOne`, and `AvrAds1115::kRate860Sps`, and passes it to
`SensorInputs`. The sketch does not call the driver. `SensorInputs`
does.

## What it creates

```text
IAds1115
  AvrAds1115      Adafruit ADS1115 on Wire, TWI0
  FakeAds1115     records calls and returns scripted counts
```

One driver instance owns the chip. The Adafruit object is created on
the first `begin()`, inside the driver translation unit.

## One update

The sketch does not call the driver. One scan step is one of these
calls.

```text
begin()              Wire.begin(), then probe the address
startSingleEnded()   single-shot conversion on one input
conversionReady()    read the OS bit
readCounts()         read the conversion register
```

## What is stored, and who reads it

The driver stores the address, the gain, the data rate, and whether
`begin()` succeeded. The conversion result lives in the ADS1115 until
`readCounts()`. `SensorInputs` copies it into the cache.

`ADC_SDA_PIN` and `ADC_SCL_PIN` name PC4 and PC5. `Wire` takes no pin
argument, because TWI0's pins are fixed.

## See also

- [ADC port reference](reference/adc-port.md) — gain, the OS bit, what the fake records.
- [Sensor inputs](sensor-inputs.md)
- [Architecture](architecture.md)
