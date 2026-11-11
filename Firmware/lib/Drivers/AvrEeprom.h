#pragma once

#include <stdint.h>

#include "IEeprom.h"

/**
 * ATmega328PB EEPROM. startWrite arms one erase-and-write and
 * returns. The program runs for about 3.4 ms with interrupts on.
 * poll does nothing because the hardware clears the busy flag.
 */
class AvrEeprom : public IEeprom
{
public:
  /**
   * Nothing to configure. The EEPROM is ready at reset.
   */
  AvrEeprom();

  /**
   * Copying would program the same bytes from two objects.
   *
   * @param other Source. Unused.
   */
  AvrEeprom(const AvrEeprom& other) = delete;

  /**
   * Assignment would program the same bytes from two objects.
   *
   * @param other Source. Unused.
   * @return Nothing.
   */
  AvrEeprom& operator=(const AvrEeprom& other) = delete;

  /**
   * Reports the on-chip EEPROM length.
   *
   * @return E2END + 1.
   */
  uint16_t length() const override;

  /**
   * Reads one byte. Waits out a program that is already running.
   *
   * @param address Byte address. Past the end reads as 0xFF.
   * @return Stored byte.
   */
  uint8_t read(uint16_t address) const override;

  /**
   * Reports whether EEPE is set.
   *
   * @return True while a byte program is running.
   */
  bool busy() const override;

  /**
   * Arms one byte program. Interrupts stay off only across the
   * EEMPE and EEPE stores. A busy device or a busy flash program
   * returns false.
   *
   * @param address Byte address.
   * @param value Byte to store.
   * @return True when EEPE was set.
   */
  bool startWrite(uint16_t address, uint8_t value) override;

  /**
   * No software step. The hardware clears EEPE when the program ends.
   *
   * @return Nothing.
   */
  void poll() override;
};
