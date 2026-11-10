#pragma once

#include <stdint.h>

#include "ISensorSwitchPort.h"

/**
 * SensorSw pins on the ATmega328PB. High closes that loop through the
 * NPN and the high-side PMOS. The base pulldown holds the loop open
 * until begin().
 */
class AvrSensorSwitchPort : public ISensorSwitchPort
{
public:
  /**
   * How many SensorSw pins this board has, in index order.
   */
  static constexpr uint8_t kChannelCount = 3;

  /**
   * Stores the pins. The levels are applied in begin().
   *
   * @param pin0 SensorSw_1. SENSOR_SW1_PIN in main.cpp.
   * @param pin1 SensorSw_2. SENSOR_SW2_PIN in main.cpp.
   * @param pin2 SensorSw_3. SENSOR_SW3_PIN in main.cpp.
   */
  AvrSensorSwitchPort(uint8_t pin0, uint8_t pin1, uint8_t pin2);

  /**
   * Copying would drive the same pins from two objects.
   *
   * @param other Source. Unused.
   */
  AvrSensorSwitchPort(const AvrSensorSwitchPort& other) = delete;

  /**
   * Assignment would drive the same pins from two objects.
   *
   * @param other Source. Unused.
   * @return Nothing.
   */
  AvrSensorSwitchPort& operator=(const AvrSensorSwitchPort& other) = delete;

  /**
   * Drives each pin low, then makes it an output. The loops stay open.
   *
   * @return Nothing.
   */
  void begin();

  /**
   * Opens or closes one loop. An index outside this board does nothing.
   *
   * @param index Zero-based sensor.
   * @param on True to drive the pin high.
   * @return Nothing.
   */
  void setOn(uint8_t index, bool on) override;

private:
  uint8_t _pins[kChannelCount];
};
