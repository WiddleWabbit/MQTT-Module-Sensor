#include "AvrTwi1Slave.h"

#if defined(__AVR_ATmega328PB__)

#include <Arduino.h>
#include <Wire1.h>
#include <avr/io.h>

#include "ModuleLink.h"

AvrTwi1Slave* AvrTwi1Slave::_instance = nullptr;


// ========== Construction ==========

/**
 * Creates a disabled slave. Call attach before MOD can go low.
 */
AvrTwi1Slave::AvrTwi1Slave()
  : _link(nullptr),
    _replyLength(0),
    _enabled(false)
{
  for (uint8_t index = 0; index < module_protocol::kMaxFrameBytes; ++index)
  {
    _reply[index] = 0xFF;
  }
}


// ========== Public API ==========

/**
 * Routes Wire1 callbacks to the link. One slave is active.
 *
 * @param link Protocol handler for received writes.
 * @return Nothing.
 */
void AvrTwi1Slave::attach(ModuleLink& link)
{
  _link = &link;
  _instance = this;
}

/**
 * Starts Wire1 at address7bit and registers the receive and request
 * callbacks.
 *
 * @param address7bit 7-bit address to acknowledge.
 * @return Nothing.
 */
void AvrTwi1Slave::enable(uint8_t address7bit)
{
  _instance = this;
  _replyLength = 0;
  Wire1.begin(address7bit);
  Wire1.onReceive(AvrTwi1Slave::_onReceive);
  Wire1.onRequest(AvrTwi1Slave::_onRequest);
  _enabled = true;
}

/**
 * Stops TWI1. The pins return to inputs with pull-ups off, so the
 * motherboard's 4.7 kΩ resistors hold the bus.
 *
 * @return Nothing.
 */
void AvrTwi1Slave::disable()
{
  Wire1.end();
  _enabled = false;
}

/**
 * Writes TWAR1. Safe from the stop callback.
 *
 * @param address7bit Assigned 7-bit address.
 * @return Nothing.
 */
void AvrTwi1Slave::setAddress(uint8_t address7bit)
{
  if (!_enabled)
  {
    return;
  }
  TWAR1 = static_cast<uint8_t>(address7bit << 1);
}

/**
 * Copies the reply sent on the next host read.
 *
 * @param frame Reply bytes, including 0xFF pad.
 * @param length Number of bytes in frame, at most 19.
 * @return Nothing.
 */
void AvrTwi1Slave::setReply(const uint8_t* frame, uint8_t length)
{
  if (frame == nullptr)
  {
    _replyLength = 0;
    return;
  }

  uint8_t count = length;
  if (count > module_protocol::kMaxFrameBytes)
  {
    count = module_protocol::kMaxFrameBytes;
  }
  for (uint8_t index = 0; index < count; ++index)
  {
    _reply[index] = frame[index];
  }
  _replyLength = count;
}


// ========== Wire callbacks ==========

/**
 * Reads the write into the link. Runs from the TWI interrupt.
 *
 * @param count Bytes Wire1 reported.
 * @return Nothing.
 */
void AvrTwi1Slave::_onReceive(int count)
{
  if (_instance == nullptr || _instance->_link == nullptr)
  {
    return;
  }

  uint8_t buffer[module_protocol::kMaxFrameBytes] = {};
  size_t length = 0;
  if (count < 0)
  {
    count = 0;
  }

  while (Wire1.available() > 0)
  {
    const int value = Wire1.read();
    if (length < module_protocol::kMaxFrameBytes)
    {
      buffer[length] = static_cast<uint8_t>(value);
    }
    length++;
  }
  if (static_cast<size_t>(count) > length)
  {
    length = static_cast<size_t>(count);
  }
  _instance->_link->onMasterWrite(buffer, length);
}

/**
 * Clocks out the reply stored by setReply. Runs from the TWI
 * interrupt after a repeated start.
 *
 * @return Nothing.
 */
void AvrTwi1Slave::_onRequest()
{
  if (_instance == nullptr || _instance->_replyLength == 0)
  {
    return;
  }
  Wire1.write(_instance->_reply, _instance->_replyLength);
}

#endif
