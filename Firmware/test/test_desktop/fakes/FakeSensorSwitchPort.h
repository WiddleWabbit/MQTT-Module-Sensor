#pragma once

#include <stdint.h>

#include "ISensorSwitchPort.h"
#include "ModuleProtocol.h"

/**
 * Records loop-switch calls. Every channel starts open.
 */
class FakeSensorSwitchPort : public ISensorSwitchPort
{
public:
  bool on[module_protocol::kMaxSensorsPerModule] = {};
  uint8_t setCount = 0;
  uint8_t lastIndex = 0xFF;

  /**
   * Records the requested level.
   *
   * @param index Zero-based sensor.
   * @param closed True when the loop should be closed.
   * @return Nothing.
   */
  void setOn(uint8_t index, bool closed) override
  {
    setCount++;
    lastIndex = index;
    if (index < module_protocol::kMaxSensorsPerModule)
    {
      on[index] = closed;
    }
  }
};
