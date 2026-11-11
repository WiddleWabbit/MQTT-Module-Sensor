#pragma once

#include <stdint.h>

#include "IEeprom.h"

/**
 * EEPROM stand-in. Bytes start at 0xFF. startWrite keeps the new
 * byte pending until poll, unless hold is set.
 */
class FakeEeprom : public IEeprom
{
public:
  static constexpr uint16_t kLength = 1024;
  static constexpr uint8_t kLogLength = 8;

  uint8_t bytes[kLength];
  bool hold = false;
  bool writing = false;
  uint16_t pendingAddress = 0;
  uint8_t pendingValue = 0xFF;
  uint8_t startCount = 0;
  uint16_t started[kLogLength] = {};
  uint8_t startedValue[kLogLength] = {};

  /**
   * Fills the image with 0xFF, then copies a prefix when one is given.
   *
   * @param image Bytes to preload, or nullptr for a blank device.
   * @param imageLength Number of bytes to copy from image.
   */
  explicit FakeEeprom(const uint8_t* image = nullptr,
                      uint16_t imageLength = 0)
  {
    for (uint16_t index = 0; index < kLength; ++index)
    {
      bytes[index] = 0xFF;
    }
    if (image == nullptr)
    {
      return;
    }
    const uint16_t count = imageLength < kLength ? imageLength : kLength;
    for (uint16_t index = 0; index < count; ++index)
    {
      bytes[index] = image[index];
    }
  }

  /**
   * Reports the stand-in length.
   *
   * @return kLength.
   */
  uint16_t length() const override
  {
    return kLength;
  }

  /**
   * Reads one committed byte. A pending program is not visible yet.
   *
   * @param address Byte address. Past the end reads as 0xFF.
   * @return Stored byte.
   */
  uint8_t read(uint16_t address) const override
  {
    if (address >= kLength)
    {
      return 0xFF;
    }
    return bytes[address];
  }

  /**
   * Reports whether a byte is waiting for poll.
   *
   * @return True after startWrite until poll commits it.
   */
  bool busy() const override
  {
    return writing;
  }

  /**
   * Remembers one byte. A second call while writing does nothing.
   *
   * @param address Byte address.
   * @param value Byte to commit on poll.
   * @return True when the program was accepted.
   */
  bool startWrite(uint16_t address, uint8_t value) override
  {
    if (writing || address >= kLength)
    {
      return false;
    }
    writing = true;
    pendingAddress = address;
    pendingValue = value;
    if (startCount < kLogLength)
    {
      started[startCount] = address;
      startedValue[startCount] = value;
    }
    startCount++;
    return true;
  }

  /**
   * Commits the pending byte. hold leaves it pending.
   *
   * @return Nothing.
   */
  void poll() override
  {
    if (!writing || hold)
    {
      return;
    }
    bytes[pendingAddress] = pendingValue;
    writing = false;
  }
};
