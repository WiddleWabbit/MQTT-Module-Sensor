#pragma once

#include <stdint.h>

/**
 * One high-side loop switch per sensor. The desktop fake records the
 * level. The AVR driver drives the SensorSw pins. true closes the loop.
 */
class ISensorSwitchPort
{
public:
  virtual ~ISensorSwitchPort() = default;

  /**
   * Opens or closes one sensor loop.
   *
   * @param index Zero-based sensor.
   * @param on True to close the loop.
   * @return Nothing.
   */
  virtual void setOn(uint8_t index, bool on) = 0;
};
