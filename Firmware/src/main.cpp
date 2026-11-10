#include <Arduino.h>

#include "AvrAds1115.h"
#include "AvrInterruptMask.h"
#include "AvrSensorSwitchPort.h"
#include "AvrTwi1Slave.h"
#include "ModuleLink.h"
#include "ModuleProtocol.h"
#include "SensorInputs.h"


// ========== Board pins ==========

// Motherboard bus is schematic I2C1. It is the chip's TWI1 peripheral:
// PE0 is SDA and PE1 is SCL. Wire1 owns these pins in hardware.
const uint8_t SDA_PIN = 23;
const uint8_t SCL_PIN = 24;
const uint8_t MOD_PIN = 3;
const uint8_t SNS_PIN = 4;

// SensorSw drives the NPN that turns on that channel's high-side PMOS.
// High closes the loop. The base pulldown holds it open until a set-on.
const uint8_t SENSOR_SW1_PIN = 5;
const uint8_t SENSOR_SW2_PIN = 6;
const uint8_t SENSOR_SW3_PIN = 7;

// ADC bus is schematic I2C0. It is the chip's TWI0 peripheral.
// PC4 is SDA and PC5 is SCL. Wire owns these pins in hardware.
const uint8_t ADC_SDA_PIN = 18;
const uint8_t ADC_SCL_PIN = 19;

static_assert(SDA_PIN == 23, "PE0 is motherboard SDA.");
static_assert(SCL_PIN == 24, "PE1 is motherboard SCL.");
static_assert(MOD_PIN == 3, "PD3 is MOD.");
static_assert(SNS_PIN == 4, "PD4 is SNS.");
static_assert(SENSOR_SW1_PIN == 5, "PD5 is SensorSw_1.");
static_assert(SENSOR_SW2_PIN == 6, "PD6 is SensorSw_2.");
static_assert(SENSOR_SW3_PIN == 7, "PD7 is SensorSw_3.");
static_assert(ADC_SDA_PIN == 18, "PC4 is ADC SDA.");
static_assert(ADC_SCL_PIN == 19, "PC5 is ADC SCL.");


// ========== Product settings ==========

const uint16_t MODULE_TYPE = module_protocol::kTypeSensorModule;

// Identity firmware bytes are board revision, then firmware revision.
const uint8_t BOARD_REVISION = 2;
const uint8_t FIRMWARE_REVISION = 1;
const uint16_t FIRMWARE_VERSION =
    static_cast<uint16_t>((static_cast<uint16_t>(BOARD_REVISION) << 8) |
                          FIRMWARE_REVISION);

// ADDR is strapped to GND on this board.
const uint8_t ADS1115_ADDRESS = 0x48;
const uint8_t SENSOR_COUNT = 3;

// 2400 counts is 0.300 V at GAIN_ONE (125 uV per count). On the
// 4.99 ohm shunt and INA186A1 (25 V/V) that is about 2.4 mA. A live
// 4 mA loop is 3992 counts. An open input sits below this.
const int16_t SENSOR_CONNECTED_MIN_COUNTS = 2400;

// 21956 counts is 2.745 V, 22 mA on that same front end. Two stored
// samples at or above this open the loop and latch a fault.
const int32_t SENSOR_TRIP_COUNTS = 21956;
const uint8_t SENSOR_TRIP_SAMPLES = 2;


// ========== Modules ==========

AvrTwi1Slave moduleSlave;
AvrAds1115 ads(ADS1115_ADDRESS, AvrAds1115::kGainOne,
               AvrAds1115::kRate860Sps);
AvrSensorSwitchPort sensorSwitches(SENSOR_SW1_PIN, SENSOR_SW2_PIN,
                                   SENSOR_SW3_PIN);
AvrInterruptMask interruptMask;
SensorInputs sensorInputs(ads, sensorSwitches, interruptMask, SENSOR_COUNT,
                          SENSOR_CONNECTED_MIN_COUNTS, SENSOR_TRIP_COUNTS,
                          SENSOR_TRIP_SAMPLES);
ModuleLink moduleLink(moduleSlave, sensorInputs, MODULE_TYPE, FIRMWARE_VERSION);


// ========== Application ==========

/**
 * Drives SNS low so the motherboard sees this module seated, drives
 * each SensorSw pin low so every loop stays open, and holds MOD as an
 * input with pull-up. The slave stays off until MOD is low. A loop
 * closes only after SET_SENSOR on.
 *
 * @return Nothing.
 */
void setup()
{
  pinMode(SNS_PIN, OUTPUT);
  digitalWrite(SNS_PIN, LOW);
  pinMode(MOD_PIN, INPUT_PULLUP);
  sensorSwitches.begin();
}

/**
 * Advances the ADS1115 scan one step, then gates the unconfigured
 * address on MOD. A low MOD enables 0x0A. After SET_ADDRESS the link
 * keeps that address. This loop does not block, so the host's 5 ms
 * MOD gate is met.
 *
 * @return Nothing.
 */
void loop()
{
  sensorInputs.update();
  const bool modIsLow = digitalRead(MOD_PIN) == LOW;
  moduleLink.update(modIsLow);
}
