#include "wild.h"
#include "moves.h"
#include "noart.h"   // speciesHasArt(): como pet.cpp

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
