#include "wild.h"
#include "moves.h"

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
