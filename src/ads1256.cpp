#include "ads1256.h"
#include "config.h"
#include <SPI.h>
#include <esp_timer.h>
#include <driver/gpio.h>

static SPIClass s_spi(FSPI);

namespace {
constexpr uint8_t CMD_WAKEUP = 0x00, CMD_RDATA = 0x01, CMD_SDATAC = 0x0F;
constexpr uint8_t CMD_RREG = 0x10, CMD_WREG = 0x50, CMD_SELFCAL = 0xF0, CMD_SYNC = 0xFC, CMD_RESET = 0xFE;
constexpr uint8_t REG_STATUS = 0, REG_MUX = 1, REG_ADCON = 2, REG_DRATE = 3;
struct Rate { uint8_t code; uint16_t sps; };
const Rate RATES[] = {{0xF0,30000},{0xE0,15000},{0xD0,7500},{0xC0,3750},{0xB0,2000},{0xA1,1000},{0x92,500},{0x82,100}};
/* t6/t11 >= 50 tCLKIN between command and data */
constexpr uint32_t T6_US = (50UL * 1000000UL + ADC_CLKIN_HZ - 1) / ADC_CLKIN_HZ + 3;
constexpr uint32_t SPI_HZ = (ADC_SPI_HZ < ADC_CLKIN_HZ / 4) ? ADC_SPI_HZ : ADC_CLKIN_HZ / 4;
}

bool ADS1256::drateValid(uint8_t c) { for (auto& r : RATES) if (r.code == c) return true; return false; }
uint16_t ADS1256::drateToSps(uint8_t c) { for (auto& r : RATES) if (r.code == c) return r.sps; return 30000; }
uint32_t ADS1256::settleUs(uint8_t c) { return 5000000UL / drateToSps(c) + 20; }

void ADS1256::txBegin() {
  s_spi.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE1));
  digitalWrite(PIN_ADC_CS, LOW);
  delayMicroseconds(1);
}
void ADS1256::txEnd() {
  delayMicroseconds(1);
  digitalWrite(PIN_ADC_CS, HIGH);
  s_spi.endTransaction();
}
void ADS1256::cmd(uint8_t c) { txBegin(); s_spi.transfer(c); txEnd(); }

uint8_t ADS1256::readReg(uint8_t reg) {
  txBegin();
  s_spi.transfer(CMD_RREG | (reg & 0x0F));
  s_spi.transfer(0x00);
  delayMicroseconds(T6_US);
  uint8_t v = s_spi.transfer(0x00);
  txEnd();
  return v;
}
void ADS1256::writeReg(uint8_t reg, uint8_t val) {
  txBegin();
  s_spi.transfer(CMD_WREG | (reg & 0x0F));
  s_spi.transfer(0x00);
  s_spi.transfer(val);
  txEnd();
  delayMicroseconds(T6_US);
}

bool ADS1256::drdyIsLow() const { return gpio_get_level((gpio_num_t)PIN_ADC_DRDY) == 0; }

bool ADS1256::waitDrdyLow(uint32_t timeout_us) {
  const int64_t end = esp_timer_get_time() + (int64_t)timeout_us;
  do { if (drdyIsLow()) return true; } while (esp_timer_get_time() < end);
  diag.timeouts++;
  return false;
}
bool ADS1256::waitDrdyHigh(uint32_t timeout_us) {
  const int64_t end = esp_timer_get_time() + (int64_t)timeout_us;
  do { if (!drdyIsLow()) return true; } while (esp_timer_get_time() < end);
  return false;
}

bool ADS1256::readSample(int32_t& code) {
  txBegin();
  s_spi.transfer(CMD_RDATA);
  delayMicroseconds(T6_US);
  uint8_t b0 = s_spi.transfer(0), b1 = s_spi.transfer(0), b2 = s_spi.transfer(0);
  txEnd();
  int32_t v = ((int32_t)b0 << 16) | ((int32_t)b1 << 8) | b2;
  if (v & 0x800000) v -= 0x1000000;
  code = v;
  diag.reads++;
  return true;
}

void ADS1256::softSync() { cmd(CMD_SYNC); delayMicroseconds(T6_US); cmd(CMD_WAKEUP); }

bool ADS1256::begin() {
  if (!cfgAdcPinsOk()) return false;
  pinMode(PIN_ADC_CS, OUTPUT); digitalWrite(PIN_ADC_CS, HIGH);
  pinMode(PIN_ADC_DRDY, INPUT);
  s_spi.begin(PIN_ADC_SCK, PIN_ADC_MISO, PIN_ADC_MOSI, -1);
  if (PIN_ADC_RESET >= 0) {
    pinMode(PIN_ADC_RESET, OUTPUT);
    digitalWrite(PIN_ADC_RESET, LOW); delay(2); digitalWrite(PIN_ADC_RESET, HIGH);
  } else {
    cmd(CMD_RESET);
  }
  delay(50);
  waitDrdyLow(1000000);   /* reset triggers self-cal; DRDY goes low when done (best effort) */
  cmd(CMD_SDATAC);
  delay(2);
  const uint8_t st = readReg(REG_STATUS);
  diag.id = st >> 4;
  if (diag.id != 0x3) { diag.cfgFail++; return false; }   /* ADS1256 ID = 3 */
  return true;
}

bool ADS1256::configure(uint8_t drate, uint8_t gainCode, bool buf) {
  if (!drateValid(drate) || gainCode > 6) return false;
  cmd(CMD_SDATAC);
  writeReg(REG_STATUS, 0x04 | (buf ? 0x02 : 0x00));   /* MSB first, auto-cal on */
  writeReg(REG_MUX, ADC_MUX_VALUE);
  writeReg(REG_ADCON, gainCode & 0x07);               /* CLKOUT off, sensor-detect off */
  writeReg(REG_DRATE, drate);
  if (readReg(REG_DRATE) != drate || (readReg(REG_ADCON) & 0x07) != gainCode) { diag.cfgFail++; return false; }
  cmd(CMD_SELFCAL);
  delay(2);
  waitDrdyHigh(20000);
  return waitDrdyLow(2000000);   /* self-cal can take up to ~1 s at low rates */
}
