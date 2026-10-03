#pragma once

#include <stdint.h>

#include "IAds1115.h"
#include "ModuleProtocol.h"

/**
 * Round-robin reader for the sensor inputs on one ADS1115.
 * update() advances one step of the scan. The cached counts are what
 * the link returns. No Arduino types.
 */
class SensorInputs
{
public:
  /**
   * Binds the ADC. The scan stays idle until update(). A count above
   * the protocol maximum is stored as that maximum.
   *
   * @param adc Chip port. One call per update().
   * @param count Number of inputs to scan, in channel order.
   * @param connectedMinCounts A sample at or above this is connected.
   */
  SensorInputs(IAds1115& adc, uint8_t count, int16_t connectedMinCounts);

  /**
   * Copying would share the ADC scan with another object.
   *
   * @param other Source. Unused.
   */
  SensorInputs(const SensorInputs& other) = delete;

  /**
   * Assignment would share the ADC scan with another object.
   *
   * @param other Source. Unused.
   * @return Nothing.
   */
  SensorInputs& operator=(const SensorInputs& other) = delete;

  /**
   * Advances one step: begin, start, poll, or store. A count of 0
   * does nothing. One call performs one driver call.
   *
   * @return Nothing.
   */
  void update();

  /**
   * Reports the number of inputs this object scans.
   *
   * @return Count given at construction, capped at the protocol maximum.
   */
  uint8_t count() const;

  /**
   * Reports whether this input has a stored conversion.
   *
   * @param index Zero-based input.
   * @return True when a conversion has been stored for index.
   */
  bool hasSample(uint8_t index) const;

  /**
   * Reports whether the stored sample is at or above the connected
   * threshold. An input with no sample is not connected.
   *
   * @param index Zero-based input.
   * @return True when the stored counts meet the threshold.
   */
  bool connected(uint8_t index) const;

  /**
   * Reports the stored counts. An input with no sample reads as 0.
   *
   * @param index Zero-based input.
   * @return Signed counts from the last stored conversion.
   */
  int32_t counts(uint8_t index) const;

private:
  /**
   * Scan step. Starting launches a conversion, Waiting polls it, and
   * Reading stores it.
   */
  enum class _Phase : uint8_t
  {
    Starting,
    Waiting,
    Reading
  };

  /**
   * Publishes one sample so a reader never observes a torn value.
   * The valid flag is cleared before the write and set after it.
   *
   * @param index Input whose cache is replaced.
   * @param counts Signed conversion result.
   * @return Nothing.
   */
  void _publish(uint8_t index, int16_t counts);

  IAds1115& _adc;
  uint8_t _count;
  int16_t _connectedMinCounts;
  uint8_t _channel;
  bool _begun;
  _Phase _phase;
  volatile uint8_t _valid[module_protocol::kMaxSensorsPerModule];
  volatile uint8_t _connected[module_protocol::kMaxSensorsPerModule];
  volatile int32_t _counts[module_protocol::kMaxSensorsPerModule];
};
