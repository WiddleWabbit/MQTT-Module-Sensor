#pragma once

#include <stdint.h>

#include "IModuleSlavePort.h"
#include "ModuleProtocol.h"

/**
 * TWI1 slave on PE0 (SCL) and PE1 (SDA).
 *
 * MiniCore names those pins SDA1 and SCL1 in the opposite order. The
 * peripheral itself is PE0 = SCL and PE1 = SDA, which is how this PCB
 * wires the motherboard header. Wire1 enables that peripheral.
 * setAddress writes TWAR1 directly so the new address is committed
 * from the stop callback without resetting the bus. General call stays
 * off because bit 0 of TWAR1 stays clear.
 */
class AvrTwi1Slave : public IModuleSlavePort
{
public:
  /**
   * Creates a disabled slave. The link registers the write handler
   * before MOD can go low.
   */
  AvrTwi1Slave();

  /**
   * Starts Wire1 at address7bit and registers the receive and request
   * callbacks.
   *
   * @param address7bit 7-bit address to acknowledge.
   * @return Nothing.
   */
  void enable(uint8_t address7bit) override;

  /**
   * Stops TWI1. The pins return to inputs with pull-ups off, so the
   * motherboard's 4.7 kΩ resistors hold the bus.
   *
   * @return Nothing.
   */
  void disable() override;

  /**
   * Writes TWAR1. Safe from the stop callback.
   *
   * @param address7bit Assigned 7-bit address.
   * @return Nothing.
   */
  void setAddress(uint8_t address7bit) override;

  /**
   * Copies the reply sent on the next host read.
   *
   * @param frame Reply bytes, including 0xFF pad.
   * @param length Number of bytes in frame, at most 19.
   * @return Nothing.
   */
  void setReply(const uint8_t* frame, uint8_t length) override;

  /**
   * Stores the single protocol handler. Wire's callbacks are plain
   * function pointers, so one driver instance is active.
   *
   * @param handler Function called with one master write.
   * @param context Pointer passed back to handler on each write.
   * @return Nothing.
   */
  void setMasterWriteHandler(MasterWriteFn handler, void* context) override;

private:
  /**
   * Reads the write and calls the registered handler. Runs from the
   * TWI interrupt.
   *
   * @param count Bytes Wire1 reported.
   * @return Nothing.
   */
  static void _onReceive(int count);

  /**
   * Clocks out the reply stored by setReply. Runs from the TWI
   * interrupt after a repeated start.
   *
   * @return Nothing.
   */
  static void _onRequest();

  static AvrTwi1Slave* _instance;
  MasterWriteFn _writeHandler;
  void* _writeContext;
  uint8_t _reply[module_protocol::kMaxFrameBytes];
  uint8_t _replyLength;
  bool _enabled;
};
