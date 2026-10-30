# Biomas en EXPLORAR — plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Al explorar se elige uno de 6 biomas con flechas y solo aparecen especies de ese hábitat, de las 809, sin regiones ni dificultad.

**Architecture:** `wildRoll` recibe el bioma y filtra los candidatos por `DEX_TBL[d].biome` sobre `REGIONS[REGION_ALL]`. `WildState` guarda el bioma elegido (clave NVS `wbio`). La pantalla Explorar gana una fila «< BIOMA >» y los avisos «¡!» usan el último bioma.

**Tech Stack:** Arduino C++ (ESP32-S3), emulador de Dylan (`tools/emu`), arduino-cli, esptool.

**Spec:** `docs/superpowers/specs/2026-10-30-biomas-explorar-design.md`

---

## Antes de empezar

- Worktree `C:\Users\Tarrito\Documents\TamaPoke-dylan\TamaPoke`, rama `dylan-1.75c`. Respetar finales de línea (`.ino` LF, `wild.*`/tests CRLF).
- Tests (desde Git Bash, siempre con `</dev/null`):

```bash
export MSYSTEM=UCRT64 CHERE_INVOKING=1
/c/msys64/usr/bin/bash.exe -lc "cd /c/Users/Tarrito/Documents/TamaPoke-dylan/TamaPoke && bash tools/emu/tests/run.sh wild" </dev/null
```

  Línea base: `suites passed: 39, failed: 0` sin filtro.
- Firmware (un solo arduino-cli a la vez):

```bash
"/c/Program Files/Arduino CLI/arduino-cli.exe" compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=32M,PSRAM=opi,PartitionScheme=custom" --output-dir build .
```

- Commits terminan con una línea en blanco y `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

Valores de `biome` de Dylan: 0 pradera, 1 playa (AGUA), 2 bosque, 3 volcán, 4 montaña (CUEVA), 5 nieve. Orden en pantalla: PRADERA, BOSQUE, AGUA, CUEVA, VOLCÁN, NIEVE.

---

### Task 1: `wild` — sorteo por bioma, orden de biomas y `wbio`

**Files:**
- Modify: `wild.h`, `wild.cpp`, `save.cpp`
- Test: `tools/emu/tests/wild_test.cpp`

- [ ] **Step 1: Helper de línea en el test** — en `wild_test.cpp`, justo antes de `static void testRoll()`:

```cpp
// La especie o alguna de sus formas anteriores es de ese bioma: un salvaje puede
// salir evolucionado y la forma evolucionada tener otro bioma (misma linea).
static bool lineHasBiome(int16_t d, uint8_t b) {
  for (int guard = 0; guard < 4 && d >= 1; guard++) {
    if (DEX_TBL[d].biome == b) return true;
    int16_t pre = 0;
    for (int16_t i = 1; i <= DEX_COUNT && !pre; i++)
      if (DEX_TBL[i].evolvesTo == d) pre = i;
    if (!pre)
      for (int k = 0; k < EEVEE_EVO_COUNT && !pre; k++)
        if (EEVEE_EVOS[k] == d) pre = DEX_EEVEE;    // ramas de Eevee: no estan en evolvesTo
    d = pre;
  }
  return false;
}
```

- [ ] **Step 2: Adaptar `testRoll` a biomas (test que falla)**

Sustituye en `testRoll` cada `wildRoll(p, m)` por `wildRoll(p, m, 0)` (pradera) y cada `wildRoll(e, m)` por `wildRoll(e, m, DEX_TBL[DEX_EEVEE].biome)`. Quita las comprobaciones de región de Kanto: la variable `regOk`, su comentario, su línea `if (m.dex < rg.lo || m.dex > rg.hi) regOk = false;`, su `ck(regOk, ...)` y `const RegionInfo &rg = REGIONS[0];`. Añade en el bucle principal:

```cpp
    if (!lineHasBiome(m.dex, 0)) biomeOk = false;
    regionsSeen |= (uint16_t)1 << regionOfDex(m.dex);
```

declarando `bool biomeOk = true; uint16_t regionsSeen = 0;` junto a las demás variables, y tras el bucle:

```cpp
  ck(biomeOk, "pradera: todo sale de la linea de una especie de pradera");
  int nReg = 0;
  for (uint8_t r = 0; r < REGION_COUNT; r++) if (r != REGION_ALL && (regionsSeen >> r & 1)) nReg++;
  ck(nReg >= 3, "explorar no mira la region: salen especies de varias regiones");
```

(`p.region = 0` puede quedarse: así se demuestra que se ignora.)

Sustituye el bloque de Unova (desde `// el filtro de arte debe poder fallar` hasta el cierre de su `else`) por:

```cpp
  // cada bioma: solo su linea, nunca sin arte; nieve es el mas pobre
  for (uint8_t b = 0; b < WILD_BIOMES; b++) {
    bool ok = true, art = true;
    int got = 0;
    for (int i = 0; i < 600; i++) {
      WildMon m;
      if (!wildRoll(p, m, b)) continue;
      got++;
      if (!lineHasBiome(m.dex, b)) ok = false;
      if (!speciesHasArt(m.dex)) art = false;
    }
    char msg[64];
    snprintf(msg, sizeof(msg), "bioma %u: %d salvajes, todos de su linea y con arte", b, got);
    ck(got > 0 && ok && art, msg);
  }
  WildMon bad;
  ck(wildRoll(p, bad, 99) && lineHasBiome(bad.dex, 0), "bioma fuera de rango = pradera");
```

En el bloque de legendarios (el bucle de 4000 tiradas tras `registerCaught`), cambia `wildRoll(p, m)` por `wildRoll(p, m, 5)` (nieve) y añade dentro del `if` del legendario:

```cpp
    if (DEX_TBL[m.dex].biome != 5) legLv = false;
```

y cambia el texto del `ck(legends > 10 && legends < 100, ...)` por `ck(legends > 0 && legends < 200, "nieve con 25+ registrados: legendarios solo de nieve")` (en nieve hay pocos candidatos, la proporción cambia).

- [ ] **Step 3: Tests del orden y de `wbio`** — antes de `int main()`:

```cpp
static void testBiomes() {
  uint8_t b = 0;
  const uint8_t order[WILD_BIOMES] = { 0, 2, 1, 4, 3, 5 };
  bool fwd = true;
  for (int i = 0; i < WILD_BIOMES; i++) { if (b != order[i]) fwd = false; b = wildBiomeStep(b, +1); }
  ck(fwd && b == 0, "flecha derecha: PRADERA BOSQUE AGUA CUEVA VOLCAN NIEVE y vuelve");
  ck(wildBiomeStep(0, -1) == 5 && wildBiomeStep(5, +1) == 0, "flechas en circulo");
  ck(wildBiomeStep(99, +1) == 2, "bioma invalido cuenta como pradera");
}
```

En `testPersist`, en el bloque que guarda y recarga `s`, añade `s.biome = 4;` antes de `wildSave(s);` y `&& r.biome == 4` a la comprobación de recarga; tras el primer `wildLoad(first, p)` añade:

```cpp
  ck(first.biome == 0, "primer arranque: bioma pradera");
```

Y en `main()`, debajo de `testRoll();`: `testBiomes();`

- [ ] **Step 4: Ver que falla**

Run: `run.sh wild` → Expected: `DID NOT COMPILE` (`wildRoll` con 3 argumentos, `WILD_BIOMES`, `wildBiomeStep`, `WildState::biome` no existen).

- [ ] **Step 5: `wild.h`**

Debajo de `#define WILD_LEGEND_MIN_REG 25 ...`:

```cpp
#define WILD_BIOMES 6               // los de Dylan: DEX_TBL[d].biome (0..5)
```

En `struct WildState`, debajo de `uint16_t paidMedals = 0; ...`:

```cpp
  uint8_t biome = 0;          // bioma elegido en EXPLORAR (valor de DEX_TBL.biome)
```

Sustituye el comentario y la declaración de `wildRoll` por:

```cpp
// Sortea el salvaje de un BIOMA (DEX_TBL[d].biome; fuera de rango = pradera),
// de las 809 especies sin mirar la region. Legendario 1% con 25+ registrados
// (solo los del bioma; nivel +10, minimo 40); si no, rareza como los huevos
// (comun / raro con el bonus de cuidado) prefiriendo lineas incompletas. Nivel por
// especie: desde el nivel de evolucion de su forma anterior hasta el suyo - 1 (nunca
// sale listo para evolucionar), con techo en la mascota + 5. Sale evolucionado (30%)
// solo si la mascota ya llego a ese nivel; la forma evolucionada puede ser de otro
// bioma (misma linea). Shiny 1/48 mejorado por el cuidado.
// false si la mascota es un huevo o no hay nada que sacar.
bool wildRoll(Pet &p, WildMon &out, uint8_t biome);
// Siguiente bioma en el orden de la pantalla (PRADERA BOSQUE AGUA CUEVA VOLCAN
// NIEVE), en circulo; dir +1 o -1. Un valor invalido cuenta como pradera.
uint8_t wildBiomeStep(uint8_t biome, int dir);
```

- [ ] **Step 6: `wild.cpp`**

Borra la función `wildRegion` y su comentario (deja de usarse). En `wildRoll`, cambia la firma a `bool wildRoll(Pet &p, WildMon &m, uint8_t biome) {` y sustituye

```cpp
  const RegionInfo &rg = REGIONS[wildRegion(p.region % REGION_COUNT)];
```

por

```cpp
  if (biome >= WILD_BIOMES) biome = 0;
  const RegionInfo &rg = REGIONS[REGION_ALL];   // explorar no mira la region
```

y dentro del bucle de candidatos, justo después de `if (DEX_TBL[d].rarity != t) continue;`, añade:

```cpp
        if (DEX_TBL[d].biome != biome) continue;
```

Al final del fichero:

```cpp
static const uint8_t WILD_BIOME_ORDER[WILD_BIOMES] = { 0, 2, 1, 4, 3, 5 };

uint8_t wildBiomeStep(uint8_t biome, int dir) {
  int at = 0;
  for (int i = 0; i < WILD_BIOMES; i++) if (WILD_BIOME_ORDER[i] == biome) at = i;
  at = (at + (dir < 0 ? WILD_BIOMES - 1 : 1)) % WILD_BIOMES;
  return WILD_BIOME_ORDER[at];
}
```

En `wildLoad`, dentro de `if (init) {`, debajo de la línea de `wmed`:

```cpp
    w.biome = pr.getUChar("wbio", 0);
```

y tras el bloque `if (init) {...}` (junto a las otras acotaciones): `if (w.biome >= WILD_BIOMES) w.biome = 0;`. En `wildSave`, debajo de la línea de `wmed`: `pr.putUChar("wbio", w.biome);`.

Comprueba que `WILD_CAND_MAX` (260) cubre el bioma más grande sobre las 809: pradera tiene 141 formas base comunes+raras en total, así que cabe.

- [ ] **Step 7: `save.cpp`** — sustituye `{ "wcare", SK_U32 },  { "wmed", SK_U16 },` por:

```cpp
  { "wcare", SK_U32 },  { "wmed", SK_U16 },   { "wbio", SK_U8 },
```

- [ ] **Step 8: Llamadas existentes** — en `TamaPoke.ino` cambia las tres llamadas `wildRoll(pet, w)` por `wildRoll(pet, w, gWild.biome)` (comando `WILD`, toque de la burbuja «¡!» y BUSCAR en `exploreTap`).

- [ ] **Step 9: Verificar**

Run: `run.sh wild` → todo PASS. Run: `run.sh` (completo) → `failed: 0`. Compilar el firmware → compila.

- [ ] **Step 10: Commit**

```bash
git add wild.h wild.cpp save.cpp TamaPoke.ino tools/emu/tests/wild_test.cpp
git commit -m "wild: sorteo por bioma de las 809 especies, sin mirar la region"
```

---

### Task 2: Textos de los biomas

**Files:** Modify `i18n.h`, `i18n.cpp`

- [ ] **Step 1: Ids** — en `i18n.h`, justo después de `S_WILD_COINS_FMT, // "+%u monedas"`:

```cpp
  // biomas de EXPLORAR, en el orden de DEX_TBL.biome (0..5)
  S_BIOME_GRASS, S_BIOME_WATER, S_BIOME_FOREST, S_BIOME_VOLCANO, S_BIOME_CAVE, S_BIOME_SNOW,
```

- [ ] **Step 2: Cadenas** — en cada fila de `STRINGS` de `i18n.cpp`, justo después de la cadena de `S_WILD_COINS_FMT` de esa fila (`"+%u monedas"`, `"+%u coins"`, …) y antes del `},`, en este orden:

| Fila | Cadenas |
|---|---|
| ES | `"PRADERA", "AGUA", "BOSQUE", "VOLCAN", "CUEVA", "NIEVE",` |
| EN | `"MEADOW", "WATER", "FOREST", "VOLCANO", "CAVE", "SNOW",` |
| FR | `"PRAIRIE", "EAU", "FORET", "VOLCAN", "GROTTE", "NEIGE",` |
| DE | `"WIESE", "WASSER", "WALD", "VULKAN", "HOEHLE", "SCHNEE",` |
| IT | `"PRATO", "ACQUA", "BOSCO", "VULCANO", "GROTTA", "NEVE",` |
| PT | `"PRADO", "AGUA", "FLORESTA", "VULCAO", "CAVERNA", "NEVE",` |

El más largo es FLORESTA (8 letras, 96 px a tamaño 2): cabe entre las flechas.

- [ ] **Step 3: Verificar** — `run.sh i18n` pasa; el firmware compila.

- [ ] **Step 4: Commit**

```bash
git add i18n.h i18n.cpp
git commit -m "i18n: nombres de los biomas de EXPLORAR"
```

---

### Task 3: Fila de biomas en Explorar, `WILDINFO` y tests de pantalla

**Files:**
- Modify: `TamaPoke.ino`
- Test: `tools/emu/tests/explore_test.cpp`

- [ ] **Step 1: Geometría** — sustituye las definiciones `EXP_*` por:

```cpp
#define EXP_BTN_X 103
#define EXP_BTN_W 260
#define EXP_BIOME_Y 136      // fila "<  BOSQUE  >"
#define EXP_BIOME_H 44
#define EXP_ARROW_W 60       // zona tactil de cada flecha, en los extremos de la fila
#define EXP_SEARCH_Y 188
#define EXP_SEARCH_H 52
#define EXP_BUY1_Y 270
#define EXP_BUY10_Y 324
#define EXP_BUY_H 48
```

(Bisel: a y=372, el borde inferior de +10, la mitad de cuerda es ~187 px, más que los 130 de los botones. El aviso de "sin bolas/sin búsquedas" queda en `EXP_SEARCH_Y + EXP_SEARCH_H + 14` = 254, entre BUSCAR y +1.)

Justo debajo de esas definiciones:

```cpp
// nombre del bioma, indexado por el valor de DEX_TBL.biome (0..5)
static const StrId BIOME_NAME[WILD_BIOMES] = {
  S_BIOME_GRASS, S_BIOME_WATER, S_BIOME_FOREST, S_BIOME_VOLCANO, S_BIOME_CAVE, S_BIOME_SNOW
};
```

- [ ] **Step 2: Dibujar la fila** — en `renderExplore`, justo antes de `bool searchOk = ...`:

```cpp
  // bioma: "<  BOSQUE  >"; las flechas lo cambian (exploreTap)
  gfx->fillRoundRect(EXP_BTN_X, EXP_BIOME_Y, EXP_BTN_W, EXP_BIOME_H, 12, UI_BG_DAY);
  gfx->drawRoundRect(EXP_BTN_X, EXP_BIOME_Y, EXP_BTN_W, EXP_BIOME_H, 12, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(EXP_BTN_X + 16, EXP_BIOME_Y + EXP_BIOME_H / 2 - 8);
  gfx->print("<");
  gfx->setCursor(EXP_BTN_X + EXP_BTN_W - 28, EXP_BIOME_Y + EXP_BIOME_H / 2 - 8);
  gfx->print(">");
  const char *bn = T(BIOME_NAME[gWild.biome < WILD_BIOMES ? gWild.biome : 0]);
  gfx->setCursor(CX - (int)strlen(bn) * 6, EXP_BIOME_Y + EXP_BIOME_H / 2 - 8);
  gfx->print(bn);
```

- [ ] **Step 3: Tocar las flechas** — en `exploreTap`, al principio:

```cpp
  if (y >= EXP_BIOME_Y && y <= EXP_BIOME_Y + EXP_BIOME_H) {   // flechas del bioma
    if (x >= EXP_BTN_X && x <= EXP_BTN_X + EXP_ARROW_W) {
      gWild.biome = wildBiomeStep(gWild.biome, -1);
    } else if (x >= EXP_BTN_X + EXP_BTN_W - EXP_ARROW_W && x <= EXP_BTN_X + EXP_BTN_W) {
      gWild.biome = wildBiomeStep(gWild.biome, +1);
    } else {
      return;                  // el nombre no hace nada
    }
    wildDirty = true;          // se guarda al salir de EXPLORAR, como las compras
    sfxPlay(SFX_TAP);
    return;
  }
```

- [ ] **Step 4: `WILDINFO`** — en el `Serial.printf` de `WILDINFO`, añade ` biome=%u` al formato y `gWild.biome` a los argumentos (antes de `pet.hasStoredEgg()`, en el mismo orden que en el formato).

- [ ] **Step 5: Tests de pantalla** — en `explore_test.cpp`, añade (siguiendo cómo el fichero ya abre Explorar y toca BUSCAR; usa los mismos helpers y la misma forma de coger coordenadas):
  - Con Explorar abierto y `gWild.biome = 0`, tocar la flecha derecha en (343, 158) — x = 103 + 260 − 20, y = 136 + 22 — seis veces recorre 2, 1, 4, 3, 5 y vuelve a 0; la flecha izquierda en (123, 158) desde 0 da 5; tocar el nombre en (233, 158) no cambia nada.
  - Con `gWild.biome = 4` (cueva), BUSCAR (y = `188 + 26`) abre un combate y la especie del rival (`btlFoe.dex`, o `wildMon.dex` si es visible desde el test) pertenece a una línea con algún miembro de bioma 4 (copia el helper `lineHasBiome` de `wild_test.cpp` en este fichero).
  - Un aviso «¡!» con `gWild.biome = 1` (agua) saca un rival de una línea con bioma 1.
  - Actualiza las coordenadas de los toques de BUSCAR y de la tienda que ya existen en `explore_test.cpp` a las nuevas `EXP_SEARCH_Y 188`, `EXP_BUY1_Y 270`, `EXP_BUY10_Y 324` (si el test define sus propias constantes, cámbialas ahí).

- [ ] **Step 6: Verificar** — `run.sh` completo → `failed: 0`; compilar → compila.

- [ ] **Step 7: Commit**

```bash
git add TamaPoke.ino tools/emu/tests/explore_test.cpp
git commit -m "Explorar: fila de biomas con flechas; los avisos usan el ultimo bioma"
```

---

### Task 4: Grabar y probar en la placa

- [ ] **Step 1: Copia de seguridad (obligatoria antes de grabar)**

```bash
cd /c/Users/Tarrito/Documents/TamaPoke-backups
F="flash-1.75c-$(date +%Y%m%d-%H%M).bin"
python -m esptool --port COM5 --baud 921600 read-flash 0 0x2000000 "$F"
sha256sum "$F" > "$F.sha256"
```

- [ ] **Step 2: Grabar solo la app**

```bash
cd /c/Users/Tarrito/Documents/TamaPoke-dylan/TamaPoke
python -m esptool --port COM5 --baud 921600 write-flash 0x10000 build/TamaPoke.ino.bin
```

- [ ] **Step 3: Consola** (DTR/RTS a False): `WILDINFO` muestra `biome=0` la primera vez (o el que hubiera). 

- [ ] **Step 4: En pantalla:** Explorar muestra «<  PRADERA  >»; las flechas recorren los 6; BUSCAR en CUEVA da Geodude, Diglett, Onix…; el fondo del combate coincide con el bioma; tras reiniciar, Explorar recuerda el último bioma.
