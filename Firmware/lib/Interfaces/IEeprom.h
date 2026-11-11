#pragma once

#include <stdint.h>

/**
 * Raw EEPROM bytes. The desktop fake records them. The AVR driver
 * programs one byte in the background. startWrite returns without
 * waiting for the program to finish.
 */
class IEeprom
{
public:
  virtual ~IEeprom() = default;

  /**
   * Reports how many bytes the device stores.
   *
   * @return Length in bytes.
   */
  virtual uint16_t length() const = 0;

  /**
   * Reads one committed byte. A program that has been started and
   * not finished is not part of this read.
   *
   * @param address Byte address. An address past the length reads
   *        as 0xFF.
   * @return Stored byte.
   */
  virtual uint8_t read(uint16_t address) const = 0;

  /**
   * Reports whether a byte program is still running.
   *
   * @return True while the last startWrite has not finished.
   */
  virtual bool busy() const = 0;

  /**
   * Starts a byte program. A call while one is running does nothing.
   *
   * @param address Byte address. An address past the length does
   *        nothing.
   * @param value Byte to store.
   * @return True when the program was started.
   */
  virtual bool startWrite(uint16_t address, uint8_t value) = 0;

  /**
   * Lets a finished program become readable. The AVR driver has
   * nothing to do: the hardware clears the busy flag. The fake
   * commits its pending byte here.
   *
   * @return Nothing.
   */
  virtual void poll() = 0;
};
