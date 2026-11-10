#include <unity.h>

#include "ModuleLink.h"
#include "ModuleProtocol.h"
#include "fakes/FakeAds1115.h"
#include "fakes/FakeInterruptMask.h"
#include "fakes/FakeModuleSlavePort.h"
#include "fakes/FakeSensorSwitchPort.h"

namespace
{

constexpr uint16_t kTestType = module_protocol::kTypeSensorModule;
constexpr uint16_t kTestFirmwareVersion = 1;
constexpr int16_t kConnectedMinCounts = 2400;
constexpr int32_t kTripCounts = 21956;
constexpr uint8_t kTripSamples = 2;

/**
 * One link, its slave fake, and a sensor cache. The default count is
 * 0 so a protocol test does not depend on a conversion.
 */
struct Board
{
  FakeAds1115 adc;
  FakeSensorSwitchPort switches;
  FakeInterruptMask mask;
  SensorInputs sensors;
  FakeModuleSlavePort port;
  ModuleLink link;

  /**
   * Builds a link around a sensor cache of the given size.
   *
   * @param sensorCount Inputs the cache reports.
   * @param typeId Type id supplied to the link.
   * @param firmwareVersion Firmware version supplied to the link.
   */
  explicit Board(uint8_t sensorCount = 0,
                 uint16_t typeId = kTestType,
                 uint16_t firmwareVersion = kTestFirmwareVersion)
    : sensors(adc, switches, mask, sensorCount, kConnectedMinCounts,
              kTripCounts, kTripSamples),
      link(port, sensors, typeId, firmwareVersion)
  {
  }
};

/**
 * Builds one request frame, including its CRC.
 *
 * @param command Command byte.
 * @param payload Payload bytes, or nullptr when payloadLen is 0.
 * @param payloadLen Payload length.
 * @param out Destination. Must hold payloadLen + 3 bytes.
 * @return On-wire size, including the CRC byte.
 */
size_t buildFrame(uint8_t command, const uint8_t* payload, uint8_t payloadLen,
                  uint8_t* out)
{
  const uint8_t lengthField = static_cast<uint8_t>(2 + payloadLen);
  out[0] = lengthField;
  out[1] = command;
  for (uint8_t index = 0; index < payloadLen; ++index)
  {
    out[2 + index] = payload[index];
  }
  out[lengthField] = module_protocol::crc8Smbus(out, lengthField);
  return static_cast<size_t>(lengthField) + 1U;
}

/**
 * Delivers one CRC-valid command through the fake port.
 *
 * @param port Fake whose handler receives the frame.
 * @param command Command byte.
 * @param payload Payload bytes, or nullptr when payloadLen is 0.
 * @param payloadLen Payload length.
 * @return Nothing.
 */
void writeCommand(FakeModuleSlavePort& port, uint8_t command,
                  const uint8_t* payload, uint8_t payloadLen)
{
  uint8_t frame[module_protocol::kMaxFrameBytes] = {};
  const size_t size = buildFrame(command, payload, payloadLen, frame);
  port.deliver(frame, size);
}

/**
 * Checks a 19-byte padded reply against status and payload.
 *
 * @param port Fake that received setReply.
 * @param status Expected status byte.
 * @param payload Expected payload, or nullptr when payloadLen is 0.
 * @param payloadLen Expected payload length.
 * @return Nothing.
 */
void expectReply(const FakeModuleSlavePort& port, uint8_t status,
                 const uint8_t* payload, uint8_t payloadLen)
{
  uint8_t expected[module_protocol::kMaxFrameBytes];
  for (uint8_t index = 0; index < module_protocol::kMaxFrameBytes; ++index)
  {
    expected[index] = 0xFF;
  }
  const uint8_t lengthField = static_cast<uint8_t>(2 + payloadLen);
  expected[0] = lengthField;
  expected[1] = status;
  for (uint8_t index = 0; index < payloadLen; ++index)
  {
    expected[2 + index] = payload[index];
  }
  expected[lengthField] = module_protocol::crc8Smbus(expected, lengthField);
  TEST_ASSERT_EQUAL(module_protocol::kMaxFrameBytes, port.replyLength);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, port.reply,
                                module_protocol::kMaxFrameBytes);
}

/**
 * Fills an identity payload from the values passed to the link.
 *
 * @param typeId Type id supplied at construction.
 * @param firmwareVersion Firmware version supplied at construction.
 * @param out Destination. Must hold kIdentityPayloadLen bytes.
 * @return Nothing.
 */
void fillIdentity(uint16_t typeId, uint16_t firmwareVersion, uint8_t* out)
{
  out[0] = static_cast<uint8_t>(typeId >> 8);
  out[1] = static_cast<uint8_t>(typeId & 0xFF);
  out[2] = module_protocol::kProtocolVersion;
  out[3] = static_cast<uint8_t>(firmwareVersion >> 8);
  out[4] = static_cast<uint8_t>(firmwareVersion & 0xFF);
}

/**
 * Fills a GET_SENSOR_READING payload.
 *
 * @param index Sensor index echoed in the payload.
 * @param connected Connected flag, 0 or 1.
 * @param value Raw counts.
 * @param out Destination. Must hold kSensorReadingPayloadLen bytes.
 * @return Nothing.
 */
void fillReading(uint8_t index, uint8_t connected, int32_t value, uint8_t* out)
{
  out[0] = index;
  out[1] = connected;
  module_protocol::writeInt32Be(out + 2, value);
}

/**
 * Runs the scan until index has a sample. Fails if that does not
 * happen within a full three-channel cycle.
 *
 * @param board Link fixture whose ADC fake is scripted.
 * @param index Input to wait for.
 * @param counts Value the fake returns for that input.
 * @return Nothing.
 */
void capture(Board& board, uint8_t index, int16_t counts)
{
  board.adc.counts[index] = counts;
  board.adc.ready = true;
  for (uint8_t step = 0; step < 16 && !board.sensors.hasSample(index); ++step)
  {
    board.sensors.update();
  }
  TEST_ASSERT_TRUE(board.sensors.hasSample(index));
}

/**
 * Runs the scan until one more conversion is stored. The board's
 * count should be 1 so the sample lands on index 0.
 *
 * @param board Link fixture whose ADC fake is scripted.
 * @param counts Value the fake returns for input 0.
 * @return Nothing.
 */
void publish(Board& board, int16_t counts)
{
  board.adc.counts[0] = counts;
  board.adc.ready = true;
  const uint8_t reads = board.adc.readCount;
  for (uint8_t step = 0; step < 8 && board.adc.readCount == reads; ++step)
  {
    board.sensors.update();
  }
  TEST_ASSERT_TRUE(board.adc.readCount > reads);
}

/**
 * Stores the next conversion in channel order. The caller sets the
 * fake counts first.
 *
 * @param board Link fixture whose ADC fake is scripted.
 * @return Nothing.
 */
void storeNext(Board& board)
{
  board.adc.ready = true;
  const uint8_t reads = board.adc.readCount;
  for (uint8_t step = 0; step < 8 && board.adc.readCount == reads; ++step)
  {
    board.sensors.update();
  }
  TEST_ASSERT_TRUE(board.adc.readCount > reads);
}

/**
 * Checks a GET_FAULT list.
 *
 * @param board Link fixture that receives the command.
 * @param body Expected records, or nullptr when length is 0.
 * @param length Expected payload length.
 * @return Nothing.
 */
void expectFault(Board& board, const uint8_t* body, uint8_t length)
{
  writeCommand(board.port, module_protocol::kCmdGetFault, nullptr, 0);
  expectReply(board.port, module_protocol::kStatusOk, body, length);
}

}

/**
 * Unity setup hook. Each test builds its own link.
 *
 * @return Nothing.
 */
void setUp()
{
}

/**
 * Unity teardown hook.
 *
 * @return Nothing.
 */
void tearDown()
{
}

/**
 * MOD high at boot leaves the slave disabled.
 *
 * @return Nothing.
 */
void testBootWithModHighLeavesSlaveDisabled()
{
  Board board;

  board.link.update(false);

  TEST_ASSERT_FALSE(board.port.enabled);
  TEST_ASSERT_EQUAL(0, board.port.enableCount);
  TEST_ASSERT_EQUAL(0, board.port.disableCount);
  TEST_ASSERT_EQUAL(0, board.port.setAddressCount);
}

/**
 * MOD low enables 0x0A once. MOD high before assignment disables it.
 *
 * @return Nothing.
 */
void testModLowEnablesUnconfiguredAddressAndModHighDisables()
{
  Board board;

  board.link.update(true);
  board.link.update(true);

  TEST_ASSERT_TRUE(board.port.enabled);
  TEST_ASSERT_EQUAL(1, board.port.enableCount);
  TEST_ASSERT_EQUAL(module_protocol::kUnconfiguredAddress,
                    board.port.lastEnabledAddress);
  TEST_ASSERT_EQUAL(0, board.port.setAddressCount);

  board.link.update(false);
  board.link.update(false);

  TEST_ASSERT_FALSE(board.port.enabled);
  TEST_ASSERT_EQUAL(1, board.port.disableCount);
  TEST_ASSERT_EQUAL(0, board.port.setAddressCount);
}

/**
 * PING returns status Ok, an empty payload, a valid CRC, and 0xFF pad.
 *
 * @return Nothing.
 */
void testPingReplyIsOkAndPadded()
{
  Board board;
  board.link.update(true);

  writeCommand(board.port, module_protocol::kCmdPing, nullptr, 0);

  expectReply(board.port, module_protocol::kStatusOk, nullptr, 0);
  TEST_ASSERT_EQUAL(0, board.port.setAddressCount);
}

/**
 * GET_IDENTITY reports the type and firmware version passed in,
 * with protocol 1.
 *
 * @return Nothing.
 */
void testIdentityReportsSensorModule()
{
  Board board;
  board.link.update(true);

  writeCommand(board.port, module_protocol::kCmdGetIdentity, nullptr, 0);

  uint8_t identity[module_protocol::kIdentityPayloadLen];
  fillIdentity(kTestType, kTestFirmwareVersion, identity);
  expectReply(board.port, module_protocol::kStatusOk, identity,
              module_protocol::kIdentityPayloadLen);
}

/**
 * A type and firmware version other than the product defaults are
 * what GET_IDENTITY returns.
 *
 * @return Nothing.
 */
void testSuppliedFirmwareVersion()
{
  const uint16_t typeId = 0x1234;
  const uint16_t firmwareVersion = 2;
  Board board(0, typeId, firmwareVersion);
  board.link.update(true);

  writeCommand(board.port, module_protocol::kCmdGetIdentity, nullptr, 0);

  uint8_t identity[module_protocol::kIdentityPayloadLen];
  fillIdentity(typeId, firmwareVersion, identity);
  expectReply(board.port, module_protocol::kStatusOk, identity,
              module_protocol::kIdentityPayloadLen);
}

/**
 * SET_ADDRESS commits only while listening, and MOD high keeps it.
 * A write before the slave is enabled does not commit.
 *
 * @return Nothing.
 */
void testSetAddressCommitsAndSurvivesModHigh()
{
  Board board;
  const uint8_t assigned = 0x10;
  const uint8_t later = 0x11;

  writeCommand(board.port, module_protocol::kCmdSetAddress, &assigned, 1);
  TEST_ASSERT_EQUAL(0, board.port.setAddressCount);

  board.link.update(true);
  writeCommand(board.port, module_protocol::kCmdSetAddress, &assigned, 1);

  TEST_ASSERT_EQUAL(1, board.port.setAddressCount);
  TEST_ASSERT_EQUAL(assigned, board.port.lastSetAddress);
  TEST_ASSERT_EQUAL(0, board.port.setReplyCount);
  TEST_ASSERT_TRUE(board.port.enabled);

  board.link.update(false);

  TEST_ASSERT_TRUE(board.port.enabled);
  TEST_ASSERT_EQUAL(0, board.port.disableCount);
  TEST_ASSERT_EQUAL(assigned, board.port.lastSetAddress);

  writeCommand(board.port, module_protocol::kCmdSetAddress, &later, 1);
  TEST_ASSERT_EQUAL(assigned, board.port.lastSetAddress);
  TEST_ASSERT_EQUAL(1, board.port.setAddressCount);

  writeCommand(board.port, module_protocol::kCmdPing, nullptr, 0);
  expectReply(board.port, module_protocol::kStatusOk, nullptr, 0);
}

/**
 * A bad CRC or an address outside 0x10..0x6F leaves the slave at 0x0A.
 *
 * @return Nothing.
 */
void testSetAddressRejectsBadCrcAndIllegalAddresses()
{
  Board board;
  board.link.update(true);

  uint8_t frame[8] = {};
  const uint8_t assigned = 0x10;
  const size_t size = buildFrame(module_protocol::kCmdSetAddress, &assigned, 1,
                                 frame);
  frame[size - 1] ^= 0xFF;
  board.port.deliver(frame, size);

  TEST_ASSERT_EQUAL(0, board.port.setAddressCount);
  TEST_ASSERT_EQUAL(module_protocol::kUnconfiguredAddress,
                    board.port.lastEnabledAddress);
  expectReply(board.port, module_protocol::kStatusBadCrc, nullptr, 0);

  const uint8_t repliesBeforeIllegal = board.port.setReplyCount;
  const uint8_t illegal[] = {0x00, 0x0A, 0x70};
  for (uint8_t index = 0; index < 3; ++index)
  {
    writeCommand(board.port, module_protocol::kCmdSetAddress, &illegal[index],
                 1);
  }
  TEST_ASSERT_EQUAL(0, board.port.setAddressCount);
  TEST_ASSERT_EQUAL(repliesBeforeIllegal, board.port.setReplyCount);
  TEST_ASSERT_TRUE(board.port.enabled);
  TEST_ASSERT_EQUAL(module_protocol::kUnconfiguredAddress,
                    board.port.lastEnabledAddress);

  writeCommand(board.port, module_protocol::kCmdPing, nullptr, 0);
  expectReply(board.port, module_protocol::kStatusOk, nullptr, 0);
}

/**
 * Count is the cache size. A missing sample is Busy. The loops are
 * closed before the samples, so a stored sample below the threshold
 * is disconnected and a sample at the threshold is on. A negative
 * code stays disconnected.
 *
 * @return Nothing.
 */
void testSensorCommandsReportCachedReadings()
{
  Board board(3);
  board.link.update(true);

  writeCommand(board.port, module_protocol::kCmdGetSensorCount, nullptr, 0);
  const uint8_t count[] = {3};
  expectReply(board.port, module_protocol::kStatusOk, count, 1);

  const uint8_t index0 = 0;
  writeCommand(board.port, module_protocol::kCmdGetSensorReading, &index0, 1);
  expectReply(board.port, module_protocol::kStatusBusy, nullptr, 0);
  writeCommand(board.port, module_protocol::kCmdGetSensorConnected, &index0, 1);
  expectReply(board.port, module_protocol::kStatusBusy, nullptr, 0);

  board.sensors.turnOn(0);
  board.sensors.turnOn(1);
  board.sensors.turnOn(2);

  capture(board, 0, 2399);
  writeCommand(board.port, module_protocol::kCmdGetSensorReading, &index0, 1);
  uint8_t reading[module_protocol::kSensorReadingPayloadLen];
  fillReading(0, 0, 2399, reading);
  expectReply(board.port, module_protocol::kStatusOk, reading,
              module_protocol::kSensorReadingPayloadLen);
  writeCommand(board.port, module_protocol::kCmdGetSensorConnected, &index0, 1);
  const uint8_t absent[] = {0, 0};
  expectReply(board.port, module_protocol::kStatusOk, absent, 2);

  capture(board, 1, 2400);
  const uint8_t index1 = 1;
  writeCommand(board.port, module_protocol::kCmdGetSensorReading, &index1, 1);
  fillReading(1, 1, 2400, reading);
  expectReply(board.port, module_protocol::kStatusOk, reading,
              module_protocol::kSensorReadingPayloadLen);
  writeCommand(board.port, module_protocol::kCmdGetSensorConnected, &index1, 1);
  const uint8_t present[] = {1, 1};
  expectReply(board.port, module_protocol::kStatusOk, present, 2);

  capture(board, 2, -4);
  const uint8_t index2 = 2;
  writeCommand(board.port, module_protocol::kCmdGetSensorReading, &index2, 1);
  fillReading(2, 0, -4, reading);
  expectReply(board.port, module_protocol::kStatusOk, reading,
              module_protocol::kSensorReadingPayloadLen);

  writeCommand(board.port, module_protocol::kCmdGetSensorReading, &index0, 1);
  fillReading(0, 0, 2399, reading);
  expectReply(board.port, module_protocol::kStatusOk, reading,
              module_protocol::kSensorReadingPayloadLen);
}

/**
 * State starts off. Set on waits for a sample, then reports
 * disconnected or on. Two samples at the trip latch fault and stay
 * there through set on and set off. Reset leaves the sensor off, and
 * a later set on is accepted. A bad index, a fault or disconnected
 * set value, and a wrong length are BadLength and leave the switches
 * open.
 *
 * @return Nothing.
 */
void testSensorStateCommands()
{
  Board board(1);
  board.link.update(true);

  const uint8_t index0 = 0;
  writeCommand(board.port, module_protocol::kCmdGetSensorState, &index0, 1);
  const uint8_t off[] = {0, module_protocol::kSensorStateOff};
  expectReply(board.port, module_protocol::kStatusOk, off,
              module_protocol::kSensorStatePayloadLen);
  TEST_ASSERT_FALSE(board.switches.on[0]);

  const uint8_t setOn[] = {0, module_protocol::kSensorStateOn};
  writeCommand(board.port, module_protocol::kCmdSetSensor, setOn, 2);
  expectReply(board.port, module_protocol::kStatusBusy, nullptr, 0);
  TEST_ASSERT_TRUE(board.switches.on[0]);

  writeCommand(board.port, module_protocol::kCmdGetSensorState, &index0, 1);
  expectReply(board.port, module_protocol::kStatusBusy, nullptr, 0);

  publish(board, 2399);
  writeCommand(board.port, module_protocol::kCmdGetSensorState, &index0, 1);
  const uint8_t disconnected[] = {
    0, module_protocol::kSensorStateDisconnected};
  expectReply(board.port, module_protocol::kStatusOk, disconnected,
              module_protocol::kSensorStatePayloadLen);
  uint8_t reading[module_protocol::kSensorReadingPayloadLen];
  fillReading(0, 0, 2399, reading);
  writeCommand(board.port, module_protocol::kCmdGetSensorReading, &index0, 1);
  expectReply(board.port, module_protocol::kStatusOk, reading,
              module_protocol::kSensorReadingPayloadLen);

  writeCommand(board.port, module_protocol::kCmdSetSensor, setOn, 2);
  expectReply(board.port, module_protocol::kStatusOk, disconnected,
              module_protocol::kSensorStatePayloadLen);
  TEST_ASSERT_TRUE(board.switches.on[0]);

  publish(board, 2400);
  writeCommand(board.port, module_protocol::kCmdGetSensorState, &index0, 1);
  const uint8_t on[] = {0, module_protocol::kSensorStateOn};
  expectReply(board.port, module_protocol::kStatusOk, on,
              module_protocol::kSensorStatePayloadLen);
  fillReading(0, 1, 2400, reading);
  writeCommand(board.port, module_protocol::kCmdGetSensorReading, &index0, 1);
  expectReply(board.port, module_protocol::kStatusOk, reading,
              module_protocol::kSensorReadingPayloadLen);

  const uint8_t setOff[] = {0, module_protocol::kSensorStateOff};
  writeCommand(board.port, module_protocol::kCmdSetSensor, setOff, 2);
  expectReply(board.port, module_protocol::kStatusOk, off,
              module_protocol::kSensorStatePayloadLen);
  TEST_ASSERT_FALSE(board.switches.on[0]);
  fillReading(0, 0, 2400, reading);
  writeCommand(board.port, module_protocol::kCmdGetSensorReading, &index0, 1);
  expectReply(board.port, module_protocol::kStatusOk, reading,
              module_protocol::kSensorReadingPayloadLen);

  writeCommand(board.port, module_protocol::kCmdSetSensor, setOn, 2);
  expectReply(board.port, module_protocol::kStatusBusy, nullptr, 0);
  publish(board, static_cast<int16_t>(kTripCounts));
  writeCommand(board.port, module_protocol::kCmdGetSensorState, &index0, 1);
  expectReply(board.port, module_protocol::kStatusOk, on,
              module_protocol::kSensorStatePayloadLen);
  TEST_ASSERT_TRUE(board.switches.on[0]);

  publish(board, static_cast<int16_t>(kTripCounts));
  writeCommand(board.port, module_protocol::kCmdGetSensorState, &index0, 1);
  const uint8_t fault[] = {0, module_protocol::kSensorStateFault};
  expectReply(board.port, module_protocol::kStatusOk, fault,
              module_protocol::kSensorStatePayloadLen);
  TEST_ASSERT_FALSE(board.switches.on[0]);
  fillReading(0, 0, kTripCounts, reading);
  writeCommand(board.port, module_protocol::kCmdGetSensorReading, &index0, 1);
  expectReply(board.port, module_protocol::kStatusOk, reading,
              module_protocol::kSensorReadingPayloadLen);

  writeCommand(board.port, module_protocol::kCmdSetSensor, setOn, 2);
  expectReply(board.port, module_protocol::kStatusOk, fault,
              module_protocol::kSensorStatePayloadLen);
  writeCommand(board.port, module_protocol::kCmdSetSensor, setOff, 2);
  expectReply(board.port, module_protocol::kStatusOk, fault,
              module_protocol::kSensorStatePayloadLen);
  TEST_ASSERT_FALSE(board.switches.on[0]);

  writeCommand(board.port, module_protocol::kCmdResetSensor, &index0, 1);
  expectReply(board.port, module_protocol::kStatusOk, off,
              module_protocol::kSensorStatePayloadLen);
  TEST_ASSERT_FALSE(board.switches.on[0]);

  writeCommand(board.port, module_protocol::kCmdSetSensor, setOn, 2);
  expectReply(board.port, module_protocol::kStatusBusy, nullptr, 0);
  TEST_ASSERT_TRUE(board.switches.on[0]);
  publish(board, 4800);
  writeCommand(board.port, module_protocol::kCmdGetSensorState, &index0, 1);
  expectReply(board.port, module_protocol::kStatusOk, on,
              module_protocol::kSensorStatePayloadLen);

  Board wide(3);
  wide.link.update(true);
  const uint8_t badIndex[] = {3, module_protocol::kSensorStateOn};
  writeCommand(wide.port, module_protocol::kCmdSetSensor, badIndex, 2);
  expectReply(wide.port, module_protocol::kStatusBadLength, nullptr, 0);
  const uint8_t faultValue[] = {0, module_protocol::kSensorStateFault};
  writeCommand(wide.port, module_protocol::kCmdSetSensor, faultValue, 2);
  expectReply(wide.port, module_protocol::kStatusBadLength, nullptr, 0);
  const uint8_t disconnectedValue[] = {
    0, module_protocol::kSensorStateDisconnected};
  writeCommand(wide.port, module_protocol::kCmdSetSensor, disconnectedValue, 2);
  expectReply(wide.port, module_protocol::kStatusBadLength, nullptr, 0);
  writeCommand(wide.port, module_protocol::kCmdSetSensor, nullptr, 0);
  expectReply(wide.port, module_protocol::kStatusBadLength, nullptr, 0);
  const uint8_t extra[] = {0, module_protocol::kSensorStateOn, 0};
  writeCommand(wide.port, module_protocol::kCmdSetSensor, extra, 3);
  expectReply(wide.port, module_protocol::kStatusBadLength, nullptr, 0);
  writeCommand(wide.port, module_protocol::kCmdResetSensor, nullptr, 0);
  expectReply(wide.port, module_protocol::kStatusBadLength, nullptr, 0);
  writeCommand(wide.port, module_protocol::kCmdResetSensor, extra, 2);
  expectReply(wide.port, module_protocol::kStatusBadLength, nullptr, 0);
  writeCommand(wide.port, module_protocol::kCmdGetSensorState, nullptr, 0);
  expectReply(wide.port, module_protocol::kStatusBadLength, nullptr, 0);
  const uint8_t index3 = 3;
  writeCommand(wide.port, module_protocol::kCmdGetSensorState, &index3, 1);
  expectReply(wide.port, module_protocol::kStatusBadLength, nullptr, 0);
  writeCommand(wide.port, module_protocol::kCmdResetSensor, &index3, 1);
  expectReply(wide.port, module_protocol::kStatusBadLength, nullptr, 0);
  TEST_ASSERT_FALSE(wide.switches.on[0]);
  TEST_ASSERT_EQUAL(0, wide.switches.setCount);

  writeCommand(wide.port, module_protocol::kCmdSetSensor, setOn, 2);
  expectReply(wide.port, module_protocol::kStatusBusy, nullptr, 0);
  TEST_ASSERT_TRUE(wide.switches.on[0]);
  TEST_ASSERT_FALSE(wide.switches.on[1]);
  const uint8_t index1 = 1;
  writeCommand(wide.port, module_protocol::kCmdGetSensorState, &index1, 1);
  const uint8_t channel1Off[] = {1, module_protocol::kSensorStateOff};
  expectReply(wide.port, module_protocol::kStatusOk, channel1Off,
              module_protocol::kSensorStatePayloadLen);
}

/**
 * GET_FAULT before a fault lists every input as reason 0. A count
 * of 0 is an empty Ok payload.
 *
 * @return Nothing.
 */
void testGetFaultWhenIdle()
{
  Board board(3);
  board.link.update(true);

  const uint8_t none = module_protocol::kFaultNone;
  const uint8_t clear[] = {none, 0, none, 1, none, 2};
  expectFault(board, clear, sizeof(clear));

  Board empty;
  empty.link.update(true);
  expectFault(empty, nullptr, 0);
}

/**
 * One trip sample stays clear. The second sample on input 2 sets
 * that record to overcurrent. A disconnected input stays reason 0.
 * Faulting input 0 sets its record too. Reset clears one record
 * at a time.
 *
 * @return Nothing.
 */
void testGetFaultReportsEveryInput()
{
  Board board(3);
  board.link.update(true);
  board.sensors.turnOn(2);
  board.adc.counts[0] = static_cast<int16_t>(kTripCounts);
  board.adc.counts[1] = 2399;
  board.adc.counts[2] = static_cast<int16_t>(kTripCounts);

  const uint8_t none = module_protocol::kFaultNone;
  const uint8_t trip = module_protocol::kSensorFaultOvercurrent;
  const uint8_t clear[] = {none, 0, none, 1, none, 2};
  const uint8_t input2[] = {none, 0, none, 1, trip, 2};
  const uint8_t both[] = {trip, 0, none, 1, trip, 2};

  storeNext(board);
  storeNext(board);
  storeNext(board);
  expectFault(board, clear, sizeof(clear));
  TEST_ASSERT_TRUE(board.switches.on[2]);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateOn,
                          board.sensors.state(2));

  storeNext(board);
  storeNext(board);
  storeNext(board);
  expectFault(board, input2, sizeof(input2));
  TEST_ASSERT_FALSE(board.switches.on[2]);

  board.sensors.turnOn(1);
  storeNext(board);
  storeNext(board);
  expectFault(board, input2, sizeof(input2));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateDisconnected,
                          board.sensors.state(1));
  TEST_ASSERT_TRUE(board.switches.on[1]);

  board.sensors.turnOn(0);
  storeNext(board);
  storeNext(board);
  storeNext(board);
  storeNext(board);
  storeNext(board);
  expectFault(board, both, sizeof(both));
  TEST_ASSERT_FALSE(board.switches.on[0]);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          board.sensors.state(2));

  const uint8_t index0 = 0;
  writeCommand(board.port, module_protocol::kCmdResetSensor, &index0, 1);
  const uint8_t off[] = {0, module_protocol::kSensorStateOff};
  expectReply(board.port, module_protocol::kStatusOk, off,
              module_protocol::kSensorStatePayloadLen);
  expectFault(board, input2, sizeof(input2));

  const uint8_t index2 = 2;
  writeCommand(board.port, module_protocol::kCmdResetSensor, &index2, 1);
  const uint8_t cleared[] = {2, module_protocol::kSensorStateOff};
  expectReply(board.port, module_protocol::kStatusOk, cleared,
              module_protocol::kSensorStatePayloadLen);
  expectFault(board, clear, sizeof(clear));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateDisconnected,
                          board.sensors.state(1));
}

/**
 * A payload on GET_FAULT is BadLength. The latched fault and the
 * open switch stay as they are.
 *
 * @return Nothing.
 */
void testGetFaultRejectsPayload()
{
  Board board(1);
  board.link.update(true);
  board.sensors.turnOn(0);
  publish(board, static_cast<int16_t>(kTripCounts));
  publish(board, static_cast<int16_t>(kTripCounts));
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          board.sensors.state(0));
  TEST_ASSERT_FALSE(board.switches.on[0]);
  const uint8_t sets = board.switches.setCount;

  const uint8_t extra = 0;
  writeCommand(board.port, module_protocol::kCmdGetFault, &extra, 1);
  expectReply(board.port, module_protocol::kStatusBadLength, nullptr, 0);
  TEST_ASSERT_EQUAL_UINT8(module_protocol::kSensorStateFault,
                          board.sensors.state(0));
  TEST_ASSERT_FALSE(board.switches.on[0]);
  TEST_ASSERT_EQUAL(sets, board.switches.setCount);
  const uint8_t latched[] = {module_protocol::kSensorFaultOvercurrent, 0};
  expectFault(board, latched, sizeof(latched));
}

/**
 * Eight inputs fill the payload. Nine do not fit, so the reply is
 * Unsupported and carries no partial list.
 *
 * @return Nothing.
 */
void testGetFaultRejectsTooManyInputs()
{
  const uint8_t fit = static_cast<uint8_t>(
      module_protocol::kMaxPayload / module_protocol::kFaultRecordLen);
  Board full(fit);
  full.link.update(true);
  uint8_t body[module_protocol::kMaxPayload];
  for (uint8_t index = 0; index < fit; ++index)
  {
    body[static_cast<uint8_t>(index * 2U)] = module_protocol::kFaultNone;
    body[static_cast<uint8_t>(index * 2U + 1U)] = index;
  }
  expectFault(full, body, module_protocol::kMaxPayload);

  Board over(static_cast<uint8_t>(fit + 1U));
  over.link.update(true);
  writeCommand(over.port, module_protocol::kCmdGetFault, nullptr, 0);
  expectReply(over.port, module_protocol::kStatusUnsupported, nullptr, 0);
}

/**
 * An index outside the count, a payload that is not one byte, and a
 * count command with a payload are BadLength. A count of 0 rejects
 * index 0 the same way.
 *
 * @return Nothing.
 */
void testSensorCommandsRejectBadIndexAndLength()
{
  Board board(3);
  board.link.update(true);

  const uint8_t index = 3;
  writeCommand(board.port, module_protocol::kCmdGetSensorReading, &index, 1);
  expectReply(board.port, module_protocol::kStatusBadLength, nullptr, 0);
  writeCommand(board.port, module_protocol::kCmdGetSensorConnected, &index, 1);
  expectReply(board.port, module_protocol::kStatusBadLength, nullptr, 0);

  writeCommand(board.port, module_protocol::kCmdGetSensorReading, nullptr, 0);
  expectReply(board.port, module_protocol::kStatusBadLength, nullptr, 0);
  const uint8_t extra[] = {0, 1};
  writeCommand(board.port, module_protocol::kCmdGetSensorReading, extra, 2);
  expectReply(board.port, module_protocol::kStatusBadLength, nullptr, 0);
  writeCommand(board.port, module_protocol::kCmdGetSensorConnected, extra, 2);
  expectReply(board.port, module_protocol::kStatusBadLength, nullptr, 0);
  writeCommand(board.port, module_protocol::kCmdGetSensorCount, &index, 1);
  expectReply(board.port, module_protocol::kStatusBadLength, nullptr, 0);

  Board empty;
  empty.link.update(true);
  writeCommand(empty.port, module_protocol::kCmdGetSensorCount, nullptr, 0);
  const uint8_t count[] = {0};
  expectReply(empty.port, module_protocol::kStatusOk, count, 1);
  const uint8_t zero = 0;
  writeCommand(empty.port, module_protocol::kCmdGetSensorReading, &zero, 1);
  expectReply(empty.port, module_protocol::kStatusBadLength, nullptr, 0);
  writeCommand(empty.port, module_protocol::kCmdGetSensorConnected, &zero, 1);
  expectReply(empty.port, module_protocol::kStatusBadLength, nullptr, 0);
}

/**
 * ECHO is unknown. A short frame, a mismatched length, a bad CRC, a
 * PING with a payload, and an empty SET_ADDRESS are rejected.
 *
 * @return Nothing.
 */
void testUnknownCommandAndMalformedFrames()
{
  Board board;
  board.link.update(true);

  const uint8_t echoByte = 0x5A;
  writeCommand(board.port, module_protocol::kCmdEcho, &echoByte, 1);
  expectReply(board.port, module_protocol::kStatusUnknownCmd, nullptr, 0);

  const uint8_t truncated[] = {0x02, 0x01};
  board.port.deliver(truncated, sizeof(truncated));
  expectReply(board.port, module_protocol::kStatusBadLength, nullptr, 0);

  uint8_t ping[8] = {};
  const size_t pingSize = buildFrame(module_protocol::kCmdPing, nullptr, 0,
                                     ping);
  ping[pingSize] = 0x00;
  board.port.deliver(ping, pingSize + 1U);
  expectReply(board.port, module_protocol::kStatusBadLength, nullptr, 0);

  ping[2] ^= 0xFF;
  board.port.deliver(ping, pingSize);
  expectReply(board.port, module_protocol::kStatusBadCrc, nullptr, 0);

  const uint8_t extra = 0x01;
  writeCommand(board.port, module_protocol::kCmdPing, &extra, 1);
  expectReply(board.port, module_protocol::kStatusBadLength, nullptr, 0);

  writeCommand(board.port, module_protocol::kCmdSetAddress, nullptr, 0);
  expectReply(board.port, module_protocol::kStatusBadLength, nullptr, 0);
  TEST_ASSERT_EQUAL(0, board.port.setAddressCount);
  TEST_ASSERT_EQUAL(module_protocol::kUnconfiguredAddress,
                    board.port.lastEnabledAddress);
}

/**
 * MOD low, PING, SET_ADDRESS, MOD high, identity, then a count of 3.
 *
 * @return Nothing.
 */
void testEnumerationSequence()
{
  Board board(3);
  const uint8_t assigned = 0x12;

  board.link.update(true);
  writeCommand(board.port, module_protocol::kCmdPing, nullptr, 0);
  expectReply(board.port, module_protocol::kStatusOk, nullptr, 0);

  writeCommand(board.port, module_protocol::kCmdSetAddress, &assigned, 1);
  board.link.update(false);

  TEST_ASSERT_EQUAL(assigned, board.port.lastSetAddress);
  TEST_ASSERT_TRUE(board.port.enabled);
  TEST_ASSERT_EQUAL(0, board.port.disableCount);
  TEST_ASSERT_EQUAL(1, board.port.enableCount);

  writeCommand(board.port, module_protocol::kCmdGetIdentity, nullptr, 0);
  uint8_t identity[module_protocol::kIdentityPayloadLen];
  fillIdentity(kTestType, kTestFirmwareVersion, identity);
  expectReply(board.port, module_protocol::kStatusOk, identity,
              module_protocol::kIdentityPayloadLen);

  writeCommand(board.port, module_protocol::kCmdGetSensorCount, nullptr, 0);
  const uint8_t count[] = {3};
  expectReply(board.port, module_protocol::kStatusOk, count, 1);
}

/**
 * Runs the sensor-input tests defined in the other translation unit.
 *
 * @return Nothing.
 */
void runSensorInputTests();

/**
 * Runs the module-link desktop tests.
 *
 * @return Unity result.
 */
int main()
{
  UNITY_BEGIN();
  RUN_TEST(testBootWithModHighLeavesSlaveDisabled);
  RUN_TEST(testModLowEnablesUnconfiguredAddressAndModHighDisables);
  RUN_TEST(testPingReplyIsOkAndPadded);
  RUN_TEST(testIdentityReportsSensorModule);
  RUN_TEST(testSuppliedFirmwareVersion);
  RUN_TEST(testSetAddressCommitsAndSurvivesModHigh);
  RUN_TEST(testSetAddressRejectsBadCrcAndIllegalAddresses);
  RUN_TEST(testSensorCommandsReportCachedReadings);
  RUN_TEST(testSensorStateCommands);
  RUN_TEST(testGetFaultWhenIdle);
  RUN_TEST(testGetFaultReportsEveryInput);
  RUN_TEST(testGetFaultRejectsPayload);
  RUN_TEST(testGetFaultRejectsTooManyInputs);
  RUN_TEST(testSensorCommandsRejectBadIndexAndLength);
  RUN_TEST(testUnknownCommandAndMalformedFrames);
  RUN_TEST(testEnumerationSequence);
  runSensorInputTests();
  return UNITY_END();
}
