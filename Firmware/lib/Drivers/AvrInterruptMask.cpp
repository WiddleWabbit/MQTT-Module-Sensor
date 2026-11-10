#include "AvrInterruptMask.h"

#if defined(__AVR_ATmega328PB__)

#include <Arduino.h>


// ========== Construction ==========

/**
 * Starts open. Nothing is masked until lock().
 */
AvrInterruptMask::AvrInterruptMask()
  : _depth(0),
    _wasEnabled(0)
{
}


// ========== Public API ==========

/**
 * Closes the mask. Nested calls stay closed.
 *
 * @return Nothing.
 */
void AvrInterruptMask::lock() const
{
  const uint8_t sreg = SREG;
  noInterrupts();
  if (_depth == 0)
  {
    _wasEnabled = (sreg & static_cast<uint8_t>(1U << 7)) != 0 ? 1 : 0;
  }
  _depth++;
}

/**
 * Opens one matching lock. The outer open restores the saved flag.
 *
 * @return Nothing.
 */
void AvrInterruptMask::unlock() const
{
  if (_depth == 0)
  {
    return;
  }
  _depth--;
  if (_depth == 0 && _wasEnabled != 0)
  {
    interrupts();
  }
}

#endif
