// Encuentros salvajes y captura: economia, formula de captura, sorteo del
// salvaje y persistencia. Contra wild.cpp real, sin pantalla.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "wild.h"
#include "dex.h"
#include "noart.h"
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

int main() {
  testEconomy();
  testCatch();
  testRoll();
  printf(bad ? "%d FAILED\n" : "all passed\n", bad);
  return bad ? 1 : 0;
}
