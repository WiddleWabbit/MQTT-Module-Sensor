#pragma once

#include <stdint.h>

#include "IAds1115.h"
#include "IInterruptMask.h"
#include "ISensorSwitchPort.h"
#include "ModuleProtocol.h"

/**
 * Round-robin reader and loop switch for the sensors on one ADS1115.
 * update() advances one step of the scan. A sample stored while the
 * loop is closed sets on, disconnected, or a latched fault. The link
 * reads the cache and the state. No Arduino types.
 *
 * turnOn, turnOff, reset, state, stateReady, and readFault may run
 * from the TWI interrupt. update runs from the main loop. The ADC
 * call stays preemptible. The phase and the pin are committed
 * together under the mask.
 */
class SensorInputs
{
public:
  /**
   * Binds the ADC, the switches, and the mask. The scan stays idle
   * until update(). Every loop starts open. The constructor does not
   * touch the pins. A count above the protocol maximum is stored as
   * that maximum.
   *
   * @param adc Chip port. One call per update().
   * @param switches Loop switches, one per sensor index.
   * @param mask Closed around a phase change and the matching pin write.
   * @param count Number of inputs to scan, in channel order.
   * @param connectedMinCounts A live sample at or above this is on.
   * @param tripCounts A sample at or above this counts toward a fault.
   * @param tripSamples Consecutive trip samples that latch a fault.
   */
  SensorInputs(IAds1115& adc, ISensorSwitchPort& switches,
               IInterruptMask& mask, uint8_t count,
               int16_t connectedMinCounts, int32_t tripCounts,
               uint8_t tripSamples);

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
   * Advances one step: begin, start, poll, or store. A stored sample
   * updates that sensor's state. A count of 0 does nothing. One call
   * performs one driver call.
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
   * Reports whether the sensor state is on. Off, disconnected, fault,
   * and a loop waiting for its first sample are not connected.
   *
   * @param index Zero-based input.
   * @return True when the state is on.
   */
  bool connected(uint8_t index) const;

  /**
   * Reports the stored counts. An input with no sample reads as 0.
   *
   * @param index Zero-based input.
   * @return Signed counts from the last stored conversion.
   */
  int32_t counts(uint8_t index) const;

  /**
   * Reports whether GET_SENSOR_STATE can answer. Off and fault are
   * ready immediately. A loop closed since the last sample is not.
   *
   * @param index Zero-based input.
   * @return False when the index is outside the count, or the loop is
   *         closed and no sample has been stored since it closed.
   */
  bool stateReady(uint8_t index) const;

  /**
   * Wire state: off, on, fault, or disconnected. Meaningful when
   * stateReady() is true.
   *
   * @param index Zero-based input.
   * @return module_protocol sensor state byte. Off when index is
   *         outside the count.
   */
  uint8_t state(uint8_t index) const;

  /**
   * Writes the GET_FAULT list. Each input is one record, in index
   * order: reason, then index. Reason 0 is not latched. The only
   * latched reason is overcurrent. One mask covers the whole list.
   *
   * @param dest Destination. Holds two bytes per input. Not retained.
   * @return Number of bytes written.
   */
  uint8_t readFault(uint8_t* dest) const;

  /**
   * Closes the loop from off. A sensor that is already on is left as
   * it is, including its strike count. A fault ignores this call.
   * An index outside the count does nothing.
   *
   * @param index Zero-based input.
   * @return Nothing.
   */
  void turnOn(uint8_t index);

  /**
   * Opens the loop from on or disconnected. A fault ignores this call,
   * so only reset() clears a fault. An index outside the count does
   * nothing.
   *
   * @param index Zero-based input.
   * @return Nothing.
   */
  void turnOff(uint8_t index);

  /**
   * Clears a fault and opens the loop. On and disconnected end off
   * as well. An index outside the count does nothing.
   *
   * @param index Zero-based input.
   * @return Nothing.
   */
  void reset(uint8_t index);

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
   * Commanded loop. On covers the reported states on and disconnected.
   * Fault stays latched with the switch open.
   */
  enum class _Power : uint8_t
  {
    Off,
    On,
    Fault
  };

  /**
   * Publishes one sample, then applies on, disconnected, or a fault
   * strike while the loop is closed. The mask is held for the store
   * and the pin write.
   *
   * @param index Input whose cache is replaced.
   * @param counts Signed conversion result.
   * @return Nothing.
   */
  void _publish(uint8_t index, int16_t counts);

  /**
   * Applies one stored sample to a closed loop. The caller holds the
   * mask. An open or faulted loop is left as it is.
   *
   * @param index Input the sample belongs to.
   * @param counts Signed conversion result.
   * @return Nothing.
   */
  void _applySample(uint8_t index, int16_t counts);

  /**
   * Wire state. The caller holds the mask.
   *
   * @param index Input inside the count.
   * @return module_protocol sensor state byte.
   */
  uint8_t _state(uint8_t index) const;

  /**
   * True when the state can be answered. The caller holds the mask.
   *
   * @param index Input inside the count.
   * @return False while a closed loop is waiting for a sample.
   */
  bool _stateReady(uint8_t index) const;

  IAds1115& _adc;
  ISensorSwitchPort& _switches;
  IInterruptMask& _mask;
  uint8_t _count;
  int16_t _connectedMinCounts;
  int32_t _tripCounts;
  uint8_t _tripSamples;
  uint8_t _channel;
  bool _begun;
  _Phase _phase;
  volatile uint8_t _valid[module_protocol::kMaxSensorsPerModule];
  volatile int32_t _counts[module_protocol::kMaxSensorsPerModule];
  volatile uint8_t _power[module_protocol::kMaxSensorsPerModule];
  volatile uint8_t _fresh[module_protocol::kMaxSensorsPerModule];
  volatile uint8_t _strikes[module_protocol::kMaxSensorsPerModule];
};
