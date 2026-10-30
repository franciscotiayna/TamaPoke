#include "wild.h"
#include <Preferences.h>
#include "moves.h"
#include "noart.h"   // speciesHasArt(): como pet.cpp
#include "catchrate.h"

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

uint8_t wildCatchRate(int16_t dex) {
  return (dex >= 1 && dex <= DEX_COUNT) ? CATCH_RATE[dex] : 45;
}

uint8_t wildCatchValue(uint8_t rate, uint16_t hp, uint16_t maxHp, uint8_t ailment, uint8_t aim) {
  if (maxHp == 0) maxHp = 1;
  if (hp > maxHp) hp = maxHp;
  // estado y punteria como fracciones, todo antes de UNA sola division
  uint32_t sNum = 1, sDen = 1, aNum = 1, aDen = 1;
  if (ailment == AIL_SLEEP || ailment == AIL_FREEZE) sNum = 2;
  else if (ailment == AIL_PARA || ailment == AIL_BURN || ailment == AIL_POISON) { sNum = 3; sDen = 2; }
  if (aim == AIM_PERFECT) aNum = 2;
  else if (aim == AIM_GOOD) { aNum = 3; aDen = 2; }
  else if (aim == AIM_MISS) aDen = 2;
  uint32_t num = (uint32_t)rate * (3u * maxHp - 2u * hp) * sNum * aNum;   // max ~2e8: cabe
  uint32_t den = 3u * maxHp * sDen * aDen;
  uint32_t a = num / den;
  if (a < 1) a = 1;
  if (a > 255) a = 255;
  return (uint8_t)a;
}

uint8_t wildShakes(uint8_t a) {
  // random(255) da 0..254: con a=255 siempre entra. Una sola tirada decide;
  // los tambaleos de un fallo (0..2) son puro adorno
  if (random(255) < a) return 3;
  return (uint8_t)random(3);
}

bool wildFlees(uint8_t failedThrows) {
  return failedThrows >= 4 && random(4) == 0;
}

#define WILD_CAND_MAX 260   // como CAND_MAX en pet.cpp: cabe la region mas grande

// Nivel al que evoluciona la forma anterior de `dex` (el minimo con el que tiene
// sentido verlo), o 2 si es una forma base. Las ramas de Eevee no estan en
// evolvesTo: salen solo por el paso de evolucion de wildRoll, que fija su minimo.
int wildPreEvoLevel(int16_t dex) {
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].evolvesTo == dex && DEX_TBL[d].evolveLevel) return DEX_TBL[d].evolveLevel;
  return 2;
}

bool wildRoll(Pet &p, WildMon &m, uint8_t biome) {
  if (p.isEgg()) return false;
  if (biome >= WILD_BIOMES) biome = 0;
  const RegionInfo &rg = REGIONS[REGION_ALL];   // explorar no mira la region

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
        if (DEX_TBL[d].biome != biome) continue;
        if (pass == 0 && !p.lineHasUnregistered(d)) continue;
        if (!regionAvailable(regionOfDex(d)) || !speciesHasArt(d)) continue;
        cand[n++] = d;
      }
      if (n) { dex = cand[random(n)]; tier = (uint8_t)t; }
    }
  }
  if (!dex) return false;

  const int pet = p.level();
  int lvl;
  if (tier == R_LEGENDARIO) {
    lvl = pet + 10;
    if (lvl < 40) lvl = 40;
  } else {
    // Nivel por especie, como en los juegos: una forma base nunca sale a su nivel
    // de evolucion (atraparla y evolucionarla al momento no tenia gracia).
    // Rango: desde el nivel al que evoluciona su forma anterior (2 si no tiene)
    // hasta su propio nivel de evolucion - 1 (o +15 si ya no evoluciona).
    int lo = wildPreEvoLevel(dex);
    // Puede salir ya evolucionado, un paso cada vez, solo si tu mascota ya llego
    // a ese nivel de evolucion.
    for (;;) {
      int16_t next = DEX_TBL[dex].evolvesTo;
      if (dex == DEX_EEVEE) {                      // la tabla solo guarda 134
        int16_t o[EEVEE_EVO_COUNT];
        uint8_t n = p.eeveeOptions(o);
        if (n) next = o[random(n)];
      }
      if (!next || next > DEX_COUNT || !DEX_TBL[dex].evolveLevel ||
          pet < DEX_TBL[dex].evolveLevel || !speciesHasArt(next) ||
          !regionAvailable(regionOfDex(next)) || random(100) >= WILD_EVOLVED_PCT)
        break;
      lo = DEX_TBL[dex].evolveLevel;
      dex = next;
    }
    const DexEntry &d = DEX_TBL[dex];
    bool evolves = d.evolvesTo && d.evolveLevel;
    int hi = evolves ? d.evolveLevel - 1 : lo + 15;
    if (lo <= 2 && !evolves) { lo = 5; hi = 35; }  // ni evoluciona ni viene de otra
    if (lo < 2) lo = 2;
    int cap = pet + 5;                             // nada muy por encima de ti...
    if (cap < lo) cap = lo;                        // ...salvo su propio minimo
    if (hi > cap) hi = cap;
    if (hi < lo) hi = lo;
    lvl = lo + (int)random(hi - lo + 1);
  }
  if (lvl > MAX_LEVEL) lvl = MAX_LEVEL;

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

void wildLoad(WildState &w, const Pet &p) {
  Preferences pr;
  if (!pr.begin("tamapoke", true)) return;      // sin NVS: el estado del llamador queda como esta
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
    w.biome = pr.getUChar("wbio", 0);
  }
  pr.end();
  if (w.coins > WILD_COINS_MAX) w.coins = WILD_COINS_MAX;
  if (w.balls > WILD_BALLS_MAX) w.balls = WILD_BALLS_MAX;
  if (w.biome >= WILD_BIOMES) w.biome = 0;
  if (!init) {
    w.paidCareDay = p.lastCareDay;
    w.paidMedals = p.totalMedals;
    wildSave(w);
  }
}

void wildSave(const WildState &w) {
  Preferences pr;
  if (!pr.begin("tamapoke", false)) return;
  pr.putBool("wini", true);
  pr.putUShort("wcoin", w.coins);
  pr.putUChar("wball", w.balls);
  pr.putUChar("wexp", w.explores);
  pr.putUChar("wamb", w.ambient);
  pr.putUInt("wday", w.day);
  pr.putUInt("wcare", w.paidCareDay);
  pr.putUShort("wmed", w.paidMedals);
  pr.putUChar("wbio", w.biome);
  pr.end();
}

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

static const uint8_t WILD_BIOME_ORDER[WILD_BIOMES] = { 0, 2, 1, 4, 3, 5 };

uint8_t wildBiomeStep(uint8_t biome, int dir) {
  int at = 0;
  for (int i = 0; i < WILD_BIOMES; i++) if (WILD_BIOME_ORDER[i] == biome) at = i;
  at = (at + (dir < 0 ? WILD_BIOMES - 1 : 1)) % WILD_BIOMES;
  return WILD_BIOME_ORDER[at];
}
