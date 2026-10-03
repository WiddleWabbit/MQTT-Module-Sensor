#include "AvrAds1115.h"

#if defined(__AVR_ATmega328PB__)

#include <Adafruit_ADS1X15.h>
#include <Arduino.h>
#include <Wire.h>

namespace
{

/**
 * The one ADS1115 object. Constructed on the first begin(), after
 * the Arduino core has started.
 *
 * @return The chip driver.
 */
Adafruit_ADS1115& adsChip()
{
  static Adafruit_ADS1115 chip;
  return chip;
}

}


// ========== Construction ==========

/**
 * Stores the address, gain, and data rate. The bus stays idle
 * until begin().
 *
 * @param address7bit 7-bit ADS1115 address. This board uses 0x48.
 * @param gain PGA field. begin() accepts kGainOne.
 * @param dataRate Data-rate field. begin() accepts kRate860Sps.
 */
AvrAds1115::AvrAds1115(uint8_t address7bit, uint16_t gain, uint16_t dataRate)
  : _address(address7bit),
    _gain(gain),
    _dataRate(dataRate),
    _busReady(false)
{
}


// ========== Public API ==========

/**
 * Starts Wire on TWI0 and probes the chip. A gain or rate other
 * than the constants above is refused.
 *
 * @return True when the chip acknowledges.
 */
bool AvrAds1115::begin()
{
  if (_gain != kGainOne || _dataRate != kRate860Sps)
  {
    return false;
  }

  Wire.begin();
  Adafruit_ADS1115& ads = adsChip();
  ads.setGain(GAIN_ONE);
  ads.setDataRate(RATE_ADS1115_860SPS);
  _busReady = ads.begin(_address);
  return _busReady;
}

/**
 * Starts a single-shot conversion on one input. Adafruit also writes
 * both threshold registers to arm ALERT/RDY. That pin is unconnected
 * on this board. The OS bit is what conversionReady() polls.
 *
 * @param channel Input 0..3, AIN0..AIN3.
 * @return True when the start was issued.
 */
bool AvrAds1115::startSingleEnded(uint8_t channel)
{
  if (!_busReady || channel > 3)
  {
    return false;
  }
  adsChip().startADCReading(MUX_BY_CHANNEL[channel], false);
  return true;
}

/**
 * Polls the ADS1115 OS bit.
 *
 * @return True when the conversion is complete.
 */
bool AvrAds1115::conversionReady()
{
  if (!_busReady)
  {
    return false;
  }
  return adsChip().conversionComplete();
}

/**
 * Reads the conversion register.
 *
 * @return Signed counts, or 0 when the bus has not begun.
 */
int16_t AvrAds1115::readCounts()
{
  if (!_busReady)
  {
    return 0;
  }
  return adsChip().getLastConversionResults();
}

#endif
