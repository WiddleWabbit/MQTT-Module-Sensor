#pragma once

#include <stdint.h>

#include "IAds1115.h"

/**
 * Records ADS1115 calls for desktop tests. The test sets beginOk,
 * startOk, ready, and counts. readCounts returns the counts for the
 * channel passed to the last startSingleEnded.
 */
class FakeAds1115 : public IAds1115
{
public:
  bool beginOk = true;
  bool startOk = true;
  bool ready = false;
  uint8_t beginCount = 0;
  uint8_t startCount = 0;
  uint8_t readyCount = 0;
  uint8_t readCount = 0;
  uint8_t lastChannel = 0xFF;
  int16_t counts[4] = {};

  /**
   * Records begin and returns the scripted result.
   *
   * @return beginOk.
   */
  bool begin() override
  {
    beginCount++;
    return beginOk;
  }

  /**
   * Records the channel. A channel above 3, or startOk clear,
   * does not start.
   *
   * @param channel Input the scan asked to convert.
   * @return True when the start is accepted.
   */
  bool startSingleEnded(uint8_t channel) override
  {
    startCount++;
    lastChannel = channel;
    if (channel > 3 || !startOk)
    {
      return false;
    }
    return true;
  }

  /**
   * Records a ready poll.
   *
   * @return The scripted ready flag.
   */
  bool conversionReady() override
  {
    readyCount++;
    return ready;
  }

  /**
   * Returns the scripted counts for the last started channel.
   * A channel above 3 reads as 0.
   *
   * @return Scripted counts.
   */
  int16_t readCounts() override
  {
    readCount++;
    if (lastChannel > 3)
    {
      return 0;
    }
    return counts[lastChannel];
  }
};
