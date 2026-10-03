#include <Arduino.h>

#include "AvrTwi1Slave.h"
#include "ModuleLink.h"
#include "ModuleProtocol.h"


// ========== Board pins ==========

// Motherboard bus is schematic I2C0. It is the chip's TWI1 peripheral:
// PE0 is SCL and PE1 is SDA. Wire1 owns these pins in hardware.
const uint8_t SCL_PIN = 23;
const uint8_t SDA_PIN = 24;
const uint8_t MOD_PIN = 3;
const uint8_t SNS_PIN = 4;

static_assert(SCL_PIN == 23, "PE0 is motherboard SCL.");
static_assert(SDA_PIN == 24, "PE1 is motherboard SDA.");
static_assert(MOD_PIN == 3, "PD3 is MOD.");
static_assert(SNS_PIN == 4, "PD4 is SNS.");


// ========== Product settings ==========

const uint16_t MODULE_TYPE = module_protocol::kTypeSensorModule;
const uint16_t FIRMWARE_VERSION = 1;


// ========== Modules ==========

AvrTwi1Slave moduleSlave;
ModuleLink moduleLink(moduleSlave, MODULE_TYPE, FIRMWARE_VERSION);


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
 * Gates the unconfigured address on MOD. A low MOD enables 0x0A.
 * After SET_ADDRESS the link keeps that address. This loop does not
 * block, so the host's 5 ms MOD gate is met.
 *
 * @return Nothing.
 */
void loop()
{
  const bool modIsLow = digitalRead(MOD_PIN) == LOW;
  moduleLink.update(modIsLow);
}
