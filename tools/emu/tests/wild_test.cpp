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
