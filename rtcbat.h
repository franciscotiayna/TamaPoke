#pragma once
#include <Arduino.h>

// RTC PCF85063: hora persistente mientras la placa tenga alimentacion
bool rtcBegin();
uint32_t rtcEpoch();             // segundos unix; 0 si el RTC no es valido
void rtcSetEpoch(uint32_t e);

// PMU AXP2101: estado de la bateria
bool batBegin();
void pmuEnablePanel();           // enciende BLDO1 (OLED VDD 3.3V); llamar antes de gfx->begin()
void batSetPollMs(uint32_t ms);   // cada cuanto se lee el PMU por I2C (se alarga con la pantalla apagada)
int batPercent();                // 0-100, -1 si no hay bateria conectada
bool batCharging();
int batMillivolts();               // tension de la bateria en mV, 0 si no hay lectura
bool batShouldShutdown();          // descargada hasta el limite seguro (batcurve.h): guardar y apagar
void batPowerOff();                // apagado fisico del PMU
bool usbPresent();
bool usbPresentNow();             // sin cache (una lectura I2C): para decidir si se puede dormir

// boton PWR del AXP2101: corta = pantalla on/off, larga (2,5 s) = suspender
// (lo hace el firmware), 10 s = apagado fisico del PMU (emergencia)
enum : uint8_t { PWR_NONE = 0, PWR_SHORT, PWR_LONG };
void pwrSetup();
uint8_t pwrPoll();  // sondear en el loop
