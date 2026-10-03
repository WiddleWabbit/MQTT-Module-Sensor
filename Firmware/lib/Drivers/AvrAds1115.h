#pragma once

#include <stdint.h>

#include "IAds1115.h"

/**
 * ADS1115 master on TWI0. SDA is PC4 and SCL is PC5. Wire owns those
 * pins. One instance is the chip driver. The Adafruit object lives in
 * the translation unit, so this header stays free of Arduino types.
 */
class AvrAds1115 : public IAds1115
{
public:
  /**
   * PGA field for ±4.096 V, 125 µV per count. Single-ended codes use
   * the positive half, 0..32767.
   */
  static constexpr uint16_t kGainOne = 0x0200;

  /**
   * ADS1115 data-rate field for 860 samples per second.
   */
  static constexpr uint16_t kRate860Sps = 0x00E0;

  /**
   * Stores the address, gain, and data rate. The bus stays idle
   * until begin().
   *
   * @param address7bit 7-bit ADS1115 address. This board uses 0x48.
   * @param gain PGA field. begin() accepts kGainOne.
   * @param dataRate Data-rate field. begin() accepts kRate860Sps.
   */
  AvrAds1115(uint8_t address7bit, uint16_t gain, uint16_t dataRate);

  /**
   * Copying would share the one chip driver.
   *
   * @param other Source. Unused.
   */
  AvrAds1115(const AvrAds1115& other) = delete;

  /**
   * Assignment would share the one chip driver.
   *
   * @param other Source. Unused.
   * @return Nothing.
   */
  AvrAds1115& operator=(const AvrAds1115& other) = delete;

  /**
   * Starts Wire on TWI0 and probes the chip. A gain or rate other
   * than the constants above is refused.
   *
   * @return True when the chip acknowledges.
   */
  bool begin() override;

  /**
   * Starts a single-shot conversion on one input.
   *
   * @param channel Input 0..3, AIN0..AIN3.
   * @return True when the start was issued.
   */
  bool startSingleEnded(uint8_t channel) override;

  /**
   * Polls the ADS1115 OS bit.
   *
   * @return True when the conversion is complete.
   */
  bool conversionReady() override;

  /**
   * Reads the conversion register.
   *
   * @return Signed counts, or 0 when the bus has not begun.
   */
  int16_t readCounts() override;

private:
  uint8_t _address;
  uint16_t _gain;
  uint16_t _dataRate;
  bool _busReady;
};
