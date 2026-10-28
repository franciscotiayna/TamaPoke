#pragma once
#include <Arduino.h>

// RTC PCF85063: hora persistente mientras la placa tenga alimentacion
bool rtcBegin();
uint32_t rtcEpoch();             // segundos unix; 0 si el RTC no es valido
void rtcSetEpoch(uint32_t e);

// PMU AXP2101: estado de la bateria
bool batBegin();
void pmuEnablePanel();           // enciende BLDO1 (OLED VDD 3.3V); llamar antes de gfx->begin()
int batPercent();                // 0-100, -1 si no hay bateria conectada
bool batCharging();
bool usbPresent();
void batPowerOff();              // apagado fisico del PMU

// boton PWR del AXP2101: corta = pantalla on/off, larga (2,5 s) = suspender
// (lo hace el firmware), 10 s = apagado fisico del PMU (emergencia)
enum : uint8_t { PWR_NONE = 0, PWR_SHORT, PWR_LONG };
void pwrSetup();
uint8_t pwrPoll();  // sondear en el loop
