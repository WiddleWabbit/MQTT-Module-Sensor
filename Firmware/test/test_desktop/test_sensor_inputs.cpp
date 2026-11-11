#include <unity.h>

#include "ModuleProtocol.h"
#include "SensorInputs.h"
#include "fakes/FakeAds1115.h"
#include "fakes/FakeEeprom.h"
#include "fakes/FakeInterruptMask.h"
#include "fakes/FakeSensorSwitchPort.h"

namespace
{

constexpr int16_t kConnectedMinCounts = 2400;
constexpr int32_t kTripCounts = 21956;
constexpr uint8_t kTripSamples = 2;
constexpr uint8_t kInputCount = 3;

/**
 * One scan, its ADC fake, and its switch fake.
 */
struct SensorRig
{
  FakeAds1115 adc;
  FakeSensorSwitchPort switches;
  FakeInterruptMask mask;
  FakeEeprom eeprom;
  SensorInputs sensors;

  /**
   * Builds a scan of the given size with the product thresholds.
   * image preloads the fault bytes. A null image is blank.
   *
   * @param count Inputs to scan.
   * @param image EEPROM prefix, or nullptr.
   * @param imageLength Bytes to copy from image.
   */
  explicit SensorRig(uint8_t count, const uint8_t* image = nullptr,
                     uint16_t imageLength = 0)
    : eeprom(image, imageLength),
      sensors(adc, switches, mask, eeprom, count, kConnectedMinCounts,
              kTripCounts, kTripSamples)
  {
  }
};

/**
 * Runs update until the fault image is idle.
 *
 * @param rig Scan under test.
 * @return Nothing.
 */
void flushImage(SensorRig& rig)
{
  for (uint8_t step = 0; step < 24; ++step)
  {
    const uint8_t starts = rig.eeprom.startCount;
    rig.sensors.update();
    if (!rig.eeprom.busy() && rig.eeprom.startCount == starts)
    {
      return;
    }
  }
  TEST_FAIL_MESSAGE("fault image did not settle");
}

/**
 * Runs update until one conversion is stored.
 *
 * @param rig Scan under test. Its ADC fake must be able to finish.
 * @return Nothing.
 */
void publishOne(SensorRig& rig)
{
  rig.adc.ready = true;
  const uint8_t reads = rig.adc.readCount;
  for (uint8_t step = 0; step < 8 && rig.adc.readCount == reads; ++step)
  {
    rig.sensors.update();
  }
  TEST_ASSERT_TRUE(rig.adc.readCount > reads);
}

}

/**
 * The scan stores channel 0, then 1, then 2, then starts channel 0
 * again. A later conversion replaces the stored counts. The loops
 * are closed first, so the threshold decides on versus disconnected.
 *
 * 2399 is below the threshold. 2400 meets it. A negative code stays
 * negative and is not connected.
 *
 * @return Nothing.
 */
void testScanStoresEachChannelThenWraps()
{
  SensorRig rig(kInputCount);
  rig.sensors.turnOn(0);
  rig.sensors.turnOn(1);
  rig.sensors.turnOn(2);
  rig.adc.ready = true;
  rig.adc.counts[0] = 2399;
  rig.adc.counts[1] = 2400;
  rig.adc.counts[2] = -4;

  rig.sensors.update();
  TEST_ASSERT_EQUAL(1, rig.adc.beginCount);
  TEST_ASSERT_EQUAL(0, rig.adc.startCount);

  rig.sensors.update();
  TEST_ASSERT_EQUAL(1, rig.adc.startCount);
  TEST_ASSERT_EQUAL(0, rig.adc.lastChannel);
  TEST_ASSERT_FALSE(rig.sensors.hasSample(0));

  rig.sensors.update();
  TEST_ASSERT_EQUAL(0, rig.adc.readCount);
  TEST_ASSERT_FALSE(rig.sensors.hasSample(0));

  rig.sensors.update();
  TEST_ASSERT_TRUE(rig.sensors.hasSample(0));
  TEST_ASSERT_FALSE(rig.sensors.connected(0));
  TEST_ASSERT_EQUAL(2399, rig.sensors.counts(0));

  rig.sensors.update();
  TEST_ASSERT_EQUAL(1, rig.adc.lastChannel);
  rig.sensors.update();
  rig.sensors.update();
  TEST_ASSERT_TRUE(rig.sensors.hasSample(1));
  TEST_ASSERT_TRUE(rig.sensors.connected(1));
  TEST_ASSERT_EQUAL(2400, rig.sensors.counts(1));

  rig.sensors.update();
  TEST_ASSERT_EQUAL(2, rig.adc.lastChannel);
  rig.sensors.update();
  rig.sensors.update();
  TEST_ASSERT_TRUE(rig.sensors.hasSample(2));
  TEST_ASSERT_FALSE(rig.sensors.connected(2));
  TEST_ASSERT_EQUAL(-4, rig.sensors.counts(2));

  rig.sensors.update();
  TEST_ASSERT_EQUAL(0, rig.adc.lastChannel);
  TEST_ASSERT_EQUAL(4, rig.adc.startCount);
  TEST_ASSERT_EQUAL(1, rig.adc.beginCount);

  rig.adc.counts[0] = 4800;
  rig.sensors.update();
  rig.sensors.update();
  TEST_ASSERT_TRUE(rig.sensors.connected(0));
  TEST_ASSERT_EQUAL(4800, rig.sensors.counts(0));
  TEST_ASSERT_EQUAL(2400, rig.sensors.counts(1));
}

/**
 * begin() failing leaves every input empty and retries on the next
 * pass. The scan does not start a conversion.
 *
 * @return Nothing.
 */
void testBeginFailureRetriesAndDoesNotStart()
{
  SensorRig rig(kInputCount);
  rig.adc.beginOk = false;

  rig.sensors.update();
  rig.sensors.update();
  rig.sensors.update();

  TEST_ASSERT_EQUAL(3, rig.adc.beginCount);
  TEST_ASSERT_EQUAL(0, rig.adc.startCount);
  TEST_ASSERT_EQUAL(0, rig.adc.readCount);
  TEST_ASSERT_FALSE(rig.sensors.hasSample(0));
  TEST_ASSERT_EQUAL(0, rig.sensors.counts(0));
}

/**
 * A refused start leaves the previous sample in place and retries the
 * same channel.
 *
 * @return Nothing.
 */
void testStartFailureKeepsTheLastSampleAndRetriesTheChannel()
{
  SensorRig rig(kInputCount);
  rig.sensors.turnOn(0);
  rig.adc.ready = true;
  rig.adc.counts[0] = 4800;

  rig.sensors.update();
  rig.sensors.update();
  rig.sensors.update();
  rig.sensors.update();
  TEST_ASSERT_EQUAL(4800, rig.sensors.counts(0));
  TEST_ASSERT_EQUAL(1, rig.adc.startCount);

  rig.adc.startOk = false;
  rig.sensors.update();
  rig.sensors.update();

  TEST_ASSERT_EQUAL(3, rig.adc.startCount);
  TEST_ASSERT_EQUAL(1, rig.adc.lastChannel);
  TEST_ASSERT_FALSE(rig.sensors.hasSample(1));
  TEST_ASSERT_EQUAL(4800, rig.sensors.counts(0));
  TEST_ASSERT_TRUE(rig.sensors.connected(0));
  TEST_ASSERT_EQUAL(1, rig.adc.readCount);
}

/**
 * While the conversion is running, the scan polls and does not read
 * or move to the next channel.
 *
 * @return Nothing.
 */
void testWaitingDoesNotReadOrAdvance()
{
  SensorRig rig(kInputCount);
  rig.adc.ready = false;

  rig.sensors.update();
  rig.sensors.update();
  rig.sensors.update();
  rig.sensors.update();

  TEST_ASSERT_EQUAL(1, rig.adc.beginCount);
  TEST_ASSERT_EQUAL(1, rig.adc.startCount);
  TEST_ASSERT_EQUAL(2, rig.adc.readyCount);
  TEST_ASSERT_EQUAL(0, rig.adc.readCount);
  TEST_ASSERT_EQUAL(0, rig.adc.lastChannel);
  TEST_ASSERT_FALSE(rig.sensors.hasSample(0));
}

/**
 * A count of 0 does not call the ADC. Index 0 has no sample.
 *
 * @return Nothing.
 */
void testZeroCountDoesNotTouchTheAdc()
{
  SensorRig rig(0);

  rig.sensors.update();
  rig.sensors.update();

  TEST_ASSERT_EQUAL(0, rig.sensors.count());
  TEST_ASSERT_EQUAL(0, rig.adc.beginCount);
  TEST_ASSERT_EQUAL(0, rig.adc.startCount);
  TEST_ASSERT_FALSE(rig.sensors.hasSample(0));
  TEST_ASSERT_FALSE(rig.sensors.connected(0));
  TEST_ASSERT_EQUAL(0, rig.sensors.counts(0));
}

/**
 * A count above the protocol maximum is stored as that maximum.
 *
 * @return Nothing.
 */
void testCountAboveProtocolMaxIsClamped()
{
  const uint8_t asked = static_cast<uint8_t>(
      module_protocol::kMaxSensorsPerModule + 1);
  SensorRig rig(asked);

  TEST_ASSERT_EQUAL(module_protocol::kMaxSensorsPerModule, rig.sensors.count());
  TEST_ASSERT_FALSE(rig.sensors.hasSample(module_protocol::kMaxSensorsPerModule));
}

/**
 * A sensor starts off with the switch open. On closes that loop and
 * waits for a sample. Below the threshold is disconnected. At the
 * threshold is on. Off wins over a stored sample. Reset opens the loop.
 *
 * @return Nothing.
 */
void testOffOnDisconnectedAndReset()
{
  SensorRig rig(1);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(0));
  TEST_ASSERT_TRUE(rig.sensors.stateReady(0));
  TEST_ASSERT_FALSE(rig.switches.on[0]);
  TEST_ASSERT_EQUAL(0, rig.switches.setCount);

  rig.sensors.turnOn(0);
  TEST_ASSERT_TRUE(rig.switches.on[0]);
  TEST_ASSERT_FALSE(rig.sensors.stateReady(0));
  TEST_ASSERT_FALSE(rig.switches.on[1]);

  rig.adc.counts[0] = 2399;
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateDisconnected,
                          rig.sensors.state(0));
  TEST_ASSERT_FALSE(rig.sensors.connected(0));
  TEST_ASSERT_TRUE(rig.switches.on[0]);

  rig.sensors.turnOn(0);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateDisconnected,
                          rig.sensors.state(0));
  TEST_ASSERT_TRUE(rig.switches.on[0]);

  rig.adc.counts[0] = 2400;
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOn, rig.sensors.state(0));
  TEST_ASSERT_TRUE(rig.sensors.connected(0));

  rig.adc.counts[0] = 2399;
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateDisconnected,
                          rig.sensors.state(0));

  rig.sensors.turnOff(0);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(0));
  TEST_ASSERT_FALSE(rig.switches.on[0]);
  TEST_ASSERT_EQUAL(2399, rig.sensors.counts(0));
  TEST_ASSERT_FALSE(rig.sensors.connected(0));

  rig.sensors.turnOn(0);
  rig.adc.counts[0] = 4800;
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOn, rig.sensors.state(0));
  rig.sensors.reset(0);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(0));
  TEST_ASSERT_FALSE(rig.switches.on[0]);
  TEST_ASSERT_FALSE(rig.sensors.connected(0));

  rig.sensors.reset(0);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(0));
  TEST_ASSERT_FALSE(rig.switches.on[0]);
}

/**
 * Two stored samples at the trip latch a fault and open that loop.
 * Polls while the conversion is running do not add a strike. A later
 * sample stays faulted and does not write the pin again. The other
 * closed loop stays on.
 *
 * @return Nothing.
 */
void testTwoHighSamplesLatchFaultOnThatChannelOnly()
{
  SensorRig rig(kInputCount);
  rig.sensors.turnOn(0);
  rig.sensors.turnOn(1);
  rig.adc.counts[0] = static_cast<int16_t>(kTripCounts);
  rig.adc.counts[1] = 4800;
  rig.adc.counts[2] = 0;

  publishOne(rig);
  TEST_ASSERT_TRUE(rig.sensors.stateReady(0));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOn, rig.sensors.state(0));
  TEST_ASSERT_TRUE(rig.switches.on[0]);

  rig.adc.ready = false;
  const uint8_t reads = rig.adc.readCount;
  rig.sensors.update();
  rig.sensors.update();
  rig.sensors.update();
  TEST_ASSERT_EQUAL(reads, rig.adc.readCount);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOn, rig.sensors.state(0));
  TEST_ASSERT_TRUE(rig.switches.on[0]);

  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOn, rig.sensors.state(1));
  TEST_ASSERT_TRUE(rig.switches.on[1]);

  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff, rig.sensors.state(2));
  TEST_ASSERT_FALSE(rig.switches.on[2]);

  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          rig.sensors.state(0));
  TEST_ASSERT_FALSE(rig.switches.on[0]);
  TEST_ASSERT_FALSE(rig.sensors.connected(0));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOn, rig.sensors.state(1));
  TEST_ASSERT_TRUE(rig.switches.on[1]);

  const uint8_t sets = rig.switches.setCount;
  publishOne(rig);
  publishOne(rig);
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          rig.sensors.state(0));
  TEST_ASSERT_FALSE(rig.switches.on[0]);
  TEST_ASSERT_TRUE(rig.switches.on[1]);
  TEST_ASSERT_EQUAL(sets, rig.switches.setCount);
}

/**
 * One sample at the trip does not latch. A following sample below the
 * trip clears the strike, so the next single high sample stays on.
 *
 * @return Nothing.
 */
void testLowSampleClearsTheStrike()
{
  SensorRig rig(1);
  rig.sensors.turnOn(0);
  rig.adc.counts[0] = static_cast<int16_t>(kTripCounts);
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOn, rig.sensors.state(0));
  TEST_ASSERT_TRUE(rig.switches.on[0]);

  rig.adc.counts[0] = static_cast<int16_t>(kTripCounts - 1);
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOn, rig.sensors.state(0));

  rig.adc.counts[0] = static_cast<int16_t>(kTripCounts);
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOn, rig.sensors.state(0));
  TEST_ASSERT_TRUE(rig.switches.on[0]);
}

/**
 * Samples taken while the loop is open do not latch a fault. On and
 * off for an index outside the count leave the real channel alone.
 *
 * @return Nothing.
 */
void testHighSampleWhileOffDoesNotTrip()
{
  SensorRig rig(1);
  rig.adc.counts[0] = static_cast<int16_t>(kTripCounts);
  publishOne(rig);
  publishOne(rig);

  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(0));
  TEST_ASSERT_FALSE(rig.switches.on[0]);
  TEST_ASSERT_EQUAL(0, rig.switches.setCount);
  TEST_ASSERT_FALSE(rig.sensors.connected(0));
  TEST_ASSERT_EQUAL_INT32(kTripCounts, rig.sensors.counts(0));

  rig.sensors.turnOn(4);
  rig.sensors.turnOff(4);
  rig.sensors.reset(4);
  TEST_ASSERT_EQUAL(0, rig.switches.setCount);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(0));
}

/**
 * On and off while faulted leave the latch and the open switch.
 * Reset returns to off, and a later on is accepted.
 *
 * @return Nothing.
 */
void testFaultIgnoresOnAndOffUntilReset()
{
  SensorRig rig(1);
  rig.sensors.turnOn(0);
  rig.adc.counts[0] = static_cast<int16_t>(kTripCounts);
  publishOne(rig);
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          rig.sensors.state(0));
  const uint8_t sets = rig.switches.setCount;

  rig.sensors.turnOn(0);
  rig.sensors.turnOff(0);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          rig.sensors.state(0));
  TEST_ASSERT_FALSE(rig.switches.on[0]);
  TEST_ASSERT_EQUAL(sets, rig.switches.setCount);

  rig.adc.counts[0] = 0;
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          rig.sensors.state(0));
  TEST_ASSERT_FALSE(rig.switches.on[0]);

  rig.sensors.reset(0);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(0));
  TEST_ASSERT_FALSE(rig.switches.on[0]);

  rig.sensors.turnOn(0);
  TEST_ASSERT_TRUE(rig.switches.on[0]);
  rig.adc.counts[0] = 4800;
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOn, rig.sensors.state(0));
  TEST_ASSERT_TRUE(rig.switches.on[0]);
}

/**
 * A blank image boots every input off and programs nothing.
 *
 * @return Nothing.
 */
void testBlankEepromBootsOff()
{
  SensorRig rig(kInputCount);

  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(0));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(1));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(2));
  rig.sensors.reset(0);
  rig.sensors.reset(1);
  rig.sensors.reset(2);
  rig.sensors.update();
  rig.sensors.update();
  TEST_ASSERT_EQUAL_UINT8(0, rig.eeprom.startCount);
}

/**
 * A stored overcurrent on input 1 boots as fault and stays open.
 * The other inputs still turn on. readFault lists that one reason.
 *
 * @return Nothing.
 */
void testLatchedFaultRestoresAndStaysOpen()
{
  uint8_t image[6];
  for (uint8_t index = 0; index < sizeof(image); ++index)
  {
    image[index] = 0xFF;
  }
  image[2] = module_protocol::kSensorFaultOvercurrent;
  image[3] = static_cast<uint8_t>(
      ~module_protocol::kSensorFaultOvercurrent);
  SensorRig rig(kInputCount, image, sizeof(image));

  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(0));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          rig.sensors.state(1));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(2));
  TEST_ASSERT_FALSE(rig.switches.on[1]);

  rig.sensors.turnOn(1);
  TEST_ASSERT_FALSE(rig.switches.on[1]);
  rig.sensors.turnOn(0);
  TEST_ASSERT_TRUE(rig.switches.on[0]);

  uint8_t body[6] = {};
  TEST_ASSERT_EQUAL_UINT8(6, rig.sensors.readFault(body));
  const uint8_t expected[] = {
    module_protocol::kFaultNone, 0,
    module_protocol::kSensorFaultOvercurrent, 1,
    module_protocol::kFaultNone, 2};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, body, 6);
}

/**
 * A pair that is neither blank nor overcurrent boots as fault.
 * The blank sibling stays off. A matched 0x00 0xFF pair is fault.
 *
 * @return Nothing.
 */
void testCorruptPairRestoresAsFault()
{
  const uint8_t image[] = {0x01, 0x00, 0xFF, 0xFF};
  SensorRig rig(2, image, sizeof(image));

  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          rig.sensors.state(0));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(1));

  const uint8_t matchedClear[] = {0x00, 0xFF};
  SensorRig other(1, matchedClear, sizeof(matchedClear));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          other.sensors.state(0));
}

/**
 * Reason 0xFF with complement 0xFE is the pair left by a program
 * that lost power. A latch writes the complement first. A clear
 * writes the reason back to 0xFF first. Either stop boots as fault.
 *
 * @return Nothing.
 */
void testTornPairBootsFault()
{
  const uint8_t torn[] = {0xFF, 0xFE};
  SensorRig rig(1, torn, sizeof(torn));

  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          rig.sensors.state(0));
  TEST_ASSERT_FALSE(rig.switches.on[0]);
  rig.sensors.turnOn(0);
  TEST_ASSERT_FALSE(rig.switches.on[0]);
}

/**
 * The second trip sample faults in RAM before any byte is armed.
 * The next update arms the complement. While that program is held,
 * a later update does not arm the reason. After both bytes commit,
 * a new scan on the same image boots faulted.
 *
 * @return Nothing.
 */
void testLatchWritesInverseBeforeReason()
{
  SensorRig rig(1);
  rig.sensors.turnOn(0);
  rig.adc.counts[0] = static_cast<int16_t>(kTripCounts);
  publishOne(rig);
  publishOne(rig);

  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          rig.sensors.state(0));
  TEST_ASSERT_EQUAL_UINT8(0, rig.eeprom.startCount);
  TEST_ASSERT_EQUAL_UINT8(0xFF, rig.eeprom.bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, rig.eeprom.bytes[1]);

  rig.adc.ready = false;
  rig.sensors.update();
  TEST_ASSERT_TRUE(rig.eeprom.busy());
  TEST_ASSERT_EQUAL_UINT8(1, rig.eeprom.startCount);
  TEST_ASSERT_EQUAL_UINT16(1, rig.eeprom.started[0]);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(~module_protocol::kSensorFaultOvercurrent),
      rig.eeprom.startedValue[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, rig.eeprom.bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, rig.eeprom.bytes[1]);

  rig.eeprom.hold = true;
  const uint8_t starts = rig.eeprom.startCount;
  const uint8_t polls = rig.adc.readyCount;
  rig.sensors.update();
  TEST_ASSERT_EQUAL_UINT8(starts, rig.eeprom.startCount);
  TEST_ASSERT_TRUE(rig.adc.readyCount > polls);

  rig.eeprom.hold = false;
  rig.sensors.update();
  TEST_ASSERT_EQUAL_UINT8(0xFE, rig.eeprom.bytes[1]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, rig.eeprom.bytes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, rig.eeprom.started[1]);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorFaultOvercurrent,
                          rig.eeprom.startedValue[1]);

  rig.sensors.update();
  TEST_ASSERT_FALSE(rig.eeprom.busy());
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorFaultOvercurrent,
                          rig.eeprom.bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFE, rig.eeprom.bytes[1]);

  FakeAds1115 adc;
  FakeSensorSwitchPort switches;
  FakeInterruptMask mask;
  SensorInputs restored(adc, switches, mask, rig.eeprom, 1,
                        kConnectedMinCounts, kTripCounts, kTripSamples);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          restored.state(0));
  restored.turnOn(0);
  TEST_ASSERT_FALSE(switches.on[0]);
}

/**
 * reset reports off while the old pair is still stored, and set on
 * is accepted then. The following bytes are 0xFF at the reason
 * address, then 0xFF at the complement. A later scan boots off.
 *
 * @return Nothing.
 */
void testResetClearsRamImmediatelyThenBlanksThePair()
{
  SensorRig rig(1);
  rig.sensors.turnOn(0);
  rig.adc.counts[0] = static_cast<int16_t>(kTripCounts);
  publishOne(rig);
  publishOne(rig);
  flushImage(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorFaultOvercurrent,
                          rig.eeprom.bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFE, rig.eeprom.bytes[1]);

  rig.sensors.reset(0);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(0));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorFaultOvercurrent,
                          rig.eeprom.bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFE, rig.eeprom.bytes[1]);

  rig.adc.ready = false;
  rig.sensors.turnOn(0);
  TEST_ASSERT_TRUE(rig.switches.on[0]);
  rig.sensors.turnOff(0);
  TEST_ASSERT_FALSE(rig.switches.on[0]);

  const uint8_t starts = rig.eeprom.startCount;
  rig.sensors.update();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(starts + 1U),
                          rig.eeprom.startCount);
  TEST_ASSERT_EQUAL_UINT16(0, rig.eeprom.started[starts]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, rig.eeprom.startedValue[starts]);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorFaultOvercurrent,
                          rig.eeprom.bytes[0]);

  flushImage(rig);
  TEST_ASSERT_EQUAL_UINT8(0xFF, rig.eeprom.bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, rig.eeprom.bytes[1]);
  const uint8_t complement = static_cast<uint8_t>(starts + 1U);
  TEST_ASSERT_EQUAL_UINT16(1, rig.eeprom.started[complement]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, rig.eeprom.startedValue[complement]);

  FakeAds1115 adc;
  FakeSensorSwitchPort switches;
  FakeInterruptMask mask;
  SensorInputs restored(adc, switches, mask, rig.eeprom, 1,
                        kConnectedMinCounts, kTripCounts, kTripSamples);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          restored.state(0));
}

/**
 * reset on a blank input, and reset again after a committed clear,
 * arm no byte.
 *
 * @return Nothing.
 */
void testResetOfAClearInputDoesNotProgram()
{
  SensorRig rig(1);
  rig.sensors.reset(0);
  rig.sensors.update();
  rig.sensors.update();
  TEST_ASSERT_EQUAL_UINT8(0, rig.eeprom.startCount);

  rig.sensors.turnOn(0);
  rig.adc.counts[0] = static_cast<int16_t>(kTripCounts);
  publishOne(rig);
  publishOne(rig);
  rig.sensors.reset(0);
  flushImage(rig);
  const uint8_t starts = rig.eeprom.startCount;
  rig.sensors.reset(0);
  rig.sensors.update();
  rig.sensors.update();
  TEST_ASSERT_EQUAL_UINT8(starts, rig.eeprom.startCount);
}

/**
 * Latching input 2 programs only that pair. Input 0 keeps the
 * overcurrent pair it already had.
 *
 * @return Nothing.
 */
void testSecondLatchLeavesTheFirstImage()
{
  uint8_t image[6];
  for (uint8_t index = 0; index < sizeof(image); ++index)
  {
    image[index] = 0xFF;
  }
  image[0] = module_protocol::kSensorFaultOvercurrent;
  image[1] = 0xFE;
  SensorRig rig(kInputCount, image, sizeof(image));
  rig.sensors.turnOn(2);
  rig.adc.counts[0] = static_cast<int16_t>(kTripCounts);
  rig.adc.counts[1] = static_cast<int16_t>(kTripCounts);
  rig.adc.counts[2] = static_cast<int16_t>(kTripCounts);
  publishOne(rig);
  publishOne(rig);
  publishOne(rig);
  publishOne(rig);
  publishOne(rig);
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          rig.sensors.state(2));
  TEST_ASSERT_EQUAL_UINT8(0, rig.eeprom.startCount);

  flushImage(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorFaultOvercurrent,
                          rig.eeprom.bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFE, rig.eeprom.bytes[1]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, rig.eeprom.bytes[2]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, rig.eeprom.bytes[3]);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorFaultOvercurrent,
                          rig.eeprom.bytes[4]);
  TEST_ASSERT_EQUAL_UINT8(0xFE, rig.eeprom.bytes[5]);
  TEST_ASSERT_EQUAL_UINT8(2, rig.eeprom.startCount);
  TEST_ASSERT_EQUAL_UINT16(5, rig.eeprom.started[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFE, rig.eeprom.startedValue[0]);
  TEST_ASSERT_EQUAL_UINT16(4, rig.eeprom.started[1]);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorFaultOvercurrent,
                          rig.eeprom.startedValue[1]);
}

/**
 * A clear that is reversed by a new latch finishes as overcurrent.
 *
 * @return Nothing.
 */
void testResetThenLatchKeepsTheLatestReason()
{
  SensorRig rig(1);
  rig.sensors.turnOn(0);
  rig.adc.counts[0] = static_cast<int16_t>(kTripCounts);
  publishOne(rig);
  publishOne(rig);
  flushImage(rig);

  rig.sensors.reset(0);
  rig.adc.ready = false;
  rig.sensors.update();
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorFaultOvercurrent,
                          rig.eeprom.bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOff,
                          rig.sensors.state(0));

  rig.sensors.turnOn(0);
  rig.adc.ready = true;
  publishOne(rig);
  publishOne(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          rig.sensors.state(0));
  flushImage(rig);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorFaultOvercurrent,
                          rig.eeprom.bytes[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFE, rig.eeprom.bytes[1]);

  FakeAds1115 adc;
  FakeSensorSwitchPort switches;
  FakeInterruptMask mask;
  SensorInputs restored(adc, switches, mask, rig.eeprom, 1,
                        kConnectedMinCounts, kTripCounts, kTripSamples);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          restored.state(0));
}

/**
 * A held EEPROM program still lets the scan begin and start.
 *
 * @return Nothing.
 */
void testBusyEepromDoesNotStallTheScan()
{
  SensorRig rig(1);
  rig.eeprom.hold = true;
  TEST_ASSERT_TRUE(rig.eeprom.startWrite(0, 0x00));

  rig.sensors.update();
  TEST_ASSERT_EQUAL_UINT8(1, rig.adc.beginCount);
  TEST_ASSERT_EQUAL_UINT8(1, rig.eeprom.startCount);

  rig.sensors.update();
  TEST_ASSERT_EQUAL_UINT8(1, rig.adc.startCount);
  TEST_ASSERT_EQUAL_UINT8(1, rig.eeprom.startCount);
  TEST_ASSERT_TRUE(rig.eeprom.busy());
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
  RUN_TEST(testOffOnDisconnectedAndReset);
  RUN_TEST(testTwoHighSamplesLatchFaultOnThatChannelOnly);
  RUN_TEST(testLowSampleClearsTheStrike);
  RUN_TEST(testHighSampleWhileOffDoesNotTrip);
  RUN_TEST(testFaultIgnoresOnAndOffUntilReset);
  RUN_TEST(testBlankEepromBootsOff);
  RUN_TEST(testLatchedFaultRestoresAndStaysOpen);
  RUN_TEST(testCorruptPairRestoresAsFault);
  RUN_TEST(testTornPairBootsFault);
  RUN_TEST(testLatchWritesInverseBeforeReason);
  RUN_TEST(testResetClearsRamImmediatelyThenBlanksThePair);
  RUN_TEST(testResetOfAClearInputDoesNotProgram);
  RUN_TEST(testSecondLatchLeavesTheFirstImage);
  RUN_TEST(testResetThenLatchKeepsTheLatestReason);
  RUN_TEST(testBusyEepromDoesNotStallTheScan);
}
