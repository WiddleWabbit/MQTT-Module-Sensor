#include <unity.h>

#include "ModuleProtocol.h"
#include "SensorInputs.h"
#include "fakes/FakeAds1115.h"

namespace
{

constexpr int16_t kConnectedMinCounts = 2400;
constexpr uint8_t kInputCount = 3;

}

/**
 * The scan stores channel 0, then 1, then 2, then starts channel 0
 * again. A later conversion replaces the stored counts.
 *
 * 2399 is below the threshold. 2400 meets it. A negative code stays
 * negative and is not connected.
 *
 * @return Nothing.
 */
void testScanStoresEachChannelThenWraps()
{
  FakeAds1115 adc;
  SensorInputs sensors(adc, kInputCount, kConnectedMinCounts);
  adc.ready = true;
  adc.counts[0] = 2399;
  adc.counts[1] = 2400;
  adc.counts[2] = -4;

  sensors.update();
  TEST_ASSERT_EQUAL(1, adc.beginCount);
  TEST_ASSERT_EQUAL(0, adc.startCount);

  sensors.update();
  TEST_ASSERT_EQUAL(1, adc.startCount);
  TEST_ASSERT_EQUAL(0, adc.lastChannel);
  TEST_ASSERT_FALSE(sensors.hasSample(0));

  sensors.update();
  TEST_ASSERT_EQUAL(0, adc.readCount);
  TEST_ASSERT_FALSE(sensors.hasSample(0));

  sensors.update();
  TEST_ASSERT_TRUE(sensors.hasSample(0));
  TEST_ASSERT_FALSE(sensors.connected(0));
  TEST_ASSERT_EQUAL(2399, sensors.counts(0));

  sensors.update();
  TEST_ASSERT_EQUAL(1, adc.lastChannel);
  sensors.update();
  sensors.update();
  TEST_ASSERT_TRUE(sensors.hasSample(1));
  TEST_ASSERT_TRUE(sensors.connected(1));
  TEST_ASSERT_EQUAL(2400, sensors.counts(1));

  sensors.update();
  TEST_ASSERT_EQUAL(2, adc.lastChannel);
  sensors.update();
  sensors.update();
  TEST_ASSERT_TRUE(sensors.hasSample(2));
  TEST_ASSERT_FALSE(sensors.connected(2));
  TEST_ASSERT_EQUAL(-4, sensors.counts(2));

  sensors.update();
  TEST_ASSERT_EQUAL(0, adc.lastChannel);
  TEST_ASSERT_EQUAL(4, adc.startCount);
  TEST_ASSERT_EQUAL(1, adc.beginCount);

  adc.counts[0] = 4800;
  sensors.update();
  sensors.update();
  TEST_ASSERT_TRUE(sensors.connected(0));
  TEST_ASSERT_EQUAL(4800, sensors.counts(0));
  TEST_ASSERT_EQUAL(2400, sensors.counts(1));
}

/**
 * begin() failing leaves every input empty and retries on the next
 * pass. The scan does not start a conversion.
 *
 * @return Nothing.
 */
void testBeginFailureRetriesAndDoesNotStart()
{
  FakeAds1115 adc;
  SensorInputs sensors(adc, kInputCount, kConnectedMinCounts);
  adc.beginOk = false;

  sensors.update();
  sensors.update();
  sensors.update();

  TEST_ASSERT_EQUAL(3, adc.beginCount);
  TEST_ASSERT_EQUAL(0, adc.startCount);
  TEST_ASSERT_EQUAL(0, adc.readCount);
  TEST_ASSERT_FALSE(sensors.hasSample(0));
  TEST_ASSERT_EQUAL(0, sensors.counts(0));
}

/**
 * A refused start leaves the previous sample in place and retries the
 * same channel.
 *
 * @return Nothing.
 */
void testStartFailureKeepsTheLastSampleAndRetriesTheChannel()
{
  FakeAds1115 adc;
  SensorInputs sensors(adc, kInputCount, kConnectedMinCounts);
  adc.ready = true;
  adc.counts[0] = 4800;

  sensors.update();
  sensors.update();
  sensors.update();
  sensors.update();
  TEST_ASSERT_EQUAL(4800, sensors.counts(0));
  TEST_ASSERT_EQUAL(1, adc.startCount);

  adc.startOk = false;
  sensors.update();
  sensors.update();

  TEST_ASSERT_EQUAL(3, adc.startCount);
  TEST_ASSERT_EQUAL(1, adc.lastChannel);
  TEST_ASSERT_FALSE(sensors.hasSample(1));
  TEST_ASSERT_EQUAL(4800, sensors.counts(0));
  TEST_ASSERT_TRUE(sensors.connected(0));
  TEST_ASSERT_EQUAL(1, adc.readCount);
}

/**
 * While the conversion is running, the scan polls and does not read
 * or move to the next channel.
 *
 * @return Nothing.
 */
void testWaitingDoesNotReadOrAdvance()
{
  FakeAds1115 adc;
  SensorInputs sensors(adc, kInputCount, kConnectedMinCounts);
  adc.ready = false;

  sensors.update();
  sensors.update();
  sensors.update();
  sensors.update();

  TEST_ASSERT_EQUAL(1, adc.beginCount);
  TEST_ASSERT_EQUAL(1, adc.startCount);
  TEST_ASSERT_EQUAL(2, adc.readyCount);
  TEST_ASSERT_EQUAL(0, adc.readCount);
  TEST_ASSERT_EQUAL(0, adc.lastChannel);
  TEST_ASSERT_FALSE(sensors.hasSample(0));
}

/**
 * A count of 0 does not call the ADC. Index 0 has no sample.
 *
 * @return Nothing.
 */
void testZeroCountDoesNotTouchTheAdc()
{
  FakeAds1115 adc;
  SensorInputs sensors(adc, 0, kConnectedMinCounts);

  sensors.update();
  sensors.update();

  TEST_ASSERT_EQUAL(0, sensors.count());
  TEST_ASSERT_EQUAL(0, adc.beginCount);
  TEST_ASSERT_EQUAL(0, adc.startCount);
  TEST_ASSERT_FALSE(sensors.hasSample(0));
  TEST_ASSERT_FALSE(sensors.connected(0));
  TEST_ASSERT_EQUAL(0, sensors.counts(0));
}

/**
 * A count above the protocol maximum is stored as that maximum.
 *
 * @return Nothing.
 */
void testCountAboveProtocolMaxIsClamped()
{
  FakeAds1115 adc;
  const uint8_t asked = static_cast<uint8_t>(
      module_protocol::kMaxSensorsPerModule + 1);
  SensorInputs sensors(adc, asked, kConnectedMinCounts);

  TEST_ASSERT_EQUAL(module_protocol::kMaxSensorsPerModule, sensors.count());
  TEST_ASSERT_FALSE(sensors.hasSample(module_protocol::kMaxSensorsPerModule));
}

/**
 * Registers the sensor-input tests with the one Unity main.
 *
 * @return Nothing.
 */
void runSensorInputTests()
{
  RUN_TEST(testScanStoresEachChannelThenWraps);
  RUN_TEST(testBeginFailureRetriesAndDoesNotStart);
  RUN_TEST(testStartFailureKeepsTheLastSampleAndRetriesTheChannel);
  RUN_TEST(testWaitingDoesNotReadOrAdvance);
  RUN_TEST(testZeroCountDoesNotTouchTheAdc);
  RUN_TEST(testCountAboveProtocolMaxIsClamped);
}
