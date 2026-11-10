#pragma once

#include "IInterruptMask.h"

/**
 * Desktop stand-in for the interrupt mask. Tests run on one thread,
 * so lock and unlock do nothing.
 */
class FakeInterruptMask : public IInterruptMask
{
public:
  /**
   * Does nothing.
   *
   * @return Nothing.
   */
  void lock() const override
  {
  }

  /**
   * Does nothing.
   *
   * @return Nothing.
   */
  void unlock() const override
  {
  }
};
