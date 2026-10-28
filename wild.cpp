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
