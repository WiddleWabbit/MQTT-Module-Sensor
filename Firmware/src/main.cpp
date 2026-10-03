#include <Arduino.h>

#include "AvrAds1115.h"
#include "AvrTwi1Slave.h"
#include "ModuleLink.h"
#include "ModuleProtocol.h"
#include "SensorInputs.h"


// ========== Board pins ==========

// Motherboard bus is schematic I2C0. It is the chip's TWI1 peripheral:
// PE0 is SCL and PE1 is SDA. Wire1 owns these pins in hardware.
const uint8_t SCL_PIN = 23;
const uint8_t SDA_PIN = 24;
const uint8_t MOD_PIN = 3;
const uint8_t SNS_PIN = 4;

// ADC bus is schematic I2C1. It is the chip's TWI0 peripheral.
// PC4 is SDA and PC5 is SCL. Wire owns these pins in hardware.
const uint8_t ADC_SDA_PIN = 18;
const uint8_t ADC_SCL_PIN = 19;

static_assert(SCL_PIN == 23, "PE0 is motherboard SCL.");
static_assert(SDA_PIN == 24, "PE1 is motherboard SDA.");
static_assert(MOD_PIN == 3, "PD3 is MOD.");
static_assert(SNS_PIN == 4, "PD4 is SNS.");
static_assert(ADC_SDA_PIN == 18, "PC4 is ADC SDA.");
static_assert(ADC_SCL_PIN == 19, "PC5 is ADC SCL.");


// ========== Product settings ==========

const uint16_t MODULE_TYPE = module_protocol::kTypeSensorModule;
const uint16_t FIRMWARE_VERSION = 1;

// ADDR is strapped to GND on this board.
const uint8_t ADS1115_ADDRESS = 0x48;
const uint8_t SENSOR_COUNT = 3;

// 2400 counts is 0.300 V at GAIN_ONE (125 uV per count): 2 mA through
// the 150 ohm shunt. A live 4 mA loop sits above this. An open input does not.
const int16_t SENSOR_CONNECTED_MIN_COUNTS = 2400;


// ========== Modules ==========

AvrTwi1Slave moduleSlave;
AvrAds1115 ads(ADS1115_ADDRESS, AvrAds1115::kGainOne,
               AvrAds1115::kRate860Sps);
SensorInputs sensorInputs(ads, SENSOR_COUNT, SENSOR_CONNECTED_MIN_COUNTS);
ModuleLink moduleLink(moduleSlave, sensorInputs, MODULE_TYPE, FIRMWARE_VERSION);


// ========== Application ==========

/**
 * Drives SNS low so the motherboard sees this module seated, and holds
 * MOD as an input with pull-up. The slave stays off until MOD is low.
 *
 * @return Nothing.
 */
void setup()
{
  pinMode(SNS_PIN, OUTPUT);
  digitalWrite(SNS_PIN, LOW);
  pinMode(MOD_PIN, INPUT_PULLUP);
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
