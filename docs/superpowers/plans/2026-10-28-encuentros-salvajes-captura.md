# Encuentros salvajes y captura — plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Añadir encuentros salvajes (a demanda y espontáneos), captura con Pokéball y anillo de timing, monedas y tienda, y criar a demanda (intercambio mascota ↔ equipo, con huevo guardado) a la rama `dylan-1.75c`.

**Architecture:** La lógica pura (sorteo, fórmula de captura, economía, persistencia de monedas) va en un módulo nuevo `wild.cpp/.h` sin pantalla, testeable en el emulador de Dylan como `battle.cpp`. Los datos de vida que `PartyMon` no guarda viajan en una tabla paralela `PartyLife` dentro de `Party` (claves NVS nuevas `plife`/`blife`), y `Pet` gana `bankFull`/`raiseFrom`/`stashEgg`/`restoreEgg`. Las pantallas y el disparo de encuentros se montan en `TamaPoke.ino` sobre el combate existente (`startBattle`).

**Tech Stack:** Arduino C++ (core `esp32:esp32@3.3.10`), Preferences/NVS, emulador SDL2 de Dylan (`tools/emu`), `arduino-cli`, `esptool`.

**Spec:** `docs/superpowers/specs/2026-10-28-encuentros-salvajes-captura-design.md`

---

## Antes de empezar

- Trabaja en el worktree `C:\Users\Tarrito\Documents\TamaPoke-dylan\TamaPoke`, rama `dylan-1.75c`. La carpeta del sketch debe llamarse `TamaPoke`.
- Los ficheros usan CRLF. El tool Edit respeta lo que hay; si escribes con scripts, conserva CRLF.
- Compilar firmware (comando de referencia en todo el plan):

```bash
"/c/Program Files/Arduino CLI/arduino-cli.exe" compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=32M,PSRAM=opi,PartitionScheme=custom" --output-dir build .
```

  Expected: termina con `El Sketch usa ... bytes` y sin `error:`.
- Tests del emulador: `bash tools/emu/tests/run.sh <filtro>`. Necesitan g++ y SDL2 (Task 1). Imprimen `PASS`/`FAIL` por comprobación y `suites passed: N, failed: 0` al final.

## Mapa de ficheros

| Fichero | Qué cambia |
|---|---|
| `wild.h`, `wild.cpp` (nuevos) | Estado de monedas/bolas/contadores, fórmula de captura, tambaleos, huida, sorteo del salvaje, conversión a `PartyMon`, carga/guardado NVS |
| `party.h`, `party.cpp` | `PartyLife` y tablas `life[]`/`boxLife[]` que acompañan a cada hueco; claves `plife`/`blife` |
| `pet.h`, `pet.cpp` | `StoredEgg`, `endedLife`, `bankFull`, `raiseFrom`, `registerCaught`, `stashEgg`, `restoreEgg`; el huevo guardado sustituye al huevo nuevo al terminar una mascota |
| `save.cpp` | Claves nuevas en `SAVE_FIELDS` |
| `i18n.h`, `i18n.cpp` | 20 textos nuevos × 6 idiomas |
| `TamaPoke.ino` | Estado global de wild, ganancias de monedas, comandos serie, combate salvaje con BALL y anillo, diálogo «¿Criarlo ahora?», fila EXPLORAR y su pantalla, burbuja «¡!», CRIAR en la ficha del equipo, píldora HUEVO |
| `tools/emu/*` | Stubs de la 1.75C y `wild.cpp` en la compilación |
| `tools/emu/tests/wild_test.cpp`, `raise_test.cpp` (nuevos) | Tests de la lógica |

---

### Task 1: Toolchain de tests y stubs del emulador para la 1.75C

El emulador compila el `.ino` real. El port de la 1.75C añadió suspensión (cabeceras ESP-IDF, `digitalRead`, `touch.sleep()`...) que el emulador no conoce, así que hoy no compila.

**Files:**
- Create: `tools/emu/esp_sleep.h`, `tools/emu/driver/gpio.h`, `tools/emu/driver/rtc_io.h`
- Modify: `tools/emu/Arduino.h`, `tools/emu/Arduino_GFX_Library.h`, `tools/emu/TouchDrvCSTXXX.hpp`, `tools/emu/host_impl.cpp`

- [ ] **Step 1: Instalar g++ y SDL2 (Windows)**

```bash
winget install --id MSYS2.MSYS2 -e --accept-source-agreements --accept-package-agreements --silent
```

Después, en una consola **MSYS2 UCRT64** (`C:\msys64\ucrt64.exe`):

```bash
pacman -S --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 make
```

Los comandos `bash tools/emu/tests/run.sh` de este plan se lanzan desde esa consola, en el worktree (`cd /c/Users/Tarrito/Documents/TamaPoke-dylan/TamaPoke`). Si no se puede instalar, sigue el plan compilando solo el firmware y deja anotado en cada task que los tests quedan sin ejecutar.

- [ ] **Step 2: Comprobar que hoy falla**

Run: `bash tools/emu/build.sh`
Expected: error de compilación por `esp_sleep.h: No such file or directory`.

- [ ] **Step 3: Crear `tools/emu/esp_sleep.h`**

```cpp
// Stub del emulador: la suspension en deep sleep del port 1.75C no existe en el PC.
#pragma once
typedef enum { ESP_SLEEP_WAKEUP_UNDEFINED = 0, ESP_SLEEP_WAKEUP_EXT0 = 2 } esp_sleep_wakeup_cause_t;
inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return ESP_SLEEP_WAKEUP_UNDEFINED; }
inline int esp_sleep_enable_ext0_wakeup(int, int) { return 0; }
inline void esp_deep_sleep_start() {}
```

- [ ] **Step 4: Crear `tools/emu/driver/gpio.h`**

```cpp
// Stub del emulador para los pines retenidos durante la suspension (port 1.75C).
#pragma once
typedef int gpio_num_t;
inline int gpio_hold_en(gpio_num_t) { return 0; }
inline int gpio_hold_dis(gpio_num_t) { return 0; }
inline void gpio_deep_sleep_hold_en() {}
inline void gpio_deep_sleep_hold_dis() {}
```

- [ ] **Step 5: Crear `tools/emu/driver/rtc_io.h`**

```cpp
// Stub del emulador: pull-up RTC de la tecla PWR durante la suspension (port 1.75C).
#pragma once
#include "driver/gpio.h"
inline int rtc_gpio_pullup_en(gpio_num_t) { return 0; }
inline int rtc_gpio_pulldown_dis(gpio_num_t) { return 0; }
inline int rtc_gpio_deinit(gpio_num_t) { return 0; }
```

- [ ] **Step 6: Añadir a `tools/emu/Arduino.h`** justo debajo de `#define FALLING 2`:

```cpp
#define HIGH 1
#define LOW 0
#define INPUT_PULLDOWN 3
```

y justo debajo de `inline void pinMode(int, int) {}`:

```cpp
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return LOW; }   // la tecla PWR nunca esta pulsada en el PC
inline void delayMicroseconds(uint32_t) {}
inline int analogReadMilliVolts(int) { return 0; }
```

y dentro de `struct FakeSerial`, junto a `println`:

```cpp
  void flush() { fflush(stdout); }
```

- [ ] **Step 7: Stubs de panel y táctil**

En `tools/emu/Arduino_GFX_Library.h`, dentro de `class Arduino_CO5300`, debajo de `setBrightness`:

```cpp
  void displayOff() {}
```

En `tools/emu/TouchDrvCSTXXX.hpp`, dentro de `class TouchDrvCST92xx`, debajo de `void reset() {}`:

```cpp
  void sleep() {}
```

- [ ] **Step 8: API de la tecla PWR en `tools/emu/host_impl.cpp`**

Sustituir:

```cpp
bool pwrShortPressed() { return false; }
```

por:

```cpp
uint8_t pwrPoll() { return 0; }   // PWR_NONE: en el PC no hay tecla
void batPowerOff() {}
```

- [ ] **Step 9: Verificar**

Run: `bash tools/emu/build.sh`
Expected: `built: .../tools/emu/tamapoke-emu`. Si aparece otro símbolo del port sin stub, añádelo en el mismo estilo (función inline vacía en el stub correspondiente) y repite.

Run: `bash tools/emu/tests/run.sh`
Expected: `suites passed: N, failed: 0` (las suites de Dylan siguen pasando).

- [ ] **Step 10: Commit**

```bash
git add tools/emu
git commit -m "emu: stubs de la suspension del port 1.75C"
```

---

### Task 2: `wild` — economía de monedas, Pokéballs y contadores diarios

**Files:**
- Create: `wild.h`, `wild.cpp`
- Modify: `tools/emu/build.sh`, `tools/emu/tests/run.sh`
- Test: `tools/emu/tests/wild_test.cpp`

- [ ] **Step 1: Añadir `wild.cpp` a la compilación del emulador**

En `tools/emu/build.sh`, en la línea de fuentes, después de `"$ROOT/save.cpp"` añade ` "$ROOT/wild.cpp"`.

En `tools/emu/tests/run.sh`, en `CORE=(...)`, después de `"$ROOT/save.cpp"` añade ` "$ROOT/wild.cpp"`.

- [ ] **Step 2: Escribir el test que falla** — `tools/emu/tests/wild_test.cpp`

```cpp
// Encuentros salvajes y captura: economia, formula de captura, sorteo del
// salvaje y persistencia. Contra wild.cpp real, sin pantalla.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "wild.h"
#include "dex.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 4242;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
bool wasPressed = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

static void testEconomy() {
  WildState w;
  ck(w.coins == 50 && w.balls == 5, "arranca con 50 monedas y 5 bolas");
  ck(wildBuy(w, 1) && w.coins == 40 && w.balls == 6, "comprar 1 bola cuesta 10");
  ck(!wildBuy(w, 10), "no compra 10 con 40 monedas");
  ck(w.coins == 40 && w.balls == 6, "una compra rechazada no cambia nada");
  wildEarn(w, 60000);
  ck(w.coins == WILD_COINS_MAX, "monedas topadas a 9999");
  w.balls = 45;
  ck(!wildCanBuy(w, 10) && !wildBuy(w, 10), "no pasa de 50 bolas");
  ck(wildBuy(w, 5) && w.balls == 50, "llega justo a 50");
  ck(wildCareCoins(0) == 10 && wildCareCoins(3) == 13 && wildCareCoins(40) == 20,
     "cuidado diario: 10 + racha (max +10)");

  WildState d;
  d.explores = 5; d.ambient = 3; d.day = 100;
  wildNewDay(d, 100);
  ck(d.explores == 5 && d.ambient == 3, "el mismo dia no repone");
  wildNewDay(d, 101);
  ck(d.explores == 0 && d.ambient == 0 && d.day == 101, "dia nuevo repone busquedas y avisos");
  for (int i = 0; i < WILD_EXPLORES_PER_DAY; i++) wildUseExplore(d);
  ck(!wildUseExplore(d) && d.explores == WILD_EXPLORES_PER_DAY, "5 busquedas por dia");
}

int main() {
  testEconomy();
  printf(bad ? "%d FAILED\n" : "all passed\n", bad);
  return bad ? 1 : 0;
}
```

- [ ] **Step 3: Ejecutar y ver que falla**

Run: `bash tools/emu/tests/run.sh wild`
Expected: `=== wild_test: DID NOT COMPILE` (no existe `wild.h`).

- [ ] **Step 4: Crear `wild.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "pet.h"
#include "party.h"

// Encuentros salvajes y captura. Solo logica, sin pantalla ni hardware, igual
// que battle.cpp, para poder probarla entera en tools/emu. El azar sale de
// random(), que el emulador siembra con g_seed.

#define WILD_EXPLORES_PER_DAY 5    // busquedas a demanda (fila EXPLORAR)
#define WILD_AMBIENT_PER_DAY 3     // avisos espontaneos "!"
#define WILD_BALL_PRICE 10         // monedas por Pokeball
#define WILD_BALLS_MAX 50
#define WILD_COINS_MAX 9999
#define WILD_START_BALLS 5
#define WILD_START_COINS 50
#define WILD_EVOLVED_PCT 30        // un salvaje con nivel de sobra sale evolucionado
#define WILD_LEGEND_PCT 1
#define WILD_LEGEND_MIN_REG 25     // igual que los huevos: legendarios desde 25 registrados

// punteria del anillo al lanzar la bola
enum : uint8_t { AIM_MISS = 0, AIM_OK, AIM_GOOD, AIM_PERFECT };

struct WildMon {
  int16_t dex = 0;
  uint8_t level = 1;
  uint8_t tier = R_COMUN;     // R_COMUN / R_RARO / R_LEGENDARIO: fija la tasa de captura
  bool shiny = false;
  uint8_t ivAtk = 16, ivDef = 16, ivSpe = 16, ivHp = 16;
};

// Monedas, bolas y contadores del dia. Del jugador, como la racha: sobrevive a
// todas las crianzas.
struct WildState {
  uint16_t coins = WILD_START_COINS;
  uint8_t balls = WILD_START_BALLS;
  uint8_t explores = 0;       // busquedas gastadas hoy
  uint8_t ambient = 0;        // avisos espontaneos de hoy
  uint32_t day = 0;           // dia (epoch / 86400) de esos dos contadores
  uint32_t paidCareDay = 0;   // ultimo "primer cuidado del dia" ya pagado
  uint16_t paidMedals = 0;    // pet.totalMedals ya pagadas
};

void wildNewDay(WildState &w, uint32_t day);   // repone contadores si cambio el dia
void wildEarn(WildState &w, uint16_t coins);   // suma con tope
bool wildCanBuy(const WildState &w, uint8_t n);
bool wildBuy(WildState &w, uint8_t n);         // false = no alcanza o pasaria del tope
uint16_t wildCareCoins(uint16_t streak);       // premio del primer cuidado del dia
bool wildUseExplore(WildState &w);             // gasta una busqueda; false si no quedan
```

- [ ] **Step 5: Crear `wild.cpp`** (solo economía por ahora)

```cpp
#include "wild.h"

void wildNewDay(WildState &w, uint32_t day) {
  if (day == w.day) return;
  w.day = day;
  w.explores = 0;
  w.ambient = 0;
}

void wildEarn(WildState &w, uint16_t coins) {
  uint32_t c = (uint32_t)w.coins + coins;
  w.coins = c > WILD_COINS_MAX ? WILD_COINS_MAX : (uint16_t)c;
}

bool wildCanBuy(const WildState &w, uint8_t n) {
  return n && (uint32_t)n * WILD_BALL_PRICE <= w.coins &&
         (uint16_t)w.balls + n <= WILD_BALLS_MAX;
}

bool wildBuy(WildState &w, uint8_t n) {
  if (!wildCanBuy(w, n)) return false;
  w.coins -= (uint16_t)n * WILD_BALL_PRICE;
  w.balls += n;
  return true;
}

uint16_t wildCareCoins(uint16_t streak) {
  return 10 + (streak > 10 ? 10 : streak);
}

bool wildUseExplore(WildState &w) {
  if (w.explores >= WILD_EXPLORES_PER_DAY) return false;
  w.explores++;
  return true;
}
```

- [ ] **Step 6: Ejecutar y ver que pasa**

Run: `bash tools/emu/tests/run.sh wild`
Expected: todas las líneas `PASS` y `all passed`.

- [ ] **Step 7: Compilar el firmware**

Run: el comando de compilación de referencia.
Expected: compila (arduino-cli compila todos los `.cpp` del sketch, incluido `wild.cpp`).

- [ ] **Step 8: Commit**

```bash
git add wild.h wild.cpp tools/emu/build.sh tools/emu/tests/run.sh tools/emu/tests/wild_test.cpp
git commit -m "wild: monedas, Pokeballs y contadores diarios"
```

---

### Task 3: `wild` — fórmula de captura, tambaleos y huida

**Files:**
- Modify: `wild.h`, `wild.cpp`
- Test: `tools/emu/tests/wild_test.cpp`

- [ ] **Step 1: Añadir el test que falla** — en `wild_test.cpp`, antes de `int main()`:

```cpp
static void testCatch() {
  ck(wildCatchValue(R_COMUN, 100, 100, AIL_NONE, AIM_OK) == 15, "comun a vida llena: 15");
  ck(wildCatchValue(R_COMUN, 1, 100, AIL_NONE, AIM_OK) == 44, "comun a 1 PS: 44");
  ck(wildCatchValue(R_COMUN, 1, 100, AIL_SLEEP, AIM_PERFECT) == 176, "dormido y perfecto: x4");
  ck(wildCatchValue(R_RARO, 100, 100, AIL_PARA, AIM_GOOD) == 18, "raro, paralizado y bien: 8 x3/2 x3/2");
  ck(wildCatchValue(R_LEGENDARIO, 100, 100, AIL_NONE, AIM_MISS) == 1, "nunca baja de 1");
  ck(wildCatchValue(R_COMUN, 0, 0, AIL_SLEEP, AIM_PERFECT) == 180, "vida 0/0 no divide por cero");
  ck(wildShakes(255) == 3, "a=255 siempre captura");
  int caught = 0;
  for (int i = 0; i < 2000; i++) if (wildShakes(1) == 3) caught++;
  ck(caught == 0, "a=1 practicamente nunca captura");
  ck(!wildFlees(1) && !wildFlees(2) && !wildFlees(3), "no huye en los 3 primeros fallos");
  int fled = 0;
  for (int i = 0; i < 4000; i++) if (wildFlees(4)) fled++;
  ck(fled > 800 && fled < 1200, "desde el cuarto fallo huye ~25%");
}
```

y en `main()`, debajo de `testEconomy();`:

```cpp
  testCatch();
```

- [ ] **Step 2: Ejecutar y ver que falla**

Run: `bash tools/emu/tests/run.sh wild`
Expected: `DID NOT COMPILE` (`wildCatchValue` no declarado).

- [ ] **Step 3: Declarar en `wild.h`** (al final):

```cpp
// Valor de captura 1..255, al estilo de los juegos:
//   tasa x (3*hpMax - 2*hp) / (3*hpMax) x estado x punteria
// tasa: comun 45, raro 25, legendario 3. Dormido/congelado x2, paralizado,
// quemado o envenenado x3/2. Punteria: perfecto x2, bien x3/2, fallo x1/2.
uint8_t wildCatchValue(uint8_t tier, uint16_t hp, uint16_t maxHp, uint8_t ailment, uint8_t aim);
// Tambaleos que aguanta la bola (0..3). 3 = capturado.
uint8_t wildShakes(uint8_t a);
// Tras un lanzamiento fallido: desde el cuarto fallo, 25% de que huya.
bool wildFlees(uint8_t failedThrows);
```

- [ ] **Step 4: Implementar en `wild.cpp`** (al final):

```cpp
uint8_t wildCatchValue(uint8_t tier, uint16_t hp, uint16_t maxHp, uint8_t ailment, uint8_t aim) {
  uint32_t rate = tier == R_LEGENDARIO ? 3 : tier == R_RARO ? 25 : 45;
  if (maxHp == 0) maxHp = 1;
  if (hp > maxHp) hp = maxHp;
  uint32_t a = rate * (3u * maxHp - 2u * hp) / (3u * maxHp);
  if (ailment == AIL_SLEEP || ailment == AIL_FREEZE) a *= 2;
  else if (ailment == AIL_PARA || ailment == AIL_BURN || ailment == AIL_POISON) a = a * 3 / 2;
  if (aim == AIM_PERFECT) a *= 2;
  else if (aim == AIM_GOOD) a = a * 3 / 2;
  else if (aim == AIM_MISS) a /= 2;
  if (a < 1) a = 1;
  if (a > 255) a = 255;
  return (uint8_t)a;
}

uint8_t wildShakes(uint8_t a) {
  // random(255) da 0..254: con a=255 los tres tambaleos pasan siempre
  uint8_t n = 0;
  while (n < 3 && random(255) < a) n++;
  return n;
}

bool wildFlees(uint8_t failedThrows) {
  return failedThrows >= 4 && random(4) == 0;
}
```

Si el compilador no encuentra `AIL_*`, añade `#include "moves.h"` en `wild.cpp`.

- [ ] **Step 5: Ejecutar y ver que pasa**

Run: `bash tools/emu/tests/run.sh wild`
Expected: todo `PASS`.

- [ ] **Step 6: Commit**

```bash
git add wild.h wild.cpp tools/emu/tests/wild_test.cpp
git commit -m "wild: formula de captura, tambaleos y huida"
```

---

### Task 4: `Pet::registerCaught` y sorteo del salvaje

**Files:**
- Modify: `pet.h`, `pet.cpp`, `wild.h`, `wild.cpp`
- Test: `tools/emu/tests/wild_test.cpp`

- [ ] **Step 1: Añadir el test que falla** — en `wild_test.cpp`, antes de `int main()`:

```cpp
static void testRoll() {
  Pet p;
  p.begin();
  p.dbgHatchAs(4, false);                        // Charmander
  p.ageMinutes = 19UL * MINUTES_PER_LEVEL;       // nivel 20
  p.region = 0;                                  // Kanto
  const RegionInfo &rg = REGIONS[0];
  bool rolled = true, lvOk = true, regOk = true, artOk = true, noLegend = true, evolved = false;
  for (int i = 0; i < 500; i++) {
    WildMon m;
    if (!wildRoll(p, m)) { rolled = false; break; }
    if (m.level < 17 || m.level > 23) lvOk = false;
    if (m.dex < rg.lo || m.dex > rg.hi) regOk = false;
    if (!speciesHasArt(m.dex)) artOk = false;
    if (m.tier == R_LEGENDARIO) noLegend = false;
    if (DEX_TBL[m.dex].rarity == R_EVO) evolved = true;
  }
  ck(rolled, "siempre sale un salvaje");
  ck(lvOk, "nivel = el de la mascota +-3");
  ck(regOk, "solo de la region elegida");
  ck(artOk, "nunca una especie sin arte");
  ck(noLegend, "sin legendarios con menos de 25 registrados");
  ck(evolved, "a nivel 20 a veces sale ya evolucionado");

  for (int16_t d = 1; d <= 30; d++) p.registerCaught(d, false);
  ck(p.isRegistered(30) && !p.isShinyRegistered(30), "registerCaught apunta en la Pokedex");
  p.registerCaught(31, true);
  ck(p.isShinyRegistered(31), "registerCaught apunta el shiny aparte");
  int legends = 0;
  bool legLv = true;
  for (int i = 0; i < 4000; i++) {
    WildMon m;
    wildRoll(p, m);
    if (m.tier != R_LEGENDARIO) continue;
    legends++;
    if (m.level < 40 || DEX_TBL[m.dex].rarity != R_LEGENDARIO) legLv = false;
  }
  ck(legends > 10 && legends < 100, "con 25+ registrados, ~1% legendarios");
  ck(legLv, "el legendario sale a nivel 40 o mas");
}
```

y en `main()`, debajo de `testCatch();`:

```cpp
  testRoll();
```

(`wildToParty`, que convierte el capturado en miembro del equipo, necesita `PartyLife` y llega en la Task 6.)

- [ ] **Step 2: Ejecutar y ver que falla**

Run: `bash tools/emu/tests/run.sh wild`
Expected: `DID NOT COMPILE` (`wildRoll` y `registerCaught` no existen).

- [ ] **Step 3: `Pet::registerCaught`**

En `pet.h`, en la parte pública, justo debajo de `void reviveFrom(const PartyMon &m);`:

```cpp
  // Lo capturado cuenta en la Pokedex como lo criado. registerSpecies() mira
  // el shiny de la mascota activa, que aqui no es el capturado.
  void registerCaught(int16_t dex, bool shinyMon);
```

En `pet.cpp`, justo debajo de la función `void Pet::registerSpecies(int16_t dex) { ... }`:

```cpp
void Pet::registerCaught(int16_t dex, bool shinyMon) {
  if (dex < 1 || dex > DEX_COUNT) return;
  dexReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
  if (shinyMon) dexShinyReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
  save();
}
```

- [ ] **Step 4: Declarar el sorteo en `wild.h`** (al final):

```cpp
// Sortea el salvaje: legendario 1% con 25+ registrados (nivel +10, minimo 40);
// si no, rareza como los huevos (comun / raro con el bonus de cuidado) y
// prefiriendo lineas incompletas. Region de los huevos, nivel de la mascota
// +-3, a veces ya evolucionado, shiny 1/48 mejorado por el cuidado.
// false si la mascota es un huevo o no hay nada que sacar.
bool wildRoll(Pet &p, WildMon &out);
```

- [ ] **Step 5: Implementar `wildRoll` en `wild.cpp`** (al final):

```cpp
#define WILD_CAND_MAX 260   // como CAND_MAX en pet.cpp: cabe la region mas grande

static uint8_t wildRegion(uint8_t want) {
  return regionAvailable(want) ? want : nextAvailableRegion(want);
}

bool wildRoll(Pet &p, WildMon &m) {
  if (p.isEgg()) return false;
  const RegionInfo &rg = REGIONS[wildRegion(p.region % REGION_COUNT)];

  uint8_t tier = R_COMUN;
  int leg = p.registeredCount() >= WILD_LEGEND_MIN_REG ? WILD_LEGEND_PCT : 0;
  int rare = 27 + p.careBonus();
  int r = random(100);
  if (r < leg) tier = R_LEGENDARIO;
  else if (r < leg + rare) tier = R_RARO;

  // como pickEggSpecies: primero lineas incompletas, bajando de rareza si no hay
  int16_t dex = 0;
  for (int pass = 0; pass < 2 && !dex; pass++) {
    for (int t = tier; t >= R_COMUN && !dex; t--) {
      int16_t cand[WILD_CAND_MAX];
      int n = 0;
      for (int16_t d = rg.lo; d <= rg.hi && n < WILD_CAND_MAX; d++) {
        if (DEX_TBL[d].rarity != t) continue;
        if (pass == 0 && !p.lineHasUnregistered(d)) continue;
        if (!regionAvailable(regionOfDex(d)) || !speciesHasArt(d)) continue;
        cand[n++] = d;
      }
      if (n) { dex = cand[random(n)]; tier = (uint8_t)t; }
    }
  }
  if (!dex) return false;

  int lvl = p.level();
  if (tier == R_LEGENDARIO) { lvl += 10; if (lvl < 40) lvl = 40; }
  else lvl += (int)random(7) - 3;
  if (lvl < 3) lvl = 3;
  if (lvl > MAX_LEVEL) lvl = MAX_LEVEL;

  // con nivel de sobra puede salir ya evolucionado, un paso cada vez
  while (DEX_TBL[dex].evolvesTo && DEX_TBL[dex].evolvesTo <= DEX_COUNT &&
         DEX_TBL[dex].evolveLevel && lvl >= DEX_TBL[dex].evolveLevel &&
         speciesHasArt(DEX_TBL[dex].evolvesTo) && random(100) < WILD_EVOLVED_PCT)
    dex = DEX_TBL[dex].evolvesTo;

  int shinyBase = 48 - p.careBonus();
  if (shinyBase < 8) shinyBase = 8;
  m = WildMon();
  m.dex = dex;
  m.level = (uint8_t)lvl;
  m.tier = tier;
  m.shiny = random(shinyBase) == 0;
  m.ivAtk = 8 + random(24);
  m.ivDef = 8 + random(24);
  m.ivSpe = 8 + random(24);
  m.ivHp = 8 + random(24);
  return true;
}
```

- [ ] **Step 6: Ejecutar y ver que pasa**

Run: `bash tools/emu/tests/run.sh wild`
Expected: todo `PASS`. Si «a veces sale ya evolucionado» falla por mala suerte de la semilla, sube el bucle a 1000 iteraciones; no cambies `WILD_EVOLVED_PCT`.

- [ ] **Step 7: Compilar el firmware** con el comando de referencia. Expected: compila.

- [ ] **Step 8: Commit**

```bash
git add pet.h pet.cpp wild.h wild.cpp tools/emu/tests/wild_test.cpp
git commit -m "wild: sorteo del salvaje y Pokedex de lo capturado"
```

---

### Task 5: `wild` — guardar y cargar en NVS

**Files:**
- Modify: `wild.h`, `wild.cpp`, `save.cpp`
- Test: `tools/emu/tests/wild_test.cpp`

- [ ] **Step 1: Añadir el test que falla** — antes de `int main()`:

```cpp
static void testPersist() {
  Pet p;
  p.begin();
  WildState first;
  first.coins = 1;                      // basura: wildLoad debe pisarla
  wildLoad(first, p);
  ck(first.coins == WILD_START_COINS && first.balls == WILD_START_BALLS,
     "primer arranque: valores iniciales");
  ck(first.paidMedals == p.totalMedals && first.paidCareDay == p.lastCareDay,
     "primer arranque: no paga medallas ni cuidados anteriores");

  WildState s;
  s.coins = 123; s.balls = 7; s.explores = 2; s.ambient = 1;
  s.day = 999; s.paidCareDay = 998; s.paidMedals = 4;
  wildSave(s);
  WildState r;
  wildLoad(r, p);
  ck(r.coins == 123 && r.balls == 7 && r.explores == 2 && r.ambient == 1 &&
     r.day == 999 && r.paidCareDay == 998 && r.paidMedals == 4,
     "monedas y contadores sobreviven a un reinicio");
}
```

En `main()`, como **primera** llamada (antes de `testEconomy();`), para que la NVS esté vacía:

```cpp
  testPersist();
```

- [ ] **Step 2: Ejecutar y ver que falla**

Run: `bash tools/emu/tests/run.sh wild`
Expected: `DID NOT COMPILE` (`wildLoad` no existe).

- [ ] **Step 3: Declarar en `wild.h`** (al final):

```cpp
// NVS, en el espacio "tamapoke" de siempre (WIPE lo borra con todo lo demas).
// Sin guardado previo arranca con los valores iniciales y marca como ya
// pagados el cuidado de hoy y las medallas que ya tiene el jugador.
void wildLoad(WildState &w, const Pet &p);
void wildSave(const WildState &w);
```

- [ ] **Step 4: Implementar en `wild.cpp`** — añade `#include <Preferences.h>` arriba y al final:

```cpp
void wildLoad(WildState &w, const Pet &p) {
  Preferences pr;
  pr.begin("tamapoke", true);
  bool init = pr.getBool("wini", false);
  w = WildState();
  if (init) {
    w.coins = pr.getUShort("wcoin", WILD_START_COINS);
    w.balls = pr.getUChar("wball", WILD_START_BALLS);
    w.explores = pr.getUChar("wexp", 0);
    w.ambient = pr.getUChar("wamb", 0);
    w.day = pr.getUInt("wday", 0);
    w.paidCareDay = pr.getUInt("wcare", 0);
    w.paidMedals = pr.getUShort("wmed", 0);
  }
  pr.end();
  if (w.coins > WILD_COINS_MAX) w.coins = WILD_COINS_MAX;
  if (w.balls > WILD_BALLS_MAX) w.balls = WILD_BALLS_MAX;
  if (!init) {
    w.paidCareDay = p.lastCareDay;
    w.paidMedals = p.totalMedals;
    wildSave(w);
  }
}

void wildSave(const WildState &w) {
  Preferences pr;
  pr.begin("tamapoke", false);
  pr.putBool("wini", true);
  pr.putUShort("wcoin", w.coins);
  pr.putUChar("wball", w.balls);
  pr.putUChar("wexp", w.explores);
  pr.putUChar("wamb", w.ambient);
  pr.putUInt("wday", w.day);
  pr.putUInt("wcare", w.paidCareDay);
  pr.putUShort("wmed", w.paidMedals);
  pr.end();
}
```

Si el stub `tools/emu/Preferences.h` no tiene `getUChar`/`putUChar`/`getBool`/`putBool`, mira qué usa `pet.cpp` para claves `SK_U8`/`SK_BOOL` y usa esos mismos métodos.

- [ ] **Step 5: Registrar las claves en `save.cpp`** — en `SAVE_FIELDS`, justo antes de `// settings, so a restored device plays the way it did`:

```cpp
  // encuentros salvajes: monedas, bolas y contadores del dia (wild.cpp)
  { "wini", SK_BOOL },  { "wcoin", SK_U16 },  { "wball", SK_U8 },
  { "wexp", SK_U8 },    { "wamb", SK_U8 },    { "wday", SK_U32 },
  { "wcare", SK_U32 },  { "wmed", SK_U16 },
```

- [ ] **Step 6: Ejecutar y ver que pasa**

Run: `bash tools/emu/tests/run.sh wild`
Expected: todo `PASS`.

Run: `bash tools/emu/tests/run.sh save`
Expected: `save_test` pasa (las claves nuevas están en la lista).

- [ ] **Step 7: Commit**

```bash
git add wild.h wild.cpp save.cpp tools/emu/tests/wild_test.cpp
git commit -m "wild: monedas y contadores en NVS"
```

---

### Task 6: `PartyLife` — la vida de cada hueco del equipo y la caja

**Files:**
- Modify: `party.h`, `party.cpp`, `save.cpp`, `wild.h`, `wild.cpp`, `tools/emu/tests/wild_test.cpp`
- Test: `tools/emu/tests/raise_test.cpp`

- [ ] **Step 1: Escribir el test que falla** — `tools/emu/tests/raise_test.cpp`

```cpp
// Criar a demanda: la vida de un guardado viaja con su hueco, la mascota se
// guarda y vuelve sin perder nada, y el huevo puede esperar aparte.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "dex.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 777;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
bool wasPressed = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

static void testPartyLife() {
  party.begin();
  PartyMon a;
  a.dex = 7; a.level = 20;
  PartyLife la;
  la.valid = 1; la.bond = 77; la.ageMinutes = 12345;
  ck(party.add(a, la), "add con datos de vida");
  ck(party.life[0].bond == 77 && party.life[0].ageMinutes == 12345, "la vida va con su hueco");
  party.swapPartyBox(0, 3);
  ck(party.slots[0].empty() && !party.life[0].valid && party.boxLife[3].bond == 77,
     "la vida viaja al mover a la caja");
  party.swapPartyBox(0, 3);
  party.begin();   // recarga desde NVS
  ck(party.slots[0].dex == 7 && party.life[0].ageMinutes == 12345, "la vida sobrevive a recargar");
  party.releaseAt(0);
  ck(!party.life[0].valid, "soltar borra su vida");
  PartyMon b;
  b.dex = 1; b.level = 5;
  party.add(b);
  ck(!party.life[0].valid, "add sin vida deja el hueco como guardado antiguo");
  party.releaseAt(0);
}

int main() {
  testPartyLife();
  printf(bad ? "%d FAILED\n" : "all passed\n", bad);
  return bad ? 1 : 0;
}
```

- [ ] **Step 2: Ejecutar y ver que falla**

Run: `bash tools/emu/tests/run.sh raise`
Expected: `DID NOT COMPILE` (`PartyLife` no existe).

- [ ] **Step 3: `PartyLife` en `party.h`** — justo antes de `class Party {`:

```cpp
// Lo que un PartyMon no guarda de la vida de la mascota (edad exacta, vinculo,
// peso, descuidos...), para que CRIAR la devuelva tal cual la dejaste. Va en
// claves NVS propias, "plife" y "blife", paralelas a "party" y "box": esas no
// cambian de formato (ver el aviso de begin() sobre agrandar PartyMon) y un
// guardado antiguo simplemente no tiene estas, asi que sus huecos quedan con
// valid = 0 y vuelven a criarse con la edad de su nivel.
struct PartyLife {
  uint8_t valid = 0;
  uint8_t bond = 0, bondToday = 0, weight = 0;
  uint8_t careMistakes = 0, mistakeCooldown = 0;
  uint8_t berryKnown = 0, lastLearnLevel = 0, evoPen = 0, evoDeclinedLv = 0;
  uint16_t goodTicks = 0;
  uint32_t ageMinutes = 0;
  uint32_t farDeclinedAge = 0;
};
```

Dentro de `class Party`, debajo de `PartyMon box[BOX_SLOTS];`:

```cpp
  PartyLife life[PARTY_SLOTS];     // la vida de slots[i]
  PartyLife boxLife[BOX_SLOTS];    // la vida de box[i]
```

Y cambia estas tres declaraciones:

```cpp
  bool add(const PartyMon &m, const PartyLife &l = PartyLife());  // into the first free slot; false if full
  void replaceAt(uint8_t i, const PartyMon &m, const PartyLife &l = PartyLife());
  bool boxAdd(const PartyMon &m, const PartyLife &l = PartyLife());     // into the first free box slot
```

- [ ] **Step 4: `party.cpp`**

En `Party::begin()`, justo antes de `// The box is a separate key and simply absent on an older save, ...`:

```cpp
  for (auto &l : life) l = PartyLife();
  if (prefs.getBytesLength("plife") == sizeof(life)) prefs.getBytes("plife", life, sizeof(life));
  for (int i = 0; i < PARTY_SLOTS; i++) if (slots[i].empty()) life[i] = PartyLife();
```

Al final de `Party::begin()`, después del último bucle sobre `box`:

```cpp
  for (auto &l : boxLife) l = PartyLife();
  if (prefs.getBytesLength("blife") == sizeof(boxLife)) prefs.getBytes("blife", boxLife, sizeof(boxLife));
  for (int i = 0; i < BOX_SLOTS; i++) if (box[i].empty()) boxLife[i] = PartyLife();
```

Sustituye `save` y `boxSave`:

```cpp
void Party::save() {
  prefs.putBytes("party", slots, sizeof(slots));
  prefs.putBytes("plife", life, sizeof(life));
}

void Party::boxSave() {
  prefs.putBytes("box", box, sizeof(box));
  prefs.putBytes("blife", boxLife, sizeof(boxLife));
}
```

Sustituye los mutadores:

```cpp
bool Party::boxAdd(const PartyMon &m, const PartyLife &l) {
  int i = boxFirstFree();
  if (i < 0) return false;
  box[i] = m;
  boxLife[i] = l;
  boxSave();
  return true;
}

void Party::boxReleaseAt(uint8_t i) {
  if (i >= BOX_SLOTS) return;
  box[i] = PartyMon();
  boxLife[i] = PartyLife();
  boxSave();
}

void Party::swapPartyBox(uint8_t partyIdx, uint8_t boxIdx) {
  if (partyIdx >= PARTY_SLOTS || boxIdx >= BOX_SLOTS) return;
  PartyMon t = slots[partyIdx];
  slots[partyIdx] = box[boxIdx];
  box[boxIdx] = t;
  PartyLife tl = life[partyIdx];
  life[partyIdx] = boxLife[boxIdx];
  boxLife[boxIdx] = tl;
  save();
  boxSave();
}

bool Party::add(const PartyMon &m, const PartyLife &l) {
  int i = firstFree();
  if (i < 0) return false;
  slots[i] = m;
  life[i] = l;
  save();
  return true;
}

void Party::replaceAt(uint8_t i, const PartyMon &m, const PartyLife &l) {
  if (i >= PARTY_SLOTS) return;
  slots[i] = m;
  life[i] = l;
  save();
}

void Party::releaseAt(uint8_t i) {
  if (i >= PARTY_SLOTS) return;
  slots[i] = PartyMon();
  life[i] = PartyLife();
  save();
}
```

- [ ] **Step 5: Claves en `save.cpp`** — sustituye la línea `{ "party", SK_BYTES }, { "box", SK_BYTES },` por:

```cpp
  { "party", SK_BYTES }, { "box", SK_BYTES },
  { "plife", SK_BYTES }, { "blife", SK_BYTES },   // su vida, para CRIAR (party.h)
```

- [ ] **Step 6: Ejecutar y ver que pasa**

Run: `bash tools/emu/tests/run.sh raise`
Expected: todo `PASS`.

Run: `bash tools/emu/tests/run.sh box`
Expected: `box_test` sigue pasando.

- [ ] **Step 7: `wildToParty`, el capturado como miembro del equipo**

Test — en `wild_test.cpp`, antes de `int main()`:

```cpp
static void testToParty() {
  PartyMon pm; PartyLife pl;
  WildMon wm;
  wm.dex = 25; wm.level = 12; wm.shiny = true; wm.ivAtk = 30; wm.ivDef = 9; wm.ivSpe = 31; wm.ivHp = 8;
  uint8_t mv[MOVE_SLOTS] = { 1, 2, 0, 0 };
  wildToParty(wm, mv, pm, pl);
  ck(pm.dex == 25 && pm.level == 12 && pm.shiny == 1 && pm.ivAtk == 30 && pm.ivSpe == 31 &&
     pm.moves[0] == 1 && pm.moves[1] == 2 && pm.trAtk == 0, "el capturado entra con nivel, IVs, movimientos y shiny");
  ck(pl.valid && pl.ageMinutes == 11UL * MINUTES_PER_LEVEL && pl.lastLearnLevel == 12 && pl.bond == 0,
     "su edad equivale a su nivel");
}
```

y en `main()`, debajo de `testRoll();`: `testToParty();`. Ejecuta `bash tools/emu/tests/run.sh wild` y comprueba que no compila (`wildToParty` no existe).

Declaración — en `wild.h`, debajo de `bool wildRoll(Pet &p, WildMon &out);`:

```cpp
// El capturado como miembro del equipo: entrenamiento a 0, edad = su nivel.
void wildToParty(const WildMon &w, const uint8_t moves[MOVE_SLOTS], PartyMon &m, PartyLife &l);
```

Implementación — en `wild.cpp` (al final):

```cpp
void wildToParty(const WildMon &w, const uint8_t moves[MOVE_SLOTS], PartyMon &m, PartyLife &l) {
  m = PartyMon();
  l = PartyLife();
  m.dex = w.dex;
  m.level = w.level;
  m.shiny = w.shiny ? 1 : 0;
  m.ivAtk = w.ivAtk; m.ivDef = w.ivDef; m.ivSpe = w.ivSpe; m.ivHp = w.ivHp;
  for (int i = 0; i < MOVE_SLOTS; i++) m.moves[i] = moves[i];
  l.valid = 1;
  l.ageMinutes = (uint32_t)(w.level ? w.level - 1 : 0) * MINUTES_PER_LEVEL;
  l.lastLearnLevel = w.level;   // no repetir las ofertas de movimientos ya pasadas
}
```

Run: `bash tools/emu/tests/run.sh wild`
Expected: todo `PASS`, incluidas las dos comprobaciones del capturado.

- [ ] **Step 8: Compilar el firmware.** Las llamadas existentes `party.add(m)`, `party.boxAdd(...)` y `party.replaceAt(i, m)` siguen compilando por los parámetros por defecto. Expected: compila.

- [ ] **Step 9: Commit**

```bash
git add party.h party.cpp save.cpp wild.h wild.cpp tools/emu/tests/raise_test.cpp tools/emu/tests/wild_test.cpp
git commit -m "party: la vida de cada guardado viaja con su hueco (plife/blife)"
```

---

### Task 7: `Pet::bankFull` y `Pet::raiseFrom`

**Files:**
- Modify: `pet.h`, `pet.cpp`
- Test: `tools/emu/tests/raise_test.cpp`

- [ ] **Step 1: Añadir el test que falla** — en `raise_test.cpp`, antes de `int main()`:

```cpp
static void testBankRaise() {
  Pet p;
  p.begin();
  p.dbgHatchAs(4, false);
  p.ageMinutes = 900; p.bond = 55; p.weight = 12; p.berryKnown = true;
  p.careMistakes = 2; p.trAtk = 9; p.medals = 3;
  strcpy(p.nick, "FUEGO");
  PartyMon m; PartyLife l;
  p.bankFull(m, l);
  ck(m.dex == 4 && m.trAtk == 9 && m.medals == 3 && !strcmp(m.nick, "FUEGO"), "bankFull guarda lo de combate");
  ck(l.valid && l.ageMinutes == 900 && l.bond == 55 && l.weight == 12 && l.berryKnown && l.careMistakes == 2,
     "bankFull guarda la vida");

  p.dbgHatchAs(1, false);
  p.fullness = 5; p.poops = 3;
  p.raiseFrom(m, l);
  ck(p.speciesId == 4 && p.ageMinutes == 900 && p.bond == 55 && p.weight == 12 &&
     p.berryKnown && p.careMistakes == 2 && p.trAtk == 9, "raiseFrom la devuelve tal cual");
  ck(!p.frozen, "y crece: no queda congelada");
  ck(p.fullness == 80 && p.joy == 80 && p.energy == 80 && p.hygiene == 100 && p.poops == 0,
     "necesidades restauradas");

  PartyMon old;
  old.dex = 1; old.level = 30;
  p.raiseFrom(old, PartyLife());
  ck(p.speciesId == 1 && p.level() == 30 && p.bond == 0 && p.weight == 0 && !p.frozen,
     "un guardado antiguo vuelve con la edad de su nivel");
}
```

y en `main()`, debajo de `testPartyLife();`:

```cpp
  testBankRaise();
```

- [ ] **Step 2: Ejecutar y ver que falla**

Run: `bash tools/emu/tests/run.sh raise`
Expected: `DID NOT COMPILE` (`bankFull` no existe).

- [ ] **Step 3: Declarar en `pet.h`**

Debajo de `PartyMon endedMon;`:

```cpp
  PartyLife endedLife;          // la vida de endedMon (party.h)
```

Debajo de `void registerCaught(int16_t dex, bool shinyMon);` (Task 4):

```cpp
  // CRIAR a demanda. bankFull() vuelca la mascota entera (combate y vida) para
  // guardarla en el equipo; raiseFrom() pone a criar un guardado. A diferencia
  // de reviveFrom(), el que vuelve CRECE: envejece, evoluciona y puede
  // despedirse. Las necesidades se restauran; todo lo demas vuelve tal cual.
  void bankFull(PartyMon &m, PartyLife &l) const;
  void raiseFrom(const PartyMon &m, const PartyLife &l);
```

- [ ] **Step 4: Implementar en `pet.cpp`** — justo debajo de la función `void Pet::reviveFrom(const PartyMon &m) { ... }`:

```cpp
void Pet::bankFull(PartyMon &m, PartyLife &l) const {
  m = PartyMon();
  l = PartyLife();
  if (isEgg()) return;
  m.dex = speciesId;
  m.level = level();
  m.medals = medals;
  m.ivAtk = ivAtk; m.ivDef = ivDef; m.ivSpe = ivSpe; m.ivHp = ivHp;
  m.trAtk = trAtk; m.trDef = trDef; m.trSpe = trSpe;
  m.shiny = shiny ? 1 : 0;
  for (int i = 0; i < MOVE_SLOTS; i++) m.moves[i] = moves[i];
  strncpy(m.nick, nick, sizeof(m.nick) - 1);
  m.nick[sizeof(m.nick) - 1] = 0;
  l.valid = 1;
  l.ageMinutes = ageMinutes;
  l.bond = bond;
  l.bondToday = bondToday;
  l.weight = weight;
  l.careMistakes = careMistakes;
  l.mistakeCooldown = mistakeCooldown;
  l.berryKnown = berryKnown ? 1 : 0;
  l.lastLearnLevel = lastLearnLevel;
  l.evoPen = evoPen;
  l.evoDeclinedLv = evoDeclinedLv;
  l.goodTicks = goodTicks;
  l.farDeclinedAge = farDeclinedAge;
}

void Pet::raiseFrom(const PartyMon &m, const PartyLife &l) {
  if (m.empty()) return;
  reviveFrom(m);           // especie, IVs, entrenamiento, movimientos, apodo, necesidades
  frozen = false;          // criar de verdad
  retirePending = false;
  if (l.valid) {
    ageMinutes = l.ageMinutes;
    bond = l.bond;
    bondToday = l.bondToday;
    weight = l.weight;
    careMistakes = l.careMistakes;
    mistakeCooldown = l.mistakeCooldown;
    berryKnown = l.berryKnown != 0;
    lastLearnLevel = l.lastLearnLevel;
    evoPen = l.evoPen;
    evoDeclinedLv = l.evoDeclinedLv;
    goodTicks = l.goodTicks;
    farDeclinedAge = l.farDeclinedAge;
  } else {                 // guardado de antes de PartyLife: lo que no sabemos, a cero
    evoPen = 0;
    evoDeclinedLv = 0;
    goodTicks = 0;
    farDeclinedAge = 0;
  }
  save();
}
```

- [ ] **Step 5: Que la despedida también guarde la vida** — en `Pet::snapshotForParty()`, sustituye el bloque que va desde `endedMon = PartyMon();` hasta `endedMon.nick[sizeof(endedMon.nick) - 1] = 0;` (ambos incluidos) por:

```cpp
  bankFull(endedMon, endedLife);
```

- [ ] **Step 6: Ejecutar y ver que pasa**

Run: `bash tools/emu/tests/run.sh raise`
Expected: todo `PASS`.

Run: `bash tools/emu/tests/run.sh`
Expected: `failed: 0` (`revive_test`, `retire_test` y `release_test` siguen pasando).

- [ ] **Step 7: Commit**

```bash
git add pet.h pet.cpp tools/emu/tests/raise_test.cpp
git commit -m "pet: bankFull/raiseFrom para criar a demanda sin perder la vida"
```

---

### Task 8: Huevo guardado

**Files:**
- Modify: `pet.h`, `pet.cpp`, `save.cpp`
- Test: `tools/emu/tests/raise_test.cpp`

- [ ] **Step 1: Añadir el test que falla** — antes de `int main()`:

```cpp
static void testStoredEgg() {
  Pet p;
  p.begin();
  p.dbgHatchAs(4, false);
  p.registerCaught(4, false);       // ya hay registrados: el huevo nuevo no pide inicial
  p.newEgg();
  ck(p.isEgg() && !p.awaitingStarter(), "hay un huevo esperando");
  int16_t target = p.eggPeek();
  ck(p.stashEgg() && p.hasStoredEgg(), "guarda el huevo aparte");
  ck(!p.stashEgg(), "solo cabe un huevo guardado");

  PartyMon m;
  m.dex = 7; m.level = 15;
  p.raiseFrom(m, PartyLife());
  ck(!p.isEgg() && p.speciesId == 7, "otro ocupa su lugar");

  Pet q;
  q.begin();                        // carga desde NVS
  ck(q.hasStoredEgg(), "el huevo guardado sobrevive a un reinicio");

  ck(p.restoreEgg() && p.isEgg() && p.eggPeek() == target && !p.hasStoredEgg(),
     "vuelve el mismo huevo");
  ck(!p.restoreEgg(), "no hay segundo huevo que restaurar");
}
```

y en `main()`, debajo de `testBankRaise();`:

```cpp
  testStoredEgg();
```

- [ ] **Step 2: Ejecutar y ver que falla**

Run: `bash tools/emu/tests/run.sh raise`
Expected: `DID NOT COMPILE` (`stashEgg` no existe).

- [ ] **Step 3: `StoredEgg` en `pet.h`** — justo antes de `class Pet {`:

```cpp
// Un huevo apartado al poner a criar a otro (CRIAR con un huevo esperando).
// Conserva lo ya sorteado (especie, shiny) y su progreso. Clave NVS "egg2".
struct StoredEgg {
  uint8_t valid = 0;
  uint8_t eggTaps = 0;
  uint8_t eggShiny = 0;
  uint8_t lastEnd = 0;
  uint8_t evoPen = 0;
  uint8_t pad = 0;
  int16_t eggTarget = 1;
  uint32_t ageMinutes = 0;
  int16_t eggByRegion[REGION_COUNT] = { 0 };
};
```

En la parte pública, debajo de `void raiseFrom(...)`:

```cpp
  bool hasStoredEgg() const { return storedEgg.valid != 0; }
  bool stashEgg();      // aparta el huevo activo; false si no es huevo o ya hay uno
  bool restoreEgg();    // lo vuelve a poner como mascota activa
```

En la parte privada, debajo de `bool eggShiny = false;`:

```cpp
  StoredEgg storedEgg;
```

- [ ] **Step 4: Implementar en `pet.cpp`** — debajo de `Pet::raiseFrom`:

```cpp
bool Pet::stashEgg() {
  if (!isEgg() || starterPick || storedEgg.valid) return false;
  storedEgg = StoredEgg();
  storedEgg.valid = 1;
  storedEgg.eggTaps = eggTaps;
  storedEgg.eggShiny = eggShiny ? 1 : 0;
  storedEgg.lastEnd = lastEnd;
  storedEgg.evoPen = evoPen;
  storedEgg.eggTarget = eggTarget;
  storedEgg.ageMinutes = ageMinutes;
  for (int i = 0; i < REGION_COUNT; i++) storedEgg.eggByRegion[i] = eggByRegion[i];
  if (opened) prefs.putBytes("egg2", &storedEgg, sizeof(storedEgg));
  return true;
}

bool Pet::restoreEgg() {
  if (!storedEgg.valid) return false;
  ceremony = CER_NONE;
  neglectTicks = 0;
  frozen = false;
  retirePending = false;
  speciesId = -1;
  prevSpeciesId = -1;
  eggTaps = storedEgg.eggTaps;
  eggShiny = storedEgg.eggShiny != 0;
  lastEnd = storedEgg.lastEnd;
  evoPen = storedEgg.evoPen;
  eggTarget = storedEgg.eggTarget;
  ageMinutes = storedEgg.ageMinutes;
  for (int i = 0; i < REGION_COUNT; i++) eggByRegion[i] = storedEgg.eggByRegion[i];
  fullness = joy = energy = 80;
  hygiene = 100;
  poops = 0;
  weight = 0;
  sleeping = false;
  storedEgg = StoredEgg();
  if (opened) prefs.putBytes("egg2", &storedEgg, sizeof(storedEgg));
  save();
  return true;
}
```

- [ ] **Step 5: Cargarlo** — en `Pet::load()`, al final de la función (antes de su `}` de cierre):

```cpp
  storedEgg = StoredEgg();
  if (prefs.getBytesLength("egg2") == sizeof(storedEgg))
    prefs.getBytes("egg2", &storedEgg, sizeof(storedEgg));
```

- [ ] **Step 6: El huevo guardado sustituye al nuevo al terminar una mascota** — en `pet.cpp`, en la línea ~155 (el `newEgg();` que va justo después de `snapshotForParty();  // hand it over BEFORE newEgg() erases everything`), sustituye `newEgg();` por:

```cpp
    if (!restoreEgg()) newEgg();   // un huevo apartado tiene preferencia
```

No toques el `newEgg();` de la línea ~56 (primera partida).

- [ ] **Step 7: Clave en `save.cpp`** — en la línea de `plife`/`blife` (Task 6):

```cpp
  { "plife", SK_BYTES }, { "blife", SK_BYTES },   // su vida, para CRIAR (party.h)
  { "egg2", SK_BYTES },                            // huevo apartado (pet.h)
```

- [ ] **Step 8: Ejecutar y ver que pasa**

Run: `bash tools/emu/tests/run.sh raise` y luego `bash tools/emu/tests/run.sh`
Expected: todo `PASS`, `failed: 0`.

- [ ] **Step 9: Commit**

```bash
git add pet.h pet.cpp save.cpp tools/emu/tests/raise_test.cpp
git commit -m "pet: huevo guardado al criar a otro, y vuelve al terminar la mascota"
```

---

### Task 9: Textos (6 idiomas)

**Files:**
- Modify: `i18n.h`, `i18n.cpp`

- [ ] **Step 1: Ids en `i18n.h`** — justo antes de `STR_COUNT`:

```cpp
  // encuentros salvajes y criar a demanda (wild.cpp)
  S_EXPLORE,        // fila del menu y titulo de su pantalla
  S_SEARCH_FMT,     // "BUSCAR %u/%u"
  S_BALLS_FMT,      // "POKEBALLS %u"
  S_COINS_FMT,      // "MONEDAS %u"
  S_BUY1,           // tienda: +1 bola
  S_BUY10,          // tienda: +10 bolas
  S_NO_BALLS,       // explorar sin bolas
  S_NO_SEARCH,      // sin busquedas hoy
  S_WILD_APPEARS,   // "Un %s salvaje!"
  S_BALL,           // opcion del combate
  S_AIM_PERFECT, S_AIM_GOOD,
  S_CAUGHT,         // "Atrapaste a %s!"
  S_BROKE_FREE,     // "%s se escapo de la bola!"
  S_FLED,           // "%s huyo!"
  S_RAISE_Q,        // "Criarlo ahora?"
  S_RAISE_HINT,     // que pasa con la mascota actual
  S_RAISE,          // boton de la ficha del equipo
  S_RAISE_NO,       // por que no se puede criar ahora
  S_EGG_SLOT,       // pildora del huevo guardado
  S_WILD_COINS_FMT, // "+%u monedas"
```

- [ ] **Step 2: Cadenas en `i18n.cpp`** — `STRINGS` es posicional: en cada fila de idioma, añade estas cadenas **en este orden**, justo después de la última cadena de la fila y antes de su `},`. Solo ASCII (la fuente 5×7 no tiene acentos).

Fila ES (la primera; termina en `"no se unira a tu equipo",`):

```cpp
    "EXPLORAR", "BUSCAR %u/%u", "POKEBALLS %u", "MONEDAS %u", "+1 BALL (10)", "+10 BALLS (100)",
    "sin Pokeballs: no podras capturar", "hoy no quedan busquedas", "Un %s salvaje!", "BALL",
    "PERFECTO", "BIEN", "Atrapaste a %s!", "%s se escapo de la bola!", "%s huyo!",
    "Criarlo ahora?", "tu mascota actual ira al equipo", "CRIAR", "ahora no se puede criar",
    "HUEVO", "+%u monedas",
```

Fila EN (termina en `"it will not join your party",`):

```cpp
    "EXPLORE", "SEARCH %u/%u", "POKEBALLS %u", "COINS %u", "+1 BALL (10)", "+10 BALLS (100)",
    "no Poke Balls: you cannot catch", "no searches left today", "A wild %s appeared!", "BALL",
    "PERFECT", "GOOD", "Gotcha! %s was caught!", "%s broke free!", "%s fled!",
    "Raise it now?", "your current one joins the party", "RAISE", "cannot raise right now",
    "EGG", "+%u coins",
```

Fila FR:

```cpp
    "EXPLORER", "CHERCHER %u/%u", "POKEBALLS %u", "PIECES %u", "+1 BALL (10)", "+10 BALLS (100)",
    "sans Poke Balls: pas de capture", "plus de recherches aujourd'hui", "Un %s sauvage!", "BALL",
    "PARFAIT", "BIEN", "%s est capture!", "%s s'est libere!", "%s s'enfuit!",
    "L'elever maintenant?", "l'actuel rejoint l'equipe", "ELEVER", "impossible pour l'instant",
    "OEUF", "+%u pieces",
```

Fila DE:

```cpp
    "ERKUNDEN", "SUCHEN %u/%u", "POKEBALLS %u", "MUENZEN %u", "+1 BALL (10)", "+10 BALLS (100)",
    "keine Poke Balls: kein Fangen", "heute keine Suche mehr", "Ein wildes %s!", "BALL",
    "PERFEKT", "GUT", "%s gefangen!", "%s hat sich befreit!", "%s ist geflohen!",
    "Jetzt aufziehen?", "der jetzige kommt ins Team", "AUFZIEHEN", "gerade nicht moeglich",
    "EI", "+%u Muenzen",
```

Fila IT:

```cpp
    "ESPLORA", "CERCA %u/%u", "POKEBALLS %u", "MONETE %u", "+1 BALL (10)", "+10 BALLS (100)",
    "niente Poke Ball: non puoi catturare", "niente ricerche oggi", "Un %s selvatico!", "BALL",
    "PERFETTO", "BENE", "Preso %s!", "%s si e liberato!", "%s e fuggito!",
    "Allevarlo ora?", "l'attuale va in squadra", "ALLEVA", "ora non si puo allevare",
    "UOVO", "+%u monete",
```

Fila PT:

```cpp
    "EXPLORAR", "PROCURAR %u/%u", "POKEBALLS %u", "MOEDAS %u", "+1 BALL (10)", "+10 BALLS (100)",
    "sem Pokebolas: nao podes capturar", "sem buscas hoje", "Um %s selvagem!", "BALL",
    "PERFEITO", "BOM", "Apanhaste %s!", "%s libertou-se!", "%s fugiu!",
    "Cria-lo agora?", "o atual vai para a equipa", "CRIAR", "agora nao da para criar",
    "OVO", "+%u moedas",
```

- [ ] **Step 3: Verificar**

Run: `bash tools/emu/tests/run.sh i18n`
Expected: `i18n_test` pasa (ninguna fila corta, `%` iguales entre idiomas).

Compilar el firmware. Expected: compila.

- [ ] **Step 4: Commit**

```bash
git add i18n.h i18n.cpp
git commit -m "i18n: textos de encuentros salvajes y criar"
```

---

### Task 10: Estado global, ganancias de monedas y comandos serie

**Files:**
- Modify: `TamaPoke.ino`

- [ ] **Step 1: Include y estado global** — en los includes del principio, debajo de `#include "pin_config.h"` añade `#include "wild.h"`. Justo debajo de la línea `Combatant btlYou, btlFoe;` añade:

```cpp
// ---------- encuentros salvajes (wild.cpp) ----------
WildState gWild;             // monedas, bolas y contadores del dia
WildMon wildMon;             // el salvaje del combate en curso
bool btlWild = false;        // el combate actual es un encuentro salvaje
bool exploreOpen = false;    // pantalla EXPLORAR
uint32_t wildBubbleUntil = 0;  // aviso espontaneo "!" visible hasta
uint8_t wildFails = 0;       // lanzamientos fallidos en este encuentro
uint8_t throwStage = 0;      // 0 nada, 1 anillo encogiendo, 2 tambaleos
uint32_t throwT0 = 0;
uint8_t throwShakes = 0;     // tambaleos que aguanta (3 = capturado)
uint8_t throwAim = AIM_MISS;
bool caughtPending = false;  // capturado: preguntar al cerrar el combate
bool raiseAsk = false;       // dialogo "Criarlo ahora?"
bool eggConfirm = false;     // confirmar criar el huevo guardado
PartyMon caughtMon;
PartyLife caughtLife;
// La burbuja "!" se dibuja en render() y se toca en onTap(), ambos muy por
// encima de la pantalla EXPLORAR: sus coordenadas tienen que estar aqui.
#define WILD_BUB_X (CX + 90)
#define WILD_BUB_Y 150
// Declaradas aqui porque handleSerial() y onTap() las usan miles de lineas antes
// de su definicion: el proto.h del emulador lo perdona, arduino-cli no.
void startWildBattle(const WildMon &w);
void renderExplore();
void exploreTap(int16_t x, int16_t y);
```

- [ ] **Step 2: Cargar al arrancar** — en `setup()`, justo después de `pet.syncClock(e);`:

```cpp
  wildLoad(gWild, pet);
```

- [ ] **Step 3: `wildTick` (monedas por cuidado y medallas, día nuevo, aviso espontáneo)** — justo antes de `void loop() {`:

```cpp
// Se puede empezar un encuentro ahora? Ni huevo, ni ceremonia, ni dormida.
static bool wildCanStart() {
  return !pet.isEgg() && pet.ceremony == CER_NONE && !pet.sleeping && !pet.evolving();
}

// Un aviso espontaneo solo sale sobre la pantalla principal, encendida y libre.
static bool wildCanAmbient() {
  return wildCanStart() && !screenOff && !menuOpen && !raiseAsk &&
         uiCurrentScreen() == SCR_MAIN;
}

// Una vez por segundo: dia nuevo, monedas por el primer cuidado del dia y por
// medallas nuevas (sondeando pet, que no sabe nada de monedas), y el sorteo del
// aviso espontaneo una vez por minuto (1/60: ~1 por hora, maximo 3 al dia).
static void wildTick(uint32_t now) {
  static uint32_t last = 0, lastAmb = 0;
  if (now - last < 1000) return;
  last = now;
  bool dirty = false;
  uint32_t e = rtcEpoch();
  if (e && e / 86400 != gWild.day) { wildNewDay(gWild, e / 86400); dirty = true; }
  if (pet.lastCareDay && pet.lastCareDay != gWild.paidCareDay) {
    gWild.paidCareDay = pet.lastCareDay;
    wildEarn(gWild, wildCareCoins(pet.streak));
    dirty = true;
  }
  if (pet.totalMedals != gWild.paidMedals) {      // menos = tras un WIPE: solo re-sincroniza
    if (pet.totalMedals > gWild.paidMedals)
      wildEarn(gWild, 30 * (pet.totalMedals - gWild.paidMedals));
    gWild.paidMedals = pet.totalMedals;
    dirty = true;
  }
  if (now - lastAmb >= 60000) {
    lastAmb = now;
    if (!wildBubbleUntil && gWild.ambient < WILD_AMBIENT_PER_DAY && wildCanAmbient() &&
        random(60) == 0) {
      wildBubbleUntil = now + 30000;
      sfxPlay(SFX_MEDAL);
    }
  }
  if (wildBubbleUntil && (now > wildBubbleUntil || !wildCanAmbient())) wildBubbleUntil = 0;
  if (dirty) wildSave(gWild);
}
```

En `loop()`, justo debajo de `pet.update(now);`:

```cpp
  wildTick(now);
```

- [ ] **Step 4: Comandos serie** — en `handleSerial()`, justo antes de `} else if (line == "KEYPIN") {`:

```cpp
  } else if (line == "WILD" || line.startsWith("WILD ")) {   // forzar un encuentro
    WildMon w;
    bool ok;
    if (line.length() > 5) {           // WILD <dex> [nivel]
      String a = line.substring(5);
      int sp = a.indexOf(' ');
      w.dex = (int16_t)(sp < 0 ? a : a.substring(0, sp)).toInt();
      w.level = (uint8_t)(sp < 0 ? pet.level() : a.substring(sp + 1).toInt());
      ok = w.dex >= 1 && w.dex <= DEX_COUNT && w.level >= 1;
      if (ok) w.tier = DEX_TBL[w.dex].rarity == R_LEGENDARIO ? R_LEGENDARIO
                     : DEX_TBL[w.dex].rarity == R_RARO ? R_RARO : R_COMUN;
    } else {
      ok = wildRoll(pet, w);
    }
    if (ok && wildCanStart()) { exploreOpen = false; menuOpen = false; startWildBattle(w); }
    Serial.printf("wild %d lv%u tier%u shiny%d -> %s\n", w.dex, w.level, w.tier, w.shiny,
                  battleOpen && btlWild ? "combate" : "no");
    Serial.println("DONE");
  } else if (line.startsWith("COINS ")) {
    long v = line.substring(6).toInt();
    gWild.coins = (uint16_t)(v < 0 ? 0 : v > WILD_COINS_MAX ? WILD_COINS_MAX : v);
    wildSave(gWild);
    Serial.println("DONE");
  } else if (line.startsWith("BALLS ")) {
    long v = line.substring(6).toInt();
    gWild.balls = (uint8_t)(v < 0 ? 0 : v > WILD_BALLS_MAX ? WILD_BALLS_MAX : v);
    wildSave(gWild);
    Serial.println("DONE");
  } else if (line == "WILDINFO") {
    Serial.printf("coins=%u balls=%u explores=%u/%u ambient=%u/%u day=%u egg=%d\n",
                  gWild.coins, gWild.balls, gWild.explores, WILD_EXPLORES_PER_DAY,
                  gWild.ambient, WILD_AMBIENT_PER_DAY, (unsigned)gWild.day, pet.hasStoredEgg());
    Serial.println("DONE");
```

(`startWildBattle` llega en la Task 11; esta task compila después de la 11. Haz el commit de las Tasks 10 y 11 juntas.)

---

### Task 11: Combate salvaje, BALL, anillo de timing y captura

**Files:**
- Modify: `TamaPoke.ino`

- [ ] **Step 1: Los otros combates no son salvajes** — añade `btlWild = false;` como **primera línea** del cuerpo de `startLinkBattle()`, `startTrainerBattle(...)` y `startBattle(...)`.

- [ ] **Step 2: `startWildBattle`** — justo debajo de la función `startBattle(...)`:

```cpp
// Un encuentro salvaje: el combate suelto de siempre, pero con el salvaje que
// salio del sorteo (sus IVs y su shiny) y la opcion BALL en el menu.
void startWildBattle(const WildMon &w) {
  if (pet.isEgg() || pet.ceremony != CER_NONE) return;
  btlTrainer = -1;           // startBattle no lo toca: que no herede el del ultimo gimnasio
  btlHard = false;
  startBattle(w.dex, w.level);
  if (!battleOpen) return;
  Pet foe;                   // sin begin(): nunca escribe en la NVS (ver pet.h)
  foe.dbgHatchAs(w.dex, w.shiny);
  foe.ivAtk = w.ivAtk; foe.ivDef = w.ivDef; foe.ivSpe = w.ivSpe; foe.ivHp = w.ivHp;
  foe.ageMinutes = (uint32_t)(w.level ? w.level - 1 : 0) * MINUTES_PER_LEVEL;
  foe.relearnFromLevel();
  combatantFromPet(btlFoe, foe);
  btlHpShown[1] = btlFoe.maxHp;
  btlSyncSprite(1, btlFoe);
  wildMon = w;
  btlWild = true;
  wildFails = 0;
  throwStage = 0;
  caughtPending = false;
  btlMsgCount = 0;
  btlSay(T(S_WILD_APPEARS), btlFoe.name);
}
```

- [ ] **Step 3: Lanzamiento** — justo debajo de `startWildBattle`:

```cpp
// Anillo de timing: encoge de RING_R0 a RING_R1 en RING_MS sobre el salvaje;
// se toca cuando coincide con el circulo objetivo.
#define RING_MS 1200
#define RING_R0 90
#define RING_R1 10
#define RING_TARGET 30
#define THROW_CX 340         // centro del sprite del rival (btlSide(..., 300, 40, ...))
#define THROW_CY 100
#define SHAKE_MS 600

static int ringRadius(uint32_t t) {
  if (t > RING_MS) t = RING_MS;
  return RING_R0 - (int)((RING_R0 - RING_R1) * t / RING_MS);
}

static uint8_t aimFromRadius(int r) {
  int d = r > RING_TARGET ? r - RING_TARGET : RING_TARGET - r;
  return d <= 5 ? AIM_PERFECT : d <= 12 ? AIM_GOOD : d <= 25 ? AIM_OK : AIM_MISS;
}

static void btlThrowStart() {
  if (!gWild.balls) { sfxPlay(SFX_DENY); btlSay("%s", T(S_NO_BALLS)); return; }
  gWild.balls--;
  wildSave(gWild);
  throwStage = 1;
  throwT0 = millis();
  sfxPlay(SFX_TAP);
}

static void btlThrowRelease(uint8_t aim) {
  throwAim = aim;
  uint8_t a = wildCatchValue(wildMon.tier, btlFoe.hp, btlFoe.maxHp, btlFoe.ailment, aim);
  throwShakes = wildShakes(a);
  throwStage = 2;
  throwT0 = millis();
  sfxPlay(SFX_TAP);
}

static void wildCaught() {
  btlMsgCount = 0;
  btlSay(T(S_CAUGHT), btlFoe.name);
  sfxPlay(SFX_VICTORY);
  audioMusic(MUS_VICTORY);
  wildToParty(wildMon, btlFoe.moves, caughtMon, caughtLife);
  pet.registerCaught(wildMon.dex, wildMon.shiny);
  btlOver = true;
  btlWon = true;
  caughtPending = true;      // al cerrar el combate: "Criarlo ahora?"
}

// Fin de los tambaleos: capturado, o se escapa de la bola, quiza huye, y si no
// el salvaje juega su turno como en los juegos.
static void btlThrowFinish() {
  throwStage = 0;
  if (throwShakes >= 3) { wildCaught(); return; }
  wildFails++;
  btlMsgCount = 0;
  btlSay(T(S_BROKE_FREE), btlFoe.name);
  if (wildFlees(wildFails)) {
    btlSay(T(S_FLED), btlFoe.name);
    btlOver = true;
    btlWon = false;
    return;
  }
  TurnLog lg;
  uint16_t hp0 = btlYou.hp;
  battleAct(btlFoe, btlYou, aiChooseMove(btlFoe, btlYou, false), lg);
  btlNarrate(btlFoe, btlYou, lg);
  if (btlYou.hp < hp0) btlHitUntil[0] = millis() + BTL_HIT_MS;
  if (btlYou.fainted()) {
    if (btlSquadAt + 1 < btlSquadN) {
      btlFaintUntil[0] = millis() + BTL_FAINT_MS;
      btlSwapWho = 0;
    } else {
      btlOver = true;
      btlWon = false;
      btlSay("%s", T(S_BTL_LOSE));
    }
  }
}

// Avanza la animacion; lo llama renderBattle en cada cuadro.
static void btlThrowTick() {
  uint32_t t = millis() - throwT0;
  if (throwStage == 1 && t > RING_MS + 150) btlThrowRelease(AIM_MISS);   // no toco
  else if (throwStage == 2 && t > (uint32_t)((throwShakes < 3 ? throwShakes : 3) + 1) * SHAKE_MS)
    btlThrowFinish();
}

static void drawThrow() {
  uint32_t t = millis() - throwT0;
  if (throwStage == 1) {
    gfx->drawCircle(THROW_CX, THROW_CY, RING_TARGET, UI_BAR_OK);
    gfx->drawCircle(THROW_CX, THROW_CY, RING_TARGET + 1, UI_BAR_OK);
    int r = ringRadius(t);
    gfx->drawCircle(THROW_CX, THROW_CY, r, UI_INK);
    gfx->drawCircle(THROW_CX, THROW_CY, r + 1, UI_INK);
  } else if (throwStage == 2) {
    // la bola se tambalea: un vaiven por cada tambaleo que aguanta
    uint32_t k = t / SHAKE_MS;
    int dx = (k < throwShakes && (t % SHAKE_MS) < SHAKE_MS / 2) ? ((k & 1) ? 6 : -6) : 0;
    int bx = THROW_CX + dx, by = THROW_CY + 20;
    gfx->fillCircle(bx, by, 16, UI_WHITE);
    gfx->fillRect(bx - 16, by - 16, 33, 16, UI_BAR_BAD);   // mitad roja de arriba
    gfx->drawCircle(bx, by, 16, UI_INK);
    gfx->drawFastHLine(bx - 16, by, 33, UI_INK);
    gfx->fillCircle(bx, by, 4, UI_WHITE);
    gfx->drawCircle(bx, by, 4, UI_INK);
  }
  if (throwStage == 1 || (throwStage == 2 && t < 500)) {
    const char *lbl = throwStage == 2 && throwAim == AIM_PERFECT ? T(S_AIM_PERFECT)
                    : throwStage == 2 && throwAim == AIM_GOOD ? T(S_AIM_GOOD) : T(S_BALL);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(lbl) * 6, BTL_GRID_Y + 30);
    gfx->print(lbl);
  }
}
```

- [ ] **Step 4: Dibujar en `renderBattle`**

Justo debajo de `btlEaseBars();` añade:

```cpp
  if (throwStage) btlThrowTick();
```

Sustituye el comienzo de la cadena del menú, `if (lanWait && !btlMsgCount) {`, por:

```cpp
  if (throwStage) {
    drawThrow();               // el lanzamiento ocupa la zona del menu
  } else if (lanWait && !btlMsgCount) {
```

Dentro de la rama `} else if (btlMenu == 0) {`, sustituye el bloque que dibuja FIGHT a todo lo ancho (desde `gfx->fillRoundRect(BTL_GRID_X, BTL_GRID_Y, 328, BTL_CELL_H, 10, UI_BG_DAY);` hasta `gfx->print(T(S_FIGHT));`) por:

```cpp
    if (btlWild) {
      // salvaje: LUCHA y BALL comparten la fila de arriba
      char bl[16];
      snprintf(bl, sizeof(bl), "%s x%u", T(S_BALL), gWild.balls);
      const char *top[2] = { T(S_FIGHT), bl };
      for (int i = 0; i < 2; i++) {
        int x = BTL_CELL_X(i), y = BTL_CELL_Y(i);
        bool dead = (i == 1 && !gWild.balls);
        gfx->fillRoundRect(x, y, BTL_CELL_W, BTL_CELL_H, 10, dead ? UI_TRACK : UI_BG_DAY);
        gfx->drawRoundRect(x, y, BTL_CELL_W, BTL_CELL_H, 10, UI_INK);
        gfx->setTextColor(dead ? 0x8410 : UI_INK);
        gfx->setTextSize(2);
        gfx->setCursor(x + (BTL_CELL_W - (int)strlen(top[i]) * 12) / 2, y + 14);
        gfx->print(top[i]);
      }
    } else {
      gfx->fillRoundRect(BTL_GRID_X, BTL_GRID_Y, 328, BTL_CELL_H, 10, UI_BG_DAY);
      gfx->drawRoundRect(BTL_GRID_X, BTL_GRID_Y, 328, BTL_CELL_H, 10, UI_INK);
      gfx->setTextColor(UI_INK);
      gfx->setTextSize(2);
      gfx->setCursor(CX - (int)strlen(T(S_FIGHT)) * 6, BTL_GRID_Y + 14);
      gfx->print(T(S_FIGHT));
    }
```

- [ ] **Step 5: Toques en `battleTap`**

Al principio de `battleTap`, justo después de la llave de apertura:

```cpp
  if (throwStage == 1) { btlThrowRelease(aimFromRadius(ringRadius(millis() - throwT0))); return; }
  if (throwStage == 2) return;   // los tambaleos no se interrumpen
```

En la rama `if (btlMsgCount) {`, dentro de `if (btlOver) {`, justo antes de su `return;`:

```cpp
      if (caughtPending) { caughtPending = false; raiseAsk = true; }
      btlWild = false;
```

En la rama `if (btlMenu == 0) {`, sustituye el primer `if` (el de FIGHT a todo lo ancho) por:

```cpp
    if (btlWild) {
      if (btlCellHit(0, x, y)) { sfxPlay(SFX_TAP); btlMenu = 1; return; }   // LUCHA
      if (btlCellHit(1, x, y)) { btlThrowStart(); return; }                  // BALL
    } else if (x >= BTL_GRID_X - BTL_HIT_PAD && x <= BTL_GRID_X + 328 + BTL_HIT_PAD &&
               y >= BTL_HIT_Y0(0) && y <= BTL_HIT_Y1(0)) {
      sfxPlay(SFX_TAP);
      btlMenu = 1;                       // FIGHT
      return;
    }
```

En `btlRun()`, debajo de `battleOpen = false;`:

```cpp
  btlWild = false;
  throwStage = 0;
```

- [ ] **Step 6: Premios y mensaje de victoria en `btlResolve`** — justo antes de `audioMusic(btlWon ? MUS_VICTORY : MUS_NONE);`:

```cpp
    // monedas: medalla nueva 50, repetir un gimnasio 15, debilitar un salvaje 5
    if (btlWon && btlTrainer >= 0) { wildEarn(gWild, btlNewBadge ? 50 : 15); wildSave(gWild); }
    if (btlWon && btlWild) {
      wildEarn(gWild, 5);
      wildSave(gWild);
      if (btlPetIn) btlTrainGain = pet.rewardTraining(1, btlTrainWhich);
    }
```

Y sustituye la última línea del bloque, `btlSay("%s", T(S_BTL_LOSE));`, por:

```cpp
    btlSay("%s", btlWon ? T(S_BTL_WIN) : T(S_BTL_LOSE));
    if (btlWon && btlWild) btlSay(T(S_WILD_COINS_FMT), 5);
```

(Hasta ahora un combate suelto ganado decía «Has perdido...»: era un fallo del código de Dylan.)

- [ ] **Step 7: Diálogo «¿Criarlo ahora?»** — justo antes de `void onTap(int16_t x, int16_t y) {`:

```cpp
// "Criarlo ahora?". Si: la mascota actual se guarda con toda su vida (o el
// huevo se aparta) y la capturada pasa a criarse. No: va al equipo o la caja.
// En los dos casos, si no hay sitio, el bucle abre la pantalla de elegir a
// quien soltar (pet.endedKind), igual que tras una despedida.
static void raiseCaught(bool now) {
  raiseAsk = false;
  bool canNow = now && pet.ceremony == CER_NONE && !pet.evolving() &&
                (!pet.isEgg() || !pet.hasStoredEgg());
  if (!canNow) {
    pet.endedMon = caughtMon;
    pet.endedLife = caughtLife;
    pet.endedKind = CER_RELEASE;
    return;
  }
  if (pet.isEgg()) {
    pet.stashEgg();
    pet.raiseFrom(caughtMon, caughtLife);
  } else {
    PartyMon om; PartyLife ol;
    pet.bankFull(om, ol);
    pet.raiseFrom(caughtMon, caughtLife);
    pet.endedMon = om;
    pet.endedLife = ol;
    pet.endedKind = CER_RELEASE;
  }
  sfxPlay(SFX_HATCH);
}
```

En `onTap`, justo debajo del bloque `if (battleOpen) { battleTap(x, y); return; }`:

```cpp
  if (raiseAsk) {   // modal: nada de debajo responde
    int c1t, c1b, c2t, c2b;
    uiConfirmRects(&c1t, &c1b, &c2t, &c2b);
    if (x < CONFIRM_BTN_X || x > CONFIRM_BTN_X + CONFIRM_BTN_W) return;
    if (y >= c1t && y <= c1b) raiseCaught(true);
    else if (y >= c2t && y <= c2b) raiseCaught(false);
    return;
  }
```

En `render()`, justo antes de `if (menuOpen) drawMenu();`:

```cpp
  if (raiseAsk) {
    char q[40];
    snprintf(q, sizeof(q), "%s", T(S_RAISE_Q));
    drawConfirmPanel(q, T(S_RAISE_HINT), nullptr, UI_TRACK,
                     T(S_YES), UI_BAR_OK, UI_WHITE, T(S_NO), UI_TRACK, UI_INK);
  }
```

- [ ] **Step 8: El banco del bucle lleva la vida** — en `loop()`, sustituye:

```cpp
    if (party.add(pet.endedMon) || party.boxAdd(pet.endedMon)) {
```

por:

```cpp
    if (party.add(pet.endedMon, pet.endedLife) || party.boxAdd(pet.endedMon, pet.endedLife)) {
```

y en `partyTap`, sustituye `party.replaceAt(i, pet.endedMon);` por `party.replaceAt(i, pet.endedMon, pet.endedLife);`.

- [ ] **Step 9: Compilar** con el comando de referencia. Expected: compila. Si `MUS_VICTORY` o `SFX_VICTORY` no existen con ese nombre, usa los que ya usa `btlResolve`.

- [ ] **Step 10: Tests**

Run: `bash tools/emu/tests/run.sh`
Expected: `failed: 0`. `flush_test` debe seguir pasando: todas las ramas nuevas dibujan dentro de pantallas que ya hacen `gfx->flush()`.

- [ ] **Step 11: Commit (Tasks 10 y 11)**

```bash
git add TamaPoke.ino
git commit -m "Combate salvaje: BALL con anillo de timing, captura y criarlo ahora"
```

---

### Task 12: Fila EXPLORAR, pantalla Explorar y tienda

**Files:**
- Modify: `TamaPoke.ino`

- [ ] **Step 1: Seis filas en el menú** — sustituye las definiciones:

```cpp
#define MENU_Y 75
#define MENU_H 316
#define MENU_ROW_H 52
#define MENU_ROW_GAP 6
```

por:

```cpp
#define MENU_Y 72
#define MENU_H 322
#define MENU_ROW_H 44
#define MENU_ROW_GAP 5
```

y `#define MENU_ROWS 5` por `#define MENU_ROWS 6`. Actualiza el comentario de encima (`// 5 rows: STATS / POKEDEX / SETTINGS / RETIRE / CLOSE...`) a `// 6 rows: STATS / POKEDEX / SETTINGS / RETIRE / EXPLORE / CLOSE`.

- [ ] **Step 2: Etiqueta** — en `menuRowLabel`, añade antes de `default:`:

```cpp
    case 4: snprintf(out, n, "%s", T(S_EXPLORE)); break;
```

- [ ] **Step 3: Toque** — en el bucle de filas del menú dentro de `onTap`, debajo de la rama `else if (i == 3) { ... }`:

```cpp
      else if (i == 4) { exploreOpen = true; }
```

y cambia el comentario `// i == 4 is CLOSE: just shut` por `// i == 5 is CLOSE: just shut`.

- [ ] **Step 4: Pantalla** — justo antes de `static void menuRowLabel(`:

```cpp
// ---------- EXPLORAR (fila del menu) ----------
#define EXP_BTN_X 103
#define EXP_BTN_W 260
#define EXP_SEARCH_Y 146
#define EXP_SEARCH_H 56
#define EXP_BUY1_Y 250
#define EXP_BUY10_Y 306
#define EXP_BUY_H 48

void renderExplore() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - (int)strlen(T(S_EXPLORE)) * 9, 44);
  gfx->print(T(S_EXPLORE));

  char l1[24], l2[24];
  snprintf(l1, sizeof(l1), T(S_BALLS_FMT), gWild.balls);
  snprintf(l2, sizeof(l2), T(S_COINS_FMT), gWild.coins);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(l1) * 6, 90);
  gfx->print(l1);
  gfx->setCursor(CX - (int)strlen(l2) * 6, 114);
  gfx->print(l2);

  bool searchOk = gWild.explores < WILD_EXPLORES_PER_DAY && wildCanStart();
  char s[24];
  snprintf(s, sizeof(s), T(S_SEARCH_FMT), WILD_EXPLORES_PER_DAY - gWild.explores, WILD_EXPLORES_PER_DAY);
  gfx->fillRoundRect(EXP_BTN_X, EXP_SEARCH_Y, EXP_BTN_W, EXP_SEARCH_H, 12, searchOk ? UI_BAR_OK : UI_TRACK);
  gfx->drawRoundRect(EXP_BTN_X, EXP_SEARCH_Y, EXP_BTN_W, EXP_SEARCH_H, 12, UI_INK);
  gfx->setTextColor(searchOk ? UI_WHITE : 0x8410);
  gfx->setCursor(CX - (int)strlen(s) * 6, EXP_SEARCH_Y + EXP_SEARCH_H / 2 - 8);
  gfx->print(s);

  const char *why = gWild.explores >= WILD_EXPLORES_PER_DAY ? T(S_NO_SEARCH)
                  : !gWild.balls ? T(S_NO_BALLS) : nullptr;
  if (why) {
    gfx->setTextColor(UI_BAR_WARN);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(why) * 3, EXP_SEARCH_Y + EXP_SEARCH_H + 14);
    gfx->print(why);
  }

  const char *buy[2] = { T(S_BUY1), T(S_BUY10) };
  const int by[2] = { EXP_BUY1_Y, EXP_BUY10_Y };
  for (int i = 0; i < 2; i++) {
    bool ok = wildCanBuy(gWild, i ? 10 : 1);
    gfx->fillRoundRect(EXP_BTN_X, by[i], EXP_BTN_W, EXP_BUY_H, 12, ok ? UI_BG_DAY : UI_TRACK);
    gfx->drawRoundRect(EXP_BTN_X, by[i], EXP_BTN_W, EXP_BUY_H, 12, UI_INK);
    gfx->setTextColor(ok ? UI_INK : 0x8410);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(buy[i]) * 6, by[i] + EXP_BUY_H / 2 - 8);
    gfx->print(buy[i]);
  }
  gfx->setTextColor(UI_TRACK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(T(S_BACK)) * 6, 404);
  gfx->print(T(S_BACK));
  gfx->flush();
}

static bool expHit(int16_t x, int16_t y, int top, int h) {
  return x >= EXP_BTN_X && x <= EXP_BTN_X + EXP_BTN_W && y >= top && y <= top + h;
}

void exploreTap(int16_t x, int16_t y) {
  if (expHit(x, y, EXP_SEARCH_Y, EXP_SEARCH_H)) {
    WildMon w;
    if (gWild.explores >= WILD_EXPLORES_PER_DAY || !wildCanStart() || !wildRoll(pet, w)) {
      sfxPlay(SFX_DENY);
      return;
    }
    wildUseExplore(gWild);
    wildSave(gWild);
    exploreOpen = false;
    startWildBattle(w);
    return;
  }
  for (int i = 0; i < 2; i++) {
    if (!expHit(x, y, i ? EXP_BUY10_Y : EXP_BUY1_Y, EXP_BUY_H)) continue;
    if (!wildBuy(gWild, i ? 10 : 1)) { sfxPlay(SFX_DENY); return; }
    wildSave(gWild);
    sfxPlay(SFX_MEDAL);
    return;
  }
  if (y > 380) { exploreOpen = false; sfxPlay(SFX_TAP); }   // volver
}
```

- [ ] **Step 5: Engancharla**

`render()`: justo antes de `if (battleOpen) {`:

```cpp
  if (exploreOpen) {
    renderExplore();
    return;
  }
```

`onTap`: justo debajo del bloque `if (raiseAsk) { ... }` (Task 11):

```cpp
  if (exploreOpen) {
    exploreTap(x, y);
    return;
  }
```

`onSwipe(int dir)`: justo antes de su `if (menuOpen) { menuOpen = false; return; }` (~línea 1642; la de ~1284 es otro gesto, añádelo también ahí):

```cpp
  if (exploreOpen) { exploreOpen = false; return; }   // cualquier deslizamiento sale
```

Pantalla para los tests: en el `enum` de pantallas, añade `SCR_EXPLORE,` justo antes de `SCR_GAME,`; en `SCREEN_NAME` añade `"explore",` justo antes de `"minigame",`; y en `uiCurrentScreen()`, justo antes de la línea que devuelve `SCR_BATTLE`, añade `if (exploreOpen) return SCR_EXPLORE;`.

- [ ] **Step 6: Compilar y tests**

Compilar el firmware. Expected: compila.

Run: `bash tools/emu/tests/run.sh`
Expected: `failed: 0`. Si `touch_test` o `hit_test` tocan filas del menú con coordenadas fijas, actualízalas a las nuevas `MENU_ROW_Y(i)`. Añade en `flush_test.cpp`, junto a `menuOpen=true; check("menu");`, la línea `clearAll(); exploreOpen=true; check("explore");` y declara `extern bool exploreOpen;` arriba.

- [ ] **Step 7: Commit**

```bash
git add TamaPoke.ino tools/emu/tests
git commit -m "Menu: fila EXPLORAR con busquedas diarias y tienda de Pokeballs"
```

---

### Task 13: Aviso espontáneo «¡!»

**Files:**
- Modify: `TamaPoke.ino`

- [ ] **Step 1: Dibujo** — en `render()`, justo antes del bloque `if (raiseAsk) {` (Task 11):

```cpp
  // aviso espontaneo de encuentro: tocarlo abre el combate
  if (wildBubbleUntil && uiCurrentScreen() == SCR_MAIN && !menuOpen) {
    gfx->fillCircle(WILD_BUB_X, WILD_BUB_Y, 26, UI_BAR_WARN);
    gfx->drawCircle(WILD_BUB_X, WILD_BUB_Y, 26, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(3);
    gfx->setCursor(WILD_BUB_X - 8, WILD_BUB_Y - 11);
    gfx->print("!");
  }
```

(`WILD_BUB_X`/`WILD_BUB_Y` ya están definidos junto al estado global, Task 10.) Si en la placa la burbuja tapa el botón de evolución o despedida, muévela a `WILD_BUB_X (CX - 90)`.

- [ ] **Step 2: Toque** — en `onTap`, justo debajo del bloque `if (pet.isEgg()) { ... return; }`:

```cpp
  if (wildBubbleUntil) {
    int dx = x - WILD_BUB_X, dy = y - WILD_BUB_Y;
    if (dx * dx + dy * dy <= 44 * 44) {
      wildBubbleUntil = 0;
      WildMon w;
      if (!wildCanStart() || !wildRoll(pet, w)) { sfxPlay(SFX_DENY); return; }
      gWild.ambient++;
      wildSave(gWild);
      startWildBattle(w);
      return;
    }
  }
```

- [ ] **Step 3: Compilar y tests.** Expected: compila y `failed: 0`.

- [ ] **Step 4: Commit**

```bash
git add TamaPoke.ino
git commit -m "Aviso espontaneo de encuentro salvaje"
```

---

### Task 14: CRIAR en la ficha del equipo y píldora HUEVO

**Files:**
- Modify: `TamaPoke.ino`

- [ ] **Step 1: Regla común** — justo antes de `void renderMonSheet(`:

```cpp
// Se puede poner a criar a otro ahora? No en mitad de una ceremonia o una
// evolucion, ni eligiendo inicial, ni con un huevo esperando si ya hay otro
// apartado (solo cabe uno).
static bool canRaiseNow() {
  return pet.ceremony == CER_NONE && !pet.evolving() && !pet.awaitingStarter() &&
         !(pet.isEgg() && pet.hasStoredEgg());
}
```

- [ ] **Step 2: Botón en `renderMonSheet`** — sustituye:

```cpp
  bool leftOk = fromBox ? (party.firstFree() >= 0)
                        : (pet.isEgg() && !pet.awaitingStarter());
  const char *leftLbl = fromBox ? T(S_BOX_TAKE) : T(S_REVIVE);
```

por:

```cpp
  bool leftOk = fromBox ? (party.firstFree() >= 0) : canRaiseNow();
  const char *leftLbl = fromBox ? T(S_BOX_TAKE) : T(S_RAISE);
```

y `const char *why = fromBox ? T(S_PARTY_FULL) : T(S_REVIVE_EGG);` por `const char *why = fromBox ? T(S_PARTY_FULL) : T(S_RAISE_NO);`. Actualiza el comentario de encima de `leftOk`: CRIAR intercambia con la mascota actual, no la destruye.

- [ ] **Step 3: Toque en `partyTap`** — sustituye el bloque `if (monSheetBtn(x, y, true)) {           // BRING BACK ... }` entero por:

```cpp
    if (monSheetBtn(x, y, true)) {           // CRIAR: intercambio en el mismo hueco
      if (!canRaiseNow()) { sfxPlay(SFX_DENY); return; }
      uint8_t i = partyDetail - 1;
      PartyMon m = party.slots[i];
      PartyLife l = party.life[i];
      // primero el equipo, luego la mascota: un corte entre medias duplica, no pierde
      if (pet.isEgg()) {
        pet.stashEgg();
        party.releaseAt(i);
      } else {
        PartyMon om; PartyLife ol;
        pet.bankFull(om, ol);
        party.replaceAt(i, om, ol);
      }
      pet.raiseFrom(m, l);
      partyDetail = 0;
      boxSwapFrom = 0;
      partyOpen = false;
      sfxPlay(SFX_HATCH);
      return;
    }
```

- [ ] **Step 4: Píldora HUEVO en `renderParty`** — constantes junto a `PARTYCLOSE_*`:

```cpp
#define EGGPILL_X 163
#define EGGPILL_Y 34
#define EGGPILL_W 140
#define EGGPILL_H 44
```

En `renderParty`, sustituye el bloque del título (desde `char head[24];` hasta `gfx->print(head);`) por:

```cpp
  bool eggPill = pet.hasStoredEgg() && !partyPick && !boxSel;
  if (eggPill) {
    // el huevo apartado ocupa el sitio del titulo: tocarlo lo pone a criar
    gfx->fillRoundRect(EGGPILL_X, EGGPILL_Y, EGGPILL_W, EGGPILL_H, 12,
                       canRaiseNow() && !pet.isEgg() ? UI_BAR_OK : UI_TRACK);
    gfx->drawRoundRect(EGGPILL_X, EGGPILL_Y, EGGPILL_W, EGGPILL_H, 12, UI_INK);
    gfx->setTextColor(UI_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(T(S_EGG_SLOT)) * 6, EGGPILL_Y + EGGPILL_H / 2 - 8);
    gfx->print(T(S_EGG_SLOT));
  } else {
    char head[24];
    snprintf(head, sizeof(head), T(S_PARTY_FMT), party.count());
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(3);
    gfx->setCursor(CX - (int)strlen(head) * 9, 42);
    gfx->print(head);
  }
```

Y justo antes del `gfx->flush();` final de `renderParty`:

```cpp
  if (eggConfirm) {
    drawConfirmPanel(T(S_RAISE_Q), T(S_RAISE_HINT), nullptr, UI_TRACK,
                     T(S_YES), UI_BAR_OK, UI_WHITE, T(S_NO), UI_TRACK, UI_INK);
  }
```

- [ ] **Step 5: Toques del huevo en `partyTap`** — justo antes de `void partyTap(int16_t x, int16_t y) {`, añade:

```cpp
// Poner a criar el huevo apartado: la mascota actual va al equipo o la caja
// por el mismo camino que una despedida (pet.endedKind en el bucle).
static void raiseStoredEgg() {
  if (!canRaiseNow() || pet.isEgg() || !pet.hasStoredEgg()) { sfxPlay(SFX_DENY); return; }
  PartyMon om; PartyLife ol;
  pet.bankFull(om, ol);
  if (!pet.restoreEgg()) return;
  pet.endedMon = om;
  pet.endedLife = ol;
  pet.endedKind = CER_RELEASE;
  partyOpen = false;
  sfxPlay(SFX_HATCH);
}
```

Y dentro de `partyTap`, justo debajo de `if (boxOpen) { boxTap(x, y); return; }`:

```cpp
  if (eggConfirm) {   // modal, como el de soltar
    int c1t, c1b, c2t, c2b;
    uiConfirmRects(&c1t, &c1b, &c2t, &c2b);
    if (x < CONFIRM_BTN_X || x > CONFIRM_BTN_X + CONFIRM_BTN_W) return;
    if (y >= c1t && y <= c1b) { eggConfirm = false; raiseStoredEgg(); }
    else if (y >= c2t && y <= c2b) { eggConfirm = false; sfxPlay(SFX_TAP); }
    return;
  }
  if (!partyDetail && pet.hasStoredEgg() && !partyPick && !boxSel &&
      x >= EGGPILL_X && x <= EGGPILL_X + EGGPILL_W &&
      y >= EGGPILL_Y && y <= EGGPILL_Y + EGGPILL_H) {
    if (!canRaiseNow() || pet.isEgg()) { sfxPlay(SFX_DENY); return; }
    eggConfirm = true;
    sfxPlay(SFX_TAP);
    return;
  }
```

- [ ] **Step 6: Compilar y tests**

Compilar. Expected: compila.

Run: `bash tools/emu/tests/run.sh`
Expected: `failed: 0`. Si `revive_test` comprueba que BRING BACK solo funciona con un huevo, actualiza esa comprobación al comportamiento nuevo (CRIAR funciona siempre e intercambia) y deja un comentario explicando el cambio.

- [ ] **Step 7: Commit**

```bash
git add TamaPoke.ino tools/emu/tests
git commit -m "Equipo: CRIAR intercambia con la mascota actual; huevo apartado"
```

---

### Task 15: Grabar y probar en la placa

**Files:**
- Modify: `CLAUDE.md` (del worktree)

- [ ] **Step 1: Compilar y grabar** (placa por USB). Detecta el puerto en PowerShell con `Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match 'VID_303A&PID_1001&MI_00' }` (en esta placa ha sido COM5) y graba:

```bash
python -m esptool --port COM5 --baud 921600 write-flash 0x10000 build/TamaPoke.ino.bin
```

Expected: `Hash of data verified.`

- [ ] **Step 2: Consola** (abre el puerto con DTR/RTS a False o la placa se reinicia). Prueba con un script de pyserial como los de la sesión:

| Comando | Esperado |
|---|---|
| `WILDINFO` | `coins=50 balls=5 explores=0/5 ambient=0/3 ... egg=0` en el primer arranque |
| `WILD` | `wild <dex> lv<n> ... -> combate` y la pantalla de combate con LUCHA / BALL x5 |
| `BALLS 0` y `WILD` | BALL sale apagado; tocarlo avisa «sin Pokeballs» |
| `COINS 200` | La tienda permite +10 |

- [ ] **Step 3: Recorrido en pantalla** (anota el resultado de cada uno):
  1. Menú → EXPLORAR: se ven bolas, monedas, BUSCAR 5/5 y la tienda. Comprar +1 resta 10 monedas.
  2. BUSCAR abre un combate salvaje; BUSCAR baja a 4/5.
  3. BALL: el anillo se encoge; tocar a tiempo muestra PERFECTO o BIEN; la bola se tambalea.
  4. Debilitar al salvaje con LUCHA: «Has ganado!» y «+5 monedas».
  5. Capturar (`WILD 16 3` contra una Pidgey débil ayuda): «Atrapaste a X!» y, al tocar, «Criarlo ahora?».
  6. «No»: aparece en el equipo. Ficha → CRIAR: intercambio; la mascota anterior queda en ese hueco con su nivel y su vínculo.
  7. Con un huevo esperando, CRIAR desde el equipo: el huevo se aparta y sale la píldora HUEVO; tocarla y confirmar lo devuelve.
  8. Esperar con la pantalla principal encendida: en algún momento sale «!» (o fuerza la prueba bajando temporalmente `random(60)` a `random(2)`, sin commitear).
  9. Reiniciar la placa: monedas, bolas, equipo y huevo apartado siguen ahí.
  10. Suspensión (PWR 2,5 s) en pleno combate salvaje: al despertar no hay combate y la bola gastada no vuelve.

- [ ] **Step 4: Documentar** — añade al `CLAUDE.md` del worktree una sección:

```markdown
## Encuentros salvajes y captura (rama dylan-1.75c)
- Lógica en `wild.cpp` (sin pantalla, tests en `tools/emu/tests/wild_test.cpp`);
  criar a demanda en `Pet::bankFull/raiseFrom` y `PartyLife` (claves `plife`/`blife`);
  huevo apartado en `egg2`. Spec y plan en `docs/superpowers/`.
- Consola: `WILD` (encuentro al azar), `WILD <dex> [nivel]`, `COINS <n>`,
  `BALLS <n>`, `WILDINFO`.
```

- [ ] **Step 5: Commit**

```bash
git add CLAUDE.md
git commit -m "CLAUDE.md: encuentros salvajes y comandos de prueba"
```

---

### Task 16: Alinear el spec con lo implementado

**Files:**
- Modify: `docs/superpowers/specs/2026-10-28-encuentros-salvajes-captura-design.md`

Durante la planificación se cambiaron tres detalles respecto al spec aprobado; el spec debe reflejarlos.

- [ ] **Step 1: Sección 7 (Datos y migración)** — sustituye el texto de «`PartyMon` ampliado» y «Claves NVS nuevas» por: los datos de vida van en una tabla paralela `PartyLife` dentro de `Party` (`life[]`, `boxLife[]`), con claves nuevas `plife`/`blife`. `party`/`box` no cambian de formato; un guardado antiguo no tiene esas claves y sus huecos vuelven con la edad de su nivel. Así no hay migración ni riesgo con `MAX_VAL` (`blife` ocupa 360 bytes). Elimina el párrafo de `box2a`/`box2b`.

- [ ] **Step 2: Sección 6 (Criar)** — CRIAR está en la ficha del **equipo**. Desde la caja se usa primero AL EQUIPO (la ficha de la caja ya tiene dos botones).

- [ ] **Step 3: Sección 4 y 9** — con equipo y caja llenos se usa la pantalla existente de elegir hueco; rechazarla suelta al que no tiene sitio, igual que tras una despedida (sin confirmación extra). Elimina el caso «Especie salvaje sin sprite en la FAT»: el sorteo ya filtra por `speciesHasArt` y por los packs de región presentes (`regionAvailable`).

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/specs/2026-10-28-encuentros-salvajes-captura-design.md
git commit -m "Spec: alinear con el plan (PartyLife, CRIAR desde el equipo)"
```
