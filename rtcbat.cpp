#include "rtcbat.h"
#include "pin_config.h"  // define XPOWERS_CHIP_AXP2101
#include <Wire.h>
#include <time.h>
#include <sys/time.h>
#include <SensorPCF85063.hpp>
#include <XPowersLib.h>

static SensorPCF85063 rtc;
static XPowersPMU pmu;
static bool rtcOk = false;
static bool pmuOk = false;

// La 1.75C no lleva PCF85063: sin el chip, la hora la lleva el reloj del sistema
// del ESP32 (sobrevive a los reinicios por software, no a un corte de energia).
// setup() ya siembra la hora con la ultima guardada en NVS cuando llega a 0, asi
// que el tiempo nunca va hacia atras; lo que se pierde es el tiempo apagado.
static bool sysClock = false;

bool rtcBegin() {
  rtcOk = rtc.begin(Wire, IIC_SDA, IIC_SCL);
  if (!rtcOk) {
    sysClock = true;
    Serial.println("PCF85063 no detectado: hora por reloj del sistema");
  }
  return rtcOk;
}

uint32_t rtcEpoch() {
  if (sysClock) {
    time_t now = time(nullptr);
    return now >= 1735689600 ? (uint32_t)now : 0;  // antes de 2025 = sin hora valida
  }
  if (!rtcOk) return 0;
  RTC_DateTime t = rtc.getDateTime();
  if (t.getYear() < 2025 || t.getYear() > 2120) return 0;  // sin hora valida
  struct tm tmv = {};
  tmv.tm_year = t.getYear() - 1900;
  tmv.tm_mon = t.getMonth() - 1;
  tmv.tm_mday = t.getDay();
  tmv.tm_hour = t.getHour();
  tmv.tm_min = t.getMinute();
  tmv.tm_sec = t.getSecond();
  time_t e = mktime(&tmv);  // TZ por defecto = UTC, consistente con gmtime_r
  return e > 0 ? (uint32_t)e : 0;
}

void rtcSetEpoch(uint32_t e) {
  if (sysClock) {
    struct timeval tv = {(time_t)e, 0};
    settimeofday(&tv, nullptr);
    return;
  }
  if (!rtcOk) return;
  time_t tt = e;
  struct tm tmv;
  gmtime_r(&tt, &tmv);
  rtc.setDateTime(RTC_DateTime(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                               tmv.tm_hour, tmv.tm_min, tmv.tm_sec));
}

bool batBegin() {
  pmuOk = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!pmuOk) Serial.println("AXP2101 no detectado");
  return pmuOk;
}

// Enciende la alimentacion de la AMOLED. En la Waveshare 1.75 el panel (OLED VDD)
// cuelga del rail BLDO1 a 3.3V del AXP2101. El firmware daba por hecho que estaba
// encendido; si el PMU se resetea (drenaje total), BLDO1 queda OFF y la pantalla
// se ve negra aunque el resto funcione. Hay que llamarla ANTES de gfx->begin().
void pmuEnablePanel() {
  if (!pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("AXP2101 no detectado (pmuEnablePanel)");
    return;
  }
  pmu.setBLDO1Voltage(3300);   // OLED VDD
  pmu.enableBLDO1();
}

// el estado de energia (I2C) se cachea ~2 s: leerlo en cada frame del loop
// metia trafico I2C inutil y podia oscilar (parpadeo de brillo)
static uint32_t powerCacheT = 0;
static int cachedPct = -1;
static bool cachedCharging = false, cachedUsb = true;

static void refreshPower() {
  uint32_t now = millis();
  if (powerCacheT && now - powerCacheT < 2000) return;
  powerCacheT = now ? now : 1;
  if (!pmuOk) { cachedPct = -1; cachedCharging = false; cachedUsb = true; return; }
  cachedPct = pmu.isBatteryConnect() ? pmu.getBatteryPercent() : -1;
  cachedCharging = pmu.isCharging();
  cachedUsb = pmu.isVbusIn();
}

int batPercent() { refreshPower(); return cachedPct; }
bool batCharging() { refreshPower(); return cachedCharging; }
bool usbPresent() { refreshPower(); return cachedUsb; }

void batPowerOff() {
  if (pmuOk) pmu.shutdown();
}

void pwrSetup() {
  if (!pmuOk) return;
  // 2,5 s = aviso de pulsacion larga (el firmware suspende); el corte fisico se
  // aleja a 10 s y queda como apagado de emergencia si el juego se cuelga
  pmu.setIrqLevelTime(XPOWERS_AXP2101_PRESSOFF_2S5);
  pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_10S);
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ);
  pmu.clearIrqStatus();  // descarta la pulsacion que desperto/encendio la placa
}

uint8_t pwrPoll() {
  if (!pmuOk) return PWR_NONE;
  pmu.getIrqStatus();
  uint8_t ev = pmu.isPekeyLongPressIrq() ? PWR_LONG
             : pmu.isPekeyShortPressIrq() ? PWR_SHORT : PWR_NONE;
  if (ev) pmu.clearIrqStatus();
  return ev;
}
