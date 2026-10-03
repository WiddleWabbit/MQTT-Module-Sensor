#pragma once

#include <stdint.h>

/**
 * Master port for one ADS1115. The desktop fake records calls. The AVR
 * driver talks to TWI0. SensorInputs calls one method per pass.
 */
class IAds1115
{
public:
  virtual ~IAds1115() = default;

  /**
   * Starts the bus and checks that the chip acknowledges.
   *
   * @return True when the chip acknowledges.
   */
  virtual bool begin() = 0;

  /**
   * Starts one single-ended conversion. Channel 0 is AIN0.
   *
   * @param channel Input 0..3.
   * @return True when the conversion was started.
   */
  virtual bool startSingleEnded(uint8_t channel) = 0;

  /**
   * Reports whether the conversion started last is ready to read.
   *
   * @return True when a result can be read.
   */
  virtual bool conversionReady() = 0;

  /**
   * Reads the last conversion result. Valid after conversionReady().
   *
   * @return Signed counts. Single-ended readings use 0..32767.
   */
  virtual int16_t readCounts() = 0;
};
