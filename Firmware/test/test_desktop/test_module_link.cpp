#include <unity.h>

#include "ModuleLink.h"
#include "ModuleProtocol.h"
#include "fakes/FakeModuleSlavePort.h"

namespace
{

constexpr uint16_t kTestType = module_protocol::kTypeSensorModule;
constexpr uint16_t kTestFirmwareVersion = 1;

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
  FakeModuleSlavePort port;
  ModuleLink link(port, kTestType, kTestFirmwareVersion);

  link.update(false);

  TEST_ASSERT_FALSE(port.enabled);
  TEST_ASSERT_EQUAL(0, port.enableCount);
  TEST_ASSERT_EQUAL(0, port.disableCount);
  TEST_ASSERT_EQUAL(0, port.setAddressCount);
}

/**
 * MOD low enables 0x0A once. MOD high before assignment disables it.
 *
 * @return Nothing.
 */
void testModLowEnablesUnconfiguredAddressAndModHighDisables()
{
  FakeModuleSlavePort port;
  ModuleLink link(port, kTestType, kTestFirmwareVersion);

  link.update(true);
  link.update(true);

  TEST_ASSERT_TRUE(port.enabled);
  TEST_ASSERT_EQUAL(1, port.enableCount);
  TEST_ASSERT_EQUAL(module_protocol::kUnconfiguredAddress,
                    port.lastEnabledAddress);
  TEST_ASSERT_EQUAL(0, port.setAddressCount);

  link.update(false);
  link.update(false);

  TEST_ASSERT_FALSE(port.enabled);
  TEST_ASSERT_EQUAL(1, port.disableCount);
  TEST_ASSERT_EQUAL(0, port.setAddressCount);
}

/**
 * PING returns status Ok, an empty payload, a valid CRC, and 0xFF pad.
 *
 * @return Nothing.
 */
void testPingReplyIsOkAndPadded()
{
  FakeModuleSlavePort port;
  ModuleLink link(port, kTestType, kTestFirmwareVersion);
  link.update(true);

  writeCommand(port, module_protocol::kCmdPing, nullptr, 0);

  expectReply(port, module_protocol::kStatusOk, nullptr, 0);
  TEST_ASSERT_EQUAL(0, port.setAddressCount);
}

/**
 * GET_IDENTITY reports the type and firmware version passed in,
 * with protocol 1.
 *
 * @return Nothing.
 */
void testIdentityReportsSensorModule()
{
  FakeModuleSlavePort port;
  ModuleLink link(port, kTestType, kTestFirmwareVersion);
  link.update(true);

  writeCommand(port, module_protocol::kCmdGetIdentity, nullptr, 0);

  uint8_t identity[module_protocol::kIdentityPayloadLen];
  fillIdentity(kTestType, kTestFirmwareVersion, identity);
  expectReply(port, module_protocol::kStatusOk, identity,
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
  FakeModuleSlavePort port;
  const uint16_t typeId = 0x1234;
  const uint16_t firmwareVersion = 2;
  ModuleLink link(port, typeId, firmwareVersion);
  link.update(true);

  writeCommand(port, module_protocol::kCmdGetIdentity, nullptr, 0);

  uint8_t identity[module_protocol::kIdentityPayloadLen];
  fillIdentity(typeId, firmwareVersion, identity);
  expectReply(port, module_protocol::kStatusOk, identity,
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
  FakeModuleSlavePort port;
  ModuleLink link(port, kTestType, kTestFirmwareVersion);
  const uint8_t assigned = 0x10;
  const uint8_t later = 0x11;

  writeCommand(port, module_protocol::kCmdSetAddress, &assigned, 1);
  TEST_ASSERT_EQUAL(0, port.setAddressCount);

  link.update(true);
  writeCommand(port, module_protocol::kCmdSetAddress, &assigned, 1);

  TEST_ASSERT_EQUAL(1, port.setAddressCount);
  TEST_ASSERT_EQUAL(assigned, port.lastSetAddress);
  TEST_ASSERT_EQUAL(0, port.setReplyCount);
  TEST_ASSERT_TRUE(port.enabled);

  link.update(false);

  TEST_ASSERT_TRUE(port.enabled);
  TEST_ASSERT_EQUAL(0, port.disableCount);
  TEST_ASSERT_EQUAL(assigned, port.lastSetAddress);

  writeCommand(port, module_protocol::kCmdSetAddress, &later, 1);
  TEST_ASSERT_EQUAL(assigned, port.lastSetAddress);
  TEST_ASSERT_EQUAL(1, port.setAddressCount);

  writeCommand(port, module_protocol::kCmdPing, nullptr, 0);
  expectReply(port, module_protocol::kStatusOk, nullptr, 0);
}

/**
 * A bad CRC or an address outside 0x10..0x6F leaves the slave at 0x0A.
 *
 * @return Nothing.
 */
void testSetAddressRejectsBadCrcAndIllegalAddresses()
{
  FakeModuleSlavePort port;
  ModuleLink link(port, kTestType, kTestFirmwareVersion);
  link.update(true);

  uint8_t frame[8] = {};
  const uint8_t assigned = 0x10;
  const size_t size = buildFrame(module_protocol::kCmdSetAddress, &assigned, 1,
                                 frame);
  frame[size - 1] ^= 0xFF;
  port.deliver(frame, size);

  TEST_ASSERT_EQUAL(0, port.setAddressCount);
  TEST_ASSERT_EQUAL(module_protocol::kUnconfiguredAddress,
                    port.lastEnabledAddress);
  expectReply(port, module_protocol::kStatusBadCrc, nullptr, 0);

  const uint8_t repliesBeforeIllegal = port.setReplyCount;
  const uint8_t illegal[] = {0x00, 0x0A, 0x70};
  for (uint8_t index = 0; index < 3; ++index)
  {
    writeCommand(port, module_protocol::kCmdSetAddress, &illegal[index], 1);
  }
  TEST_ASSERT_EQUAL(0, port.setAddressCount);
  TEST_ASSERT_EQUAL(repliesBeforeIllegal, port.setReplyCount);
  TEST_ASSERT_TRUE(port.enabled);
  TEST_ASSERT_EQUAL(module_protocol::kUnconfiguredAddress,
                    port.lastEnabledAddress);

  writeCommand(port, module_protocol::kCmdPing, nullptr, 0);
  expectReply(port, module_protocol::kStatusOk, nullptr, 0);
}

/**
 * The module reports no sensors. A reading or presence query is BadLength.
 *
 * @return Nothing.
 */
void testSensorCountIsZeroAndReadingIsBadLength()
{
  FakeModuleSlavePort port;
  ModuleLink link(port, kTestType, kTestFirmwareVersion);
  link.update(true);

  writeCommand(port, module_protocol::kCmdGetSensorCount, nullptr, 0);
  const uint8_t count[] = {0};
  expectReply(port, module_protocol::kStatusOk, count, 1);

  const uint8_t index = 0;
  writeCommand(port, module_protocol::kCmdGetSensorReading, &index, 1);
  expectReply(port, module_protocol::kStatusBadLength, nullptr, 0);

  writeCommand(port, module_protocol::kCmdGetSensorConnected, &index, 1);
  expectReply(port, module_protocol::kStatusBadLength, nullptr, 0);
}

/**
 * ECHO is unknown. A short frame, a mismatched length, a bad CRC, a
 * PING with a payload, and an empty SET_ADDRESS are rejected.
 *
 * @return Nothing.
 */
void testUnknownCommandAndMalformedFrames()
{
  FakeModuleSlavePort port;
  ModuleLink link(port, kTestType, kTestFirmwareVersion);
  link.update(true);

  const uint8_t echoByte = 0x5A;
  writeCommand(port, module_protocol::kCmdEcho, &echoByte, 1);
  expectReply(port, module_protocol::kStatusUnknownCmd, nullptr, 0);

  const uint8_t truncated[] = {0x02, 0x01};
  port.deliver(truncated, sizeof(truncated));
  expectReply(port, module_protocol::kStatusBadLength, nullptr, 0);

  uint8_t ping[8] = {};
  const size_t pingSize = buildFrame(module_protocol::kCmdPing, nullptr, 0,
                                     ping);
  ping[pingSize] = 0x00;
  port.deliver(ping, pingSize + 1U);
  expectReply(port, module_protocol::kStatusBadLength, nullptr, 0);

  ping[2] ^= 0xFF;
  port.deliver(ping, pingSize);
  expectReply(port, module_protocol::kStatusBadCrc, nullptr, 0);

  const uint8_t extra = 0x01;
  writeCommand(port, module_protocol::kCmdPing, &extra, 1);
  expectReply(port, module_protocol::kStatusBadLength, nullptr, 0);

  writeCommand(port, module_protocol::kCmdSetAddress, nullptr, 0);
  expectReply(port, module_protocol::kStatusBadLength, nullptr, 0);
  TEST_ASSERT_EQUAL(0, port.setAddressCount);
  TEST_ASSERT_EQUAL(module_protocol::kUnconfiguredAddress,
                    port.lastEnabledAddress);
}

/**
 * MOD low, PING, SET_ADDRESS, MOD high, identity, then a zero count.
 *
 * @return Nothing.
 */
void testEnumerationSequence()
{
  FakeModuleSlavePort port;
  ModuleLink link(port, kTestType, kTestFirmwareVersion);
  const uint8_t assigned = 0x12;

  link.update(true);
  writeCommand(port, module_protocol::kCmdPing, nullptr, 0);
  expectReply(port, module_protocol::kStatusOk, nullptr, 0);

  writeCommand(port, module_protocol::kCmdSetAddress, &assigned, 1);
  link.update(false);

  TEST_ASSERT_EQUAL(assigned, port.lastSetAddress);
  TEST_ASSERT_TRUE(port.enabled);
  TEST_ASSERT_EQUAL(0, port.disableCount);
  TEST_ASSERT_EQUAL(1, port.enableCount);

  writeCommand(port, module_protocol::kCmdGetIdentity, nullptr, 0);
  uint8_t identity[module_protocol::kIdentityPayloadLen];
  fillIdentity(kTestType, kTestFirmwareVersion, identity);
  expectReply(port, module_protocol::kStatusOk, identity,
              module_protocol::kIdentityPayloadLen);

  writeCommand(port, module_protocol::kCmdGetSensorCount, nullptr, 0);
  const uint8_t count[] = {0};
  expectReply(port, module_protocol::kStatusOk, count, 1);
}

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
  RUN_TEST(testSensorCountIsZeroAndReadingIsBadLength);
  RUN_TEST(testUnknownCommandAndMalformedFrames);
  RUN_TEST(testEnumerationSequence);
  return UNITY_END();
}
