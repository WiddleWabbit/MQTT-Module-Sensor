#include "AvrSensorSwitchPort.h"

#if defined(__AVR_ATmega328PB__)

#include <Arduino.h>


// ========== Construction ==========

/**
 * Stores the pins. The levels are applied in begin().
 *
 * @param pin0 SensorSw_1. SENSOR_SW1_PIN in main.cpp.
 * @param pin1 SensorSw_2. SENSOR_SW2_PIN in main.cpp.
 * @param pin2 SensorSw_3. SENSOR_SW3_PIN in main.cpp.
 */
AvrSensorSwitchPort::AvrSensorSwitchPort(uint8_t pin0, uint8_t pin1,
                                         uint8_t pin2)
  : _pins{pin0, pin1, pin2}
{
}


// ========== Public API ==========

/**
 * Drives each pin low, then makes it an output. The loops stay open.
 *
 * @return Nothing.
 */
void AvrSensorSwitchPort::begin()
{
  for (uint8_t index = 0; index < kChannelCount; ++index)
  {
    digitalWrite(_pins[index], LOW);
    pinMode(_pins[index], OUTPUT);
  }
}

/**
 * Opens or closes one loop. An index outside this board does nothing.
 *
 * @param index Zero-based sensor.
 * @param on True to drive the pin high.
 * @return Nothing.
 */
void AvrSensorSwitchPort::setOn(uint8_t index, bool on)
{
  if (index >= kChannelCount)
  {
    return;
  }
  digitalWrite(_pins[index], on ? HIGH : LOW);
}

#endif
