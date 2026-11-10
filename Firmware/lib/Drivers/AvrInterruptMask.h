#pragma once

#include <stdint.h>

#include "IInterruptMask.h"

/**
 * AVR global-interrupt mask. lock saves the interrupt flag, including
 * a call from the TWI handler where that flag is already clear.
 * unlock restores it on the matching outer open.
 */
class AvrInterruptMask : public IInterruptMask
{
public:
  /**
   * Starts open. Nothing is masked until lock().
   */
  AvrInterruptMask();

  /**
   * Copying would share one nest count.
   *
   * @param other Source. Unused.
   */
  AvrInterruptMask(const AvrInterruptMask& other) = delete;

  /**
   * Assignment would share one nest count.
   *
   * @param other Source. Unused.
   * @return Nothing.
   */
  AvrInterruptMask& operator=(const AvrInterruptMask& other) = delete;

  /**
   * Closes the mask. Nested calls stay closed.
   *
   * @return Nothing.
   */
  void lock() const override;

  /**
   * Opens one matching lock. The outer open restores the saved flag.
   *
   * @return Nothing.
   */
  void unlock() const override;

private:
  mutable uint8_t _depth;
  mutable uint8_t _wasEnabled;
};
