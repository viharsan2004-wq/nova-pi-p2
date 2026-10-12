#pragma once
#include <Arduino.h>

/* Minimal ADS1256 driver (SPI mode 1, SDATAC + RDATA command reads). Single channel. */
class ADS1256 {
 public:
  struct Diag { uint32_t reads = 0, timeouts = 0, cfgFail = 0; uint8_t id = 0xFF; };
  Diag diag;

  bool begin();                                              /* pins, SPI, reset, ID check */
  bool configure(uint8_t drate, uint8_t gainCode, bool buf); /* regs + MUX + self-cal */
  bool waitDrdyLow(uint32_t timeout_us);                     /* counts timeouts */
  bool drdyIsLow() const;
  bool readSample(int32_t& code);                            /* DRDY must already be low */
  void softSync();                                           /* SYNC + WAKEUP commands */
  uint8_t readReg(uint8_t reg);
  void writeReg(uint8_t reg, uint8_t val);

  static bool     drateValid(uint8_t code);
  static uint16_t drateToSps(uint8_t code);
  static uint32_t settleUs(uint8_t code);  /* approx. filter settling after SYNC (5 conversions) */

 private:
  void cmd(uint8_t c);
  void txBegin();
  void txEnd();
  bool waitDrdyHigh(uint32_t timeout_us);
};
