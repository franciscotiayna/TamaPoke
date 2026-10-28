# TamaPoke — port a la Waveshare ESP32-S3-Touch-AMOLED-1.75C

Fork de socquique/TamaPoke (remoto `upstream`) adaptado a la **1.75C** (la versión
con carcasa CNC de aluminio). La rama de trabajo es `board-1.75c`. El original
está hecho para la 1.75 "a secas", que es otra placa.

## Diferencias de hardware con la 1.75 (verificadas en placa)
- **Sin microSD**: los sprites viven en la partición FAT `ffat` de la flash
  interna (32 MB). `sdmon.cpp` usa `FFat` vía la macro `SPRITE_FS`; los nombres
  `sdReady`/`sdBegin` se conservan para no tocar el resto.
- **Sin RTC PCF85063**: `rtcbat.cpp` cae al reloj del sistema del ESP32
  (`time()`/`settimeofday`). Sobrevive a reinicios por software y al deep sleep,
  no a un corte de energía.
- **Pines** (`pin_config.h`, del demo oficial + esquemático): LCD_RESET=1,
  TP_RESET=2, I2S MCLK=16. GPIO3 = tecla PWR (SYS_OUT vía transistor T1).
- **R9 no viene montada**: GPIO3 está a 0 V en reposo y queda flotando al pulsar.
  Hay que usar pull-up interno (también el RTC durante el deep sleep):
  pulsada = HIGH. El aviso de botón del AXP2101 (IRQ) no llega al ESP32.

## Botón PWR
- Corta: pantalla on/off.
- Larga 2,5 s (IRQ del AXP2101): `suspend()` en `TamaPoke.ino` guarda, duerme
  periféricos y entra en deep sleep; despierta por ext0 en GPIO3. Al arrancar,
  `pet.syncClock()` aplica el tiempo transcurrido.
- 10 s: corte físico del PMU (emergencia).
- Batería < 3,30 V: apagado físico, como en el original.

## Compilar y grabar
Core `esp32:esp32@3.3.10` (fijado por el CI del upstream) y librerías:
GFX Library for Arduino, SensorLib, XPowersLib, U8g2. La carpeta del sketch debe
llamarse `TamaPoke` (igual que el `.ino`).

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=32M,PSRAM=opi,PartitionScheme=custom" --output-dir build .
```

`partitions.csv` del sketch se usa solo (`PartitionScheme=custom`): app 4 MB en
0x10000, `ffat` 27,9 MB en 0x410000.

- Actualizar solo el juego (conserva los sprites):
  `esptool --port COMx --baud 921600 write-flash 0x10000 build/TamaPoke.ino.bin`
- Instalación desde cero: `erase-flash` + `write-flash 0x0 build/TamaPoke.ino.merged.bin`.
  El primer arranque formatea la FAT; después hay que subir los sprites.

## Sprites
`python tools/send_sd.py --port COMx --no-shiny` sube `tools/sdcard/mons/`
(151 normales + `thumbs.bin`, ~21 MB, ~4 min) por el protocolo PUT. Los shinies
(otros ~21 MB) no caben; el firmware usa el sprite normal en su lugar. Mientras
dura un PUT el bucle del juego está bloqueado (el táctil no responde).

## Consola serie (115200)
Abrir el puerto con DTR/RTS a False o la placa se reinicia. Comandos útiles,
además de los del README: `SDINFO` (uso de la FAT), `KEYPIN` (diagnóstico de
GPIO3/tecla PWR), `TIME`, `STATS`, `HEALTH`.

## Pendiente
- Medir el consumo en suspensión con batería (sin USB).

## Flujo de trabajo
Sesiones locales con la placa por USB: Claude compila y graba directamente con
`esptool` (no se usa el instalador web de `web/`, que sigue apuntando a la 1.75
original). Detectar el puerto por VID 0x303A antes de grabar.
