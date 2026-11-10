#pragma once

/**
 * Short mutual exclusion between the scan and the TWI handler.
 * The desktop fake does nothing. The AVR driver saves the interrupt
 * flag and restores it, including when the caller is already in the
 * handler.
 */
class IInterruptMask
{
public:
  virtual ~IInterruptMask() = default;

  /**
   * Closes the mask. Nested calls stay closed until each one opens.
   *
   * @return Nothing.
   */
  virtual void lock() const = 0;

  /**
   * Opens one matching lock. The outer open restores the flag from
   * before the outer close.
   *
   * @return Nothing.
   */
  virtual void unlock() const = 0;
};
