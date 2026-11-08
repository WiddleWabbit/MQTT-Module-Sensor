# PCB review — MQTT Current Sensor module

Rev 1.1, 2026-10-09. KiCad 9.0.6. Assembly is by hand. JLCPCB is the bare-board class only.

ERC: 10 items. DRC: 232 violations, 0 unconnected, 0 schematic parity. Sources: `MQTT-Module-CurrentSensors.kicad_sch`, `MQTT-Module-CurrentSensors.kicad_pcb`, `MQTT-Module-CurrentSensors.kicad_pro`.

## Summary

- Board: 44.0 × 55.876 mm, 1.6 mm, 4 layers, 0.035 mm (1 oz) copper on every copper layer. Dielectrics 0.1 / 1.24 / 0.1 mm FR-4. Inner layers are GND pours. Routed copper is on F.Cu (591 mm) and B.Cu (328 mm).
- Smallest via: 0.3 mm drill / 0.6 mm pad, annular ring 0.15 mm. Smallest trace 0.2 mm. Smallest copper space reported by DRC is 0.20 mm (the Signals rule asks for 0.60 mm).
- Counts: 97 through vias (all F.Cu–B.Cu), 24 PTH (1.0 mm on J301/J302, 1.3 mm on J304), 0 NPTH. No via-in-pad, no blind or buried vias.
- Finish unset — green + HASL is the cheap default. Solder mask expansion 0.1 mm, minimum mask web 0.1 mm. Vias tented both sides.
- JLCPCB class: **Multilayer** (snapshot 2026-10-03). Tick 4 layers and inner copper 1 oz (the default inner weight is 0.5 oz). Leave drill at 0.3 mm, thickness 1.6 mm, and finish at the default. No impedance and no via-in-pad.
- Parts: 71 fitted, 0 DNP, 26 unique MPNs. One part has no manufacturer part number (TP601).
- Findings: 1 blocker / 14 warning / 3 unchecked

## Findings

### Pinout and orientation

- **Blocker — U202.** `Firmware/src/main.cpp` lines 12–25 name PE0 as motherboard SCL and PE1 as SDA (`SCL_PIN` 23, `SDA_PIN` 24). The pads are the other way: pin 3 PE0 is `/Controller/SDA1`, pin 6 PE1 is `/Controller/SCL1`. Microchip DS40001906C Table 6-1 and Table 17-12 assign PE0 to SDA1 and PE1 to SCL1. MiniCore `pins_arduino.h` agrees (`PIN_PE0` 23 is `PIN_WIRE_SDA1`, `PIN_PE1` 24 is `PIN_WIRE_SCL1`), and `Wire1` uses those silicon pins. Leave the copper. Change the firmware comments and constants so SDA is 23 and SCL is 24. TWI0 is already right: pin 27 PC4 is SDA0 and pin 28 PC5 is SCL0, matching `ADC_SDA_PIN` 18 and `ADC_SCL_PIN` 19.
- **Warning — D401–D406.** The symbol names both pins A1 and A2, and the footprint Datasheet field and 3D model are the SMF4L (400 W) part. The MPN is SMF28A (200 W, SOD-123FL). Pad 1 is the protected node and pad 2 is GND. The fab cathode bar sits on pad 1, which matches the Littelfuse SMF rule that the colour band is the cathode. Leave the copper. Rename pin 1 to K, pin 2 to A, and point the field at the SMF series sheet.
- **Warning — D301.** The fitted part is 1N5819WS. The Datasheet field points at an SS39 sheet. Pad 1 (K) is +25V and pad 2 (A) is the switch node, which is the boost direction. Chaohe SOD-323 1N5819WS (Dec 2021) Table 2: pin 1 cathode, pin 2 anode. Leave the copper. Replace the datasheet link.
- **Warning — IC401, IC402, IC403, IC404.** Supply pins are type `passive` (IC401 pin 3 GND and pin 8 VDD; each INA186 pin 2 GND and pin 3 VS). ERC cannot see an undriven supply on those pins. The nets are already GND and +3V3. Set the pin types to power input.
- **Note — IC401 pin 2.** ALERT/RDY is open. SBAS444E Table 4-1: open-drain, pull up to VDD only if that output is used.
- **Note — checked and matching.** IC401 ADS1115IDGST matches SBAS444E Figure 4-2 and Table 4-1 (ADDR to GND is 0x48, SBAS444E Table 7-2, same as `ADS1115_ADDRESS`). IC402–IC404 INA186A1IDCKR match SBOS318B Figure 5-1 and Table 5-1: IN+ on the supply end of each shunt, IN− on the load end, gain 25 V/V (Table 6.5). PS301 TPS61170DRVR matches SLVS789D page 3, including the thermal pad to GND. Q401/Q404/Q406 FDN5618P match FDN5618P/D Rev. 4 page 1 and CASE 527AG (pin 1 gate, pin 2 source on +25V, pin 3 drain). Q402/Q403/Q405 MMBT3904LT1G match MMBT3904LT1/D page 1, SOT-23 style 6 (pin 1 base, pin 2 emitter to GND, pin 3 collector). J301 and J302 pin-1 pads are the square pads, and both headers are on B.Cu. L301 and the PTCs are non-polar.

### Electrical

- **Warning — R407, R403, R418 and the firmware scale.** Each shunt is 4.99 Ω 0.5% into an INA186A1 (25 V/V, SBOS318B Table 6.5). 4 mA is 0.499 V (3992 counts). 20 mA is 2.495 V (19960 counts). Both sit inside ADS1115 GAIN_ONE (±4.096 V, 125 µV/count, SBAS444E Table 7-1) and inside the +3V3 input limit. `Firmware/src/main.cpp` lines 41–43 and `Firmware/docs/reference/sensor-inputs.md` lines 65–67 still describe a 150 Ω shunt: 0.600 V / 4800 counts at 4 mA and 3.000 V / 24000 counts at 20 mA. The 2400-count “connected” threshold is about 2.4 mA on this shunt. Update the comment and any host scale, or change the shunt if 150 Ω was the intended gain.
- **Warning — U202 pins 9–11.** SensorSw_1/2/3 can turn the loops on: NPN on, PMOS gate near mid-rail, VGS about −12.5 V, inside the FDN5618P ±20 V gate limit (FDN5618P/D page 1). `setup()` only drives SNS and MOD, so PD5/PD6/PD7 stay inputs and the 47 kΩ base pulldowns hold every loop off. Drive each SensorSw pin high to enable that channel.
- **Warning — J302 pins 1–3 and J301 pin 3.** +12V enters only on J301 pins 1 and 2. `/Headers/+12V_3`, `+12V_4`, `+5V_2`, and `+5V_1` each hit one connector pad and nothing else. ERC `label_dangling` is the spare labels of `+5V_2`, `+12V_4`, and `+12V_3` in `headers.kicad_sch`. Nothing on the board uses 5 V. Tie the extra 12 V pins to +12V if the motherboard feed is shared, or delete the dangling labels and leave the pins open.
- **Warning — #FLG0302 and #FLG0303.** ERC `pin_to_pin`: both power-output flags sit on GND. They are the same rail. `#FLG0301` is +3V3 and `#FLG0601` is +12V. Delete one GND flag. ERC `multiple_net_names` is the same copper: +3V3 wins over VCC and +3V3_1, and +12V wins over +12V_1.
- **Warning — +12V, +3V3, +25V.** Classes 12V (1.5 mm), 3V3 (0.8 mm), and 5V (1.0 mm) exist in the project file and have no patterns. Patterns assign only `*SDA*`, `*SCL*`, `*MOSI`, `*MISO`, `*SCK`, and `*GND`. Measured minimum width is 0.2 mm on +12V and +3V3, and 0.25 mm on +25V. Three 20 mA loops are about 60 mA on the 25 V rail and on the order of 150 mA at the 12 V input, so 0.2 mm of 1 oz copper carries the current. Add patterns if those class widths were meant to be enforced.
- **Note — PS301.** CTRL and VIN are both +12V, so the boost runs whenever 12 V is present. SLVS789D §9.2.2.1: Vout = 1.229 × (1 + 200 kΩ / 10 kΩ) = 25.8 V. Output capacitors are C307 4.7 µF 50 V and C309 10 µF 35 V. The SMF28A devices sit on the sense nodes, not across the rail. Standoff is 28 V and breakdown is 31.1–34.4 V, so they stay off at the setpoint.
- **Note — +3V3.** Not generated on this board. J301 pin 4 and J302 pin 4 are the same +3V3 net. Local 0.1 µF capacitors are C201 and C203 at U202, C407 at IC401, and C401–C403 at the three INA186 supplies. R201 10 kΩ pulls RESET to +3V3. AREF is only C204 to GND, which matches an unused AVR ADC. The internal 8 MHz oscillator is selected in `Firmware/platformio.ini`, so the open XTAL pins are expected.

### Fabrication

- **Warning — Signals netclass.** DRC `clearance` ×139, all against netclass Signals 0.6000 mm, actual 0.2000–0.5800 mm. Tightest pair: IC401 pad 9 (SDA0) to a +3V3 track, 0.2000 mm. The VSSOP and TQFP pad gaps cannot meet 0.6 mm. 0.20 mm is still above the JLCPCB 0.127 mm trace/space line, so this does not change the board class. Relax the Signals clearance (0.2 mm matches the Default class) or accept the DRC.
- **Warning — IC402, IC403, IC404.** DRC `solder_mask_bridge` ×9. On each SC70-6 the mask opening joins pad 2 (GND) with pad 3 (+3V3), and also joins pads 4–5 and 5–6. Board mask expansion is 0.1 mm and the minimum web is 0.1 mm. Reduce the expansion on these footprints before fab, and watch those pins while hand soldering.
- **Warning — +25V.** DRC `via_dangling` ×3, each a through via with copper on one layer only. Unconnected-item count is 0, so the net is still routed. Delete the stubs or tie them into the pour.
- **Warning — PS301.** DRC `lib_footprint_mismatch`: the board footprint `SON65P200X200X80-7N` differs from the current library copy. The pad nets still match SLVS789D page 3. Update the footprint from the library only after a pad-by-pad diff.
- **Note — order.** Class Multilayer. Non-default ticks are 4 layers and 1 oz inner copper. Smallest via drill is already 0.3 mm, so leave the 0.2 mm drill option off. No castellations, no impedance, no via-in-pad. PTH annular ring is 0.325 mm minimum (J304, 1.3 mm drill in a 1.95 mm pad).

### Assembly

Hand assembly. JLCPCB assembly-library checks were skipped.

- **Warning — D401–D406, D301, C309, TP601.** DRC `silk_over_copper` ×7 (D401–D406 and D301) and `silk_overlap` ×4 (D404/D405, D401/D402, D403/D406, C309/TP601). The TVS cathode triangle is the polarity mark, and part of it is clipped by the mask opening. Confirm the band against the fab triangle on pad 1 before soldering.
- **Warning — silk text.** DRC `text_height` ×69. Board setup minimum silk height is 1.0 mm. Actual height is 0.8 mm (reference fields such as C302 and F401). Raise the silk to 1.0 mm or lower the project rule before treating DRC as clean.
- **Note — polarity and side.** C309 pin 1 (“+”) is +25V and pin 2 is GND. J301 and J302 are on the back (B.Cu), pin 1 at the higher-Y end, square pad. No parts are marked DNP.

## Unchecked

- **J304 (Phoenix 1715747).** Symbol order matches the pads, and pad 1 is the square pad (GND). The product drawing does not say which end of the housing is circuit 1, so the cable-end pin-1 side was not confirmed.
- **J301 and J302 mating.** Pin nets on this board were checked. No motherboard connector file in this project was compared, so the mate’s pin 1 and signal order are unchecked.
- **Blank Datasheet field, or no MPN.** Add a link, or drop a PDF in `docs/datasheets/`, before the next run.
  - No MPN: TP601 (test point).
  - Datasheet field empty: R201, R301, R303, R304, R401, R402, R404, R406, R408, R409, R410, R411, R413, R414, R415, R416, R417, R419, R421, R422, R423, C307, C308, C404, C405, C406, TP601.
