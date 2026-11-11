#include "SensorInputs.h"


namespace
{

constexpr uint8_t kBlankByte = 0xFF;
constexpr uint8_t kOvercurrentComplement = static_cast<uint8_t>(
    ~module_protocol::kSensorFaultOvercurrent);

/**
 * Holds the interrupt mask for one critical section. The destructor
 * releases it on every return.
 */
class MaskLock
{
public:
  /**
   * Closes the mask.
   *
   * @param mask Mask supplied to the sensor module.
   */
  explicit MaskLock(const IInterruptMask& mask)
    : _mask(mask)
  {
    _mask.lock();
  }

  /**
   * Copying would release the mask twice.
   *
   * @param other Source. Unused.
   */
  MaskLock(const MaskLock& other) = delete;

  /**
   * Assignment would release the wrong mask.
   *
   * @param other Source. Unused.
   * @return Nothing.
   */
  MaskLock& operator=(const MaskLock& other) = delete;

  /**
   * Opens the mask.
   */
  ~MaskLock()
  {
    _mask.unlock();
  }

private:
  const IInterruptMask& _mask;
};

}


// ========== Construction ==========

/**
 * Binds the ADC, the switches, the mask, and the fault image. The
 * scan stays idle until update(). A blank stored pair starts that
 * loop open. Any other pair starts it in fault. The constructor
 * reads the image and does not touch the pins. A count above the
 * protocol maximum is stored as that maximum.
 *
 * @param adc Chip port. One call per update().
 * @param switches Loop switches, one per sensor index.
 * @param mask Closed around a phase change and the matching pin write.
 * @param eeprom Fault image. Two bytes per input from address 0.
 * @param count Number of inputs to scan, in channel order.
 * @param connectedMinCounts A live sample at or above this is on.
 * @param tripCounts A sample at or above this counts toward a fault.
 * @param tripSamples Consecutive trip samples that latch a fault.
 */
SensorInputs::SensorInputs(IAds1115& adc, ISensorSwitchPort& switches,
                           IInterruptMask& mask, IEeprom& eeprom,
                           uint8_t count, int16_t connectedMinCounts,
                           int32_t tripCounts, uint8_t tripSamples)
  : _adc(adc),
    _switches(switches),
    _mask(mask),
    _eeprom(eeprom),
    _count(count > module_protocol::kMaxSensorsPerModule
               ? module_protocol::kMaxSensorsPerModule
               : count),
    _connectedMinCounts(connectedMinCounts),
    _tripCounts(tripCounts),
    _tripSamples(tripSamples),
    _channel(0),
    _begun(false),
    _phase(_Phase::Starting),
    _inFlight(false),
    _inFlightAddress(0),
    _inFlightValue(kBlankByte)
{
  for (uint8_t index = 0; index < module_protocol::kMaxSensorsPerModule;
       ++index)
  {
    _valid[index] = 0;
    _counts[index] = 0;
    _power[index] = static_cast<uint8_t>(_Power::Off);
    _fresh[index] = 0;
    _strikes[index] = 0;
    _record[index] = static_cast<uint8_t>(_Record::Clear);
  }
  for (uint8_t index = 0; index < kImageBytes; ++index)
  {
    _image[index] = kBlankByte;
  }
  for (uint8_t index = 0; index < _count; ++index)
  {
    const uint16_t reasonAddress =
        static_cast<uint16_t>(index * kImageBytesPerInput);
    const uint16_t inverseAddress =
        static_cast<uint16_t>(reasonAddress + 1U);
    if (inverseAddress >= _eeprom.length() ||
        inverseAddress >= kImageBytes)
    {
      break;
    }
    const uint8_t reason = _eeprom.read(reasonAddress);
    const uint8_t inverse = _eeprom.read(inverseAddress);
    _image[reasonAddress] = reason;
    _image[inverseAddress] = inverse;
    if (reason != kBlankByte || inverse != kBlankByte)
    {
      _power[index] = static_cast<uint8_t>(_Power::Fault);
      _record[index] = static_cast<uint8_t>(_Record::Fault);
    }
  }
}


// ========== Public API ==========

/**
 * Arms or polls one EEPROM byte, then advances one scan step:
 * begin, start, poll, or store. A stored sample updates that
 * sensor's state. A count of 0 leaves the ADC alone. One call
 * performs one ADC-driver call.
 *
 * @return Nothing.
 */
void SensorInputs::update()
{
  _serviceStore();
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

  const int16_t sample = _adc.readCounts();
  _publish(_channel, sample);
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
 * Reports whether the sensor state is on. Off, disconnected, fault,
 * and a loop waiting for its first sample are not connected.
 *
 * @param index Zero-based input.
 * @return True when the state is on.
 */
bool SensorInputs::connected(uint8_t index) const
{
  if (index >= _count)
  {
    return false;
  }
  const MaskLock hold(_mask);
  return _stateReady(index) &&
         _state(index) == module_protocol::kSensorStateOn;
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

/**
 * Reports whether GET_SENSOR_STATE can answer. Off and fault are
 * ready immediately. A loop closed since the last sample is not.
 *
 * @param index Zero-based input.
 * @return False when the index is outside the count, or the loop is
 *         closed and no sample has been stored since it closed.
 */
bool SensorInputs::stateReady(uint8_t index) const
{
  if (index >= _count)
  {
    return false;
  }
  const MaskLock hold(_mask);
  return _stateReady(index);
}

/**
 * Wire state: off, on, fault, or disconnected. Meaningful when
 * stateReady() is true.
 *
 * @param index Zero-based input.
 * @return module_protocol sensor state byte. Off when index is
 *         outside the count.
 */
uint8_t SensorInputs::state(uint8_t index) const
{
  if (index >= _count)
  {
    return module_protocol::kSensorStateOff;
  }
  const MaskLock hold(_mask);
  return _state(index);
}

/**
 * Writes the GET_FAULT list. Each input is one record, in index
 * order: reason, then index. Reason 0 is not latched. The only
 * latched reason is overcurrent. One mask covers the whole list.
 *
 * @param dest Destination. Holds two bytes per input. Not retained.
 * @return Number of bytes written.
 */
uint8_t SensorInputs::readFault(uint8_t* dest) const
{
  const MaskLock hold(_mask);
  for (uint8_t index = 0; index < _count; ++index)
  {
    const uint8_t offset = static_cast<uint8_t>(
        index * module_protocol::kFaultRecordLen);
    uint8_t reason = module_protocol::kFaultNone;
    if (_state(index) == module_protocol::kSensorStateFault)
    {
      reason = module_protocol::kSensorFaultOvercurrent;
    }
    dest[offset] = reason;
    dest[static_cast<uint8_t>(offset + 1U)] = index;
  }
  return static_cast<uint8_t>(_count * module_protocol::kFaultRecordLen);
}

/**
 * Closes the loop from off. A sensor that is already on is left as
 * it is, including its strike count. A fault ignores this call.
 * An index outside the count does nothing.
 *
 * @param index Zero-based input.
 * @return Nothing.
 */
void SensorInputs::turnOn(uint8_t index)
{
  if (index >= _count)
  {
    return;
  }
  const MaskLock hold(_mask);
  if (_power[index] != static_cast<uint8_t>(_Power::Off))
  {
    return;
  }
  _power[index] = static_cast<uint8_t>(_Power::On);
  _fresh[index] = 0;
  _strikes[index] = 0;
  _switches.setOn(index, true);
}

/**
 * Opens the loop from on or disconnected. A fault ignores this call,
 * so only reset() clears a fault. An index outside the count does
 * nothing.
 *
 * @param index Zero-based input.
 * @return Nothing.
 */
void SensorInputs::turnOff(uint8_t index)
{
  if (index >= _count)
  {
    return;
  }
  const MaskLock hold(_mask);
  if (_power[index] != static_cast<uint8_t>(_Power::On))
  {
    return;
  }
  _power[index] = static_cast<uint8_t>(_Power::Off);
  _strikes[index] = 0;
  _switches.setOn(index, false);
}

/**
 * Clears a fault and opens the loop. On and disconnected end off
 * as well. The live latch clears before this returns. The stored
 * pair catches up from update(). An index outside the count does
 * nothing.
 *
 * @param index Zero-based input.
 * @return Nothing.
 */
void SensorInputs::reset(uint8_t index)
{
  if (index >= _count)
  {
    return;
  }
  const MaskLock hold(_mask);
  _power[index] = static_cast<uint8_t>(_Power::Off);
  _fresh[index] = 0;
  _strikes[index] = 0;
  _record[index] = static_cast<uint8_t>(_Record::Clear);
  _switches.setOn(index, false);
}


// ========== Publish ==========

/**
 * Publishes one sample, then applies on, disconnected, or a fault
 * strike while the loop is closed. The mask is held for the store
 * and the pin write.
 *
 * @param index Input whose cache is replaced.
 * @param counts Signed conversion result.
 * @return Nothing.
 */
void SensorInputs::_publish(uint8_t index, int16_t counts)
{
  const MaskLock hold(_mask);
  _valid[index] = 0;
  _counts[index] = counts;
  _valid[index] = 1;
  _applySample(index, counts);
}

/**
 * Applies one stored sample to a closed loop. The caller holds the
 * mask. An open or faulted loop is left as it is.
 *
 * @param index Input the sample belongs to.
 * @param counts Signed conversion result.
 * @return Nothing.
 */
void SensorInputs::_applySample(uint8_t index, int16_t counts)
{
  if (_power[index] != static_cast<uint8_t>(_Power::On))
  {
    return;
  }
  _fresh[index] = 1;
  if (static_cast<int32_t>(counts) >= _tripCounts)
  {
    if (_strikes[index] < 255)
    {
      _strikes[index] = static_cast<uint8_t>(_strikes[index] + 1U);
    }
  }
  else
  {
    _strikes[index] = 0;
  }
  if (_tripSamples > 0 && _strikes[index] >= _tripSamples)
  {
    _power[index] = static_cast<uint8_t>(_Power::Fault);
    _record[index] = static_cast<uint8_t>(_Record::Fault);
    _switches.setOn(index, false);
  }
}

/**
 * Wire state. The caller holds the mask.
 *
 * @param index Input inside the count.
 * @return module_protocol sensor state byte.
 */
uint8_t SensorInputs::_state(uint8_t index) const
{
  if (_power[index] == static_cast<uint8_t>(_Power::Fault))
  {
    return module_protocol::kSensorStateFault;
  }
  if (_power[index] == static_cast<uint8_t>(_Power::Off))
  {
    return module_protocol::kSensorStateOff;
  }
  if (_fresh[index] == 0)
  {
    return module_protocol::kSensorStateOff;
  }
  if (_counts[index] >= _connectedMinCounts)
  {
    return module_protocol::kSensorStateOn;
  }
  return module_protocol::kSensorStateDisconnected;
}

/**
 * True when the state can be answered. The caller holds the mask.
 *
 * @param index Input inside the count.
 * @return False while a closed loop is waiting for a sample.
 */
bool SensorInputs::_stateReady(uint8_t index) const
{
  if (_power[index] != static_cast<uint8_t>(_Power::On))
  {
    return true;
  }
  return _fresh[index] != 0;
}


// ========== Fault image ==========

/**
 * Polls the EEPROM port and arms at most one byte. The caller is
 * update(). A program that is still running is left running.
 *
 * @return Nothing.
 */
void SensorInputs::_serviceStore()
{
  _eeprom.poll();
  const MaskLock hold(_mask);
  if (_inFlight)
  {
    if (_eeprom.busy())
    {
      return;
    }
    if (_inFlightAddress < kImageBytes)
    {
      _image[_inFlightAddress] = _inFlightValue;
    }
    _inFlight = false;
  }
  if (_eeprom.busy())
  {
    return;
  }
  uint16_t address = 0;
  uint8_t value = kBlankByte;
  if (!_selectByte(&address, &value))
  {
    return;
  }
  if (_eeprom.startWrite(address, value))
  {
    _inFlight = true;
    _inFlightAddress = address;
    _inFlightValue = value;
  }
}

/**
 * Chooses the next image byte that differs from the latched record.
 * The caller holds the mask. A fault writes the complement first.
 * A clear writes the reason byte back to 0xFF first.
 *
 * @param address Set to the byte address when a write is required.
 * @param value Set to the byte to program.
 * @return True when a byte should be programmed.
 */
bool SensorInputs::_selectByte(uint16_t* address, uint8_t* value) const
{
  for (uint8_t index = 0; index < _count; ++index)
  {
    const uint16_t reasonAddress =
        static_cast<uint16_t>(index * kImageBytesPerInput);
    const uint16_t inverseAddress =
        static_cast<uint16_t>(reasonAddress + 1U);
    if (inverseAddress >= _eeprom.length() ||
        inverseAddress >= kImageBytes)
    {
      return false;
    }
    const uint8_t reason = _image[reasonAddress];
    const uint8_t inverse = _image[inverseAddress];
    const bool fault =
        _record[index] == static_cast<uint8_t>(_Record::Fault);
    if (fault)
    {
      if (inverse != kOvercurrentComplement)
      {
        *address = inverseAddress;
        *value = kOvercurrentComplement;
        return true;
      }
      if (reason != module_protocol::kSensorFaultOvercurrent)
      {
        *address = reasonAddress;
        *value = module_protocol::kSensorFaultOvercurrent;
        return true;
      }
    }
    else if (reason != kBlankByte)
    {
      *address = reasonAddress;
      *value = kBlankByte;
      return true;
    }
    else if (inverse != kBlankByte)
    {
      *address = inverseAddress;
      *value = kBlankByte;
      return true;
    }
  }
  return false;
}
