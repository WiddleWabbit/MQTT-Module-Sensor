# Firmware architecture

[Home](home.md) · Reference: [Architecture](reference/architecture.md)

## Summary

This firmware runs on the ATmega328PB sensor daughterboard. The ESP32
motherboard is the I2C master. This board is a slave. Plugging it in is
enough for the motherboard to assign an address and identify a sensor
module with no inputs yet.

`src/main.cpp` is the composition root. It owns the pin constants and
the product settings, constructs the TWI adapter and the link, and
calls `update()`.

## Where it sits

Global construction runs before `setup()`. `ModuleLink` registers its
master-write handler there. The slave stays off until `loop()` sees
MOD low.

The motherboard bus is schematic I2C0. On this chip that is TWI1,
Arduino `Wire1`. The ADC bus is schematic I2C1, TWI0 on PC4 and PC5.
This firmware leaves that bus unused.

## What it creates

```text
main.cpp
  AvrTwi1Slave     TWI1 adapter for IModuleSlavePort
  ModuleLink       MOD gate, frames, and replies
```

`main.cpp` passes `MODULE_TYPE` and `FIRMWARE_VERSION` into the link.
Pin numbers stay in `main.cpp`.

`lib/Interfaces/` holds `IModuleSlavePort` and the copied
`ModuleProtocol.h`. `lib/Drivers/` holds the AVR adapter. `lib/Logic/`
holds the link. Desktop fakes live in `test/test_desktop/fakes/`.

## One update

`setup()` drives `SNS_PIN` low so the motherboard sense line says the
module is seated, and holds `MOD_PIN` as an input with pull-up.

```mermaid
flowchart TD
  loop["loop()"] --> read["digitalRead MOD_PIN"]
  read --> upd["moduleLink.update(modIsLow)"]
```

`loop()` does not block, so a low MOD is seen inside `kModGateMaxMs`.
While the module has no address, MOD low enables `0x0A` and MOD high
disables it. After a valid `SET_ADDRESS`, the link keeps that address
when MOD rises.

## What is stored, and who reads it

Each pass reads `MOD_PIN` and passes that level to the link. The
motherboard reads the sense line, which `SNS_PIN` holds low.

The link stores the address and whether it has been assigned. The TWI
adapter stores the reply the host clocks out. Those stores are on the
[module link](module-link.md) and [slave port](slave-port.md) pages.

## See also

- [Architecture reference](reference/architecture.md) — pins, the pass, the image build.
- [Module link](module-link.md)
- [Slave port](slave-port.md)
