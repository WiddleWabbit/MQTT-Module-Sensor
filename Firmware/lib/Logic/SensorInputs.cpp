#include "SensorInputs.h"


// ========== Construction ==========

/**
 * Binds the ADC. The scan stays idle until update(). A count above
 * the protocol maximum is stored as that maximum.
 *
 * @param adc Chip port. One call per update().
 * @param count Number of inputs to scan, in channel order.
 * @param connectedMinCounts A sample at or above this is connected.
 */
SensorInputs::SensorInputs(IAds1115& adc, uint8_t count,
                           int16_t connectedMinCounts)
  : _adc(adc),
    _count(count > module_protocol::kMaxSensorsPerModule
               ? module_protocol::kMaxSensorsPerModule
               : count),
    _connectedMinCounts(connectedMinCounts),
    _channel(0),
    _begun(false),
    _phase(_Phase::Starting)
{
  for (uint8_t index = 0; index < module_protocol::kMaxSensorsPerModule;
       ++index)
  {
    _valid[index] = 0;
    _connected[index] = 0;
    _counts[index] = 0;
  }
}


// ========== Public API ==========

/**
 * Advances one step: begin, start, poll, or store. A count of 0
 * does nothing. One call performs one driver call.
 *
 * @return Nothing.
 */
void SensorInputs::update()
{
  if (_count == 0)
  {
    return;
  }
  if (!_begun)
  {
    _begun = _adc.begin();
    return;
  }
  if (_phase == _Phase::Starting)
  {
    if (_adc.startSingleEnded(_channel))
    {
      _phase = _Phase::Waiting;
    }
    return;
  }
  if (_phase == _Phase::Waiting)
  {
    if (_adc.conversionReady())
    {
      _phase = _Phase::Reading;
    }
    return;
  }

  _publish(_channel, _adc.readCounts());
  _channel = static_cast<uint8_t>((_channel + 1U) % _count);
  _phase = _Phase::Starting;
}

/**
 * Reports the number of inputs this object scans.
 *
 * @return Count given at construction, capped at the protocol maximum.
 */
uint8_t SensorInputs::count() const
{
  return _count;
}

/**
 * Reports whether this input has a stored conversion.
 *
 * @param index Zero-based input.
 * @return True when a conversion has been stored for index.
 */
bool SensorInputs::hasSample(uint8_t index) const
{
  if (index >= _count)
  {
    return false;
  }
  return _valid[index] != 0;
}

/**
 * Reports whether the stored sample is at or above the connected
 * threshold. An input with no sample is not connected.
 *
 * @param index Zero-based input.
 * @return True when the stored counts meet the threshold.
 */
bool SensorInputs::connected(uint8_t index) const
{
  if (!hasSample(index))
  {
    return false;
  }
  return _connected[index] != 0;
}

/**
 * Reports the stored counts. An input with no sample reads as 0.
 *
 * @param index Zero-based input.
 * @return Signed counts from the last stored conversion.
 */
int32_t SensorInputs::counts(uint8_t index) const
{
  if (!hasSample(index))
  {
    return 0;
  }
  return _counts[index];
}


// ========== Publish ==========

/**
 * Publishes one sample so a reader never observes a torn value.
 * The valid flag is cleared before the write and set after it.
 *
 * @param index Input whose cache is replaced.
 * @param counts Signed conversion result.
 * @return Nothing.
 */
void SensorInputs::_publish(uint8_t index, int16_t counts)
{
  const uint8_t connected = counts >= _connectedMinCounts ? 1 : 0;
  _valid[index] = 0;
  _counts[index] = counts;
  _connected[index] = connected;
  _valid[index] = 1;
}
