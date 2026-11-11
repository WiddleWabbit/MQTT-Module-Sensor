#include "AvrEeprom.h"

#if defined(__AVR_ATmega328PB__)

#include <Arduino.h>


// ========== Construction ==========

/**
 * Nothing to configure. The EEPROM is ready at reset.
 */
AvrEeprom::AvrEeprom()
{
}


// ========== Public API ==========

/**
 * Reports the on-chip EEPROM length.
 *
 * @return E2END + 1.
 */
uint16_t AvrEeprom::length() const
{
  return static_cast<uint16_t>(E2END) + 1U;
}

/**
 * Reads one byte. Waits out a program that is already running.
 *
 * @param address Byte address. Past the end reads as 0xFF.
 * @return Stored byte.
 */
uint8_t AvrEeprom::read(uint16_t address) const
{
  if (address > E2END)
  {
    return 0xFF;
  }
  while ((EECR & static_cast<uint8_t>(1U << EEPE)) != 0)
  {
  }
  EEAR = address;
  EECR |= static_cast<uint8_t>(1U << EERE);
  return EEDR;
}

/**
 * Reports whether EEPE is set.
 *
 * @return True while a byte program is running.
 */
bool AvrEeprom::busy() const
{
  return (EECR & static_cast<uint8_t>(1U << EEPE)) != 0;
}

/**
 * Arms one byte program. Interrupts stay off only across the
 * EEMPE and EEPE stores. A busy device or a busy flash program
 * returns false.
 *
 * @param address Byte address.
 * @param value Byte to store.
 * @return True when EEPE was set.
 */
bool AvrEeprom::startWrite(uint16_t address, uint8_t value)
{
  if (address > E2END)
  {
    return false;
  }
  const uint8_t saved = SREG;
  noInterrupts();
  if ((EECR & static_cast<uint8_t>(1U << EEPE)) != 0 ||
      (SPMCSR & static_cast<uint8_t>(1U << SPMEN)) != 0)
  {
    SREG = saved;
    return false;
  }
  EEAR = address;
  EEDR = value;
  EECR |= static_cast<uint8_t>(1U << EEMPE);
  EECR |= static_cast<uint8_t>(1U << EEPE);
  SREG = saved;
  return true;
}

/**
 * No software step. The hardware clears EEPE when the program ends.
 *
 * @return Nothing.
 */
void AvrEeprom::poll()
{
}

#endif
