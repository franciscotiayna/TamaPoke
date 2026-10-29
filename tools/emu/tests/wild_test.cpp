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
// 45*(300-2)*2*2/300 en una sola division = 178
  ck(wildCatchValue(R_COMUN, 1, 100, AIL_SLEEP, AIM_PERFECT) == 178, "dormido y perfecto: x4");
  ck(wildCatchValue(R_RARO, 100, 100, AIL_PARA, AIM_GOOD) == 18, "raro, paralizado y bien: 8 x3/2 x3/2");
  ck(wildCatchValue(R_LEGENDARIO, 100, 100, AIL_NONE, AIM_MISS) == 1, "nunca baja de 1");
  ck(wildCatchValue(R_COMUN, 0, 0, AIL_SLEEP, AIM_PERFECT) == 180, "vida 0/0 no divide por cero");
  ck(wildShakes(255) == 3, "a=255 siempre captura");
  // una sola tirada decide: probabilidad a/255, no (a/255)^3
  int c128 = 0;
  for (int i = 0; i < 4000; i++) if (wildShakes(128) == 3) c128++;
  ck(c128 > 1700 && c128 < 2300, "a=128 captura ~50% (a/255, no al cubo)");
  int c1 = 0;
  for (int i = 0; i < 25500; i++) if (wildShakes(1) == 3) c1++;
  ck(c1 > 40 && c1 < 180, "a=1 captura ~1/255");
  bool never3 = true, small = true;
  for (int i = 0; i < 2000; i++) {
    uint8_t s = wildShakes(0);
    if (s == 3) never3 = false;
    if (s > 2) small = false;
  }
  ck(never3 && small, "a=0 nunca captura y los tambaleos del fallo son 0..2");
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
    // vale porque el nivel 17-23 esta por debajo de toda evolucion de Kanto
    // que cruza de region (la mas baja es la 25)
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

  // el filtro de arte debe poder fallar: Unova es la region con huecos (noart.h)
  Pet u;
  u.begin();
  u.dbgHatchAs(495, false);                      // Snivy
  u.ageMinutes = 19UL * MINUTES_PER_LEVEL;
  u.region = 4;                                  // Unova
  if (!regionAvailable(4)) {
    ck(true, "Unova sin arte en el emulador: se omite el filtro de arte");
  } else {
    const RegionInfo &ug = REGIONS[4];
    bool uArt = true, uReg = true;
    int uCount = 0;
    for (int i = 0; i < 3000; i++) {
      WildMon m;
      if (!wildRoll(u, m)) { uArt = false; break; }
      uCount++;
      if (!speciesHasArt(m.dex)) uArt = false;
      if (m.dex < ug.lo || m.dex > ug.hi) uReg = false;
    }
    ck(uCount == 3000 && uArt, "Unova: ninguna especie sin arte");
    ck(uReg, "Unova: todas dentro de su region");
  }

  // Eevee: la rama sale de eeveeOptions(), no solo del 134 de la tabla
  Pet e;
  e.begin();
  e.dbgHatchAs(4, false);
  e.ageMinutes = 29UL * MINUTES_PER_LEVEL;       // nivel 30: Eevee ya puede evolucionar
  e.region = 0;
  int16_t opt[EEVEE_EVO_COUNT];
  uint8_t nOpt = e.eeveeOptions(opt);
  bool seen[EEVEE_EVO_COUNT] = {};
  int branches = 0, eeveeSeen = 0;
  for (int i = 0; i < 20000; i++) {
    WildMon m;
    if (!wildRoll(e, m)) continue;
    if (m.dex == DEX_EEVEE) eeveeSeen++;
    for (int k = 0; k < EEVEE_EVO_COUNT; k++)
      if (m.dex == EEVEE_EVOS[k] && !seen[k]) { seen[k] = true; branches++; }
  }
  printf("     Eevee: %u ramas disponibles, %d vistas, %d Eevee sin evolucionar\n",
         nOpt, branches, eeveeSeen);
  ck(nOpt < 2 || branches >= 2, "Eevee evoluciona por mas de una rama");
}

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

int main() {
  testPersist();
  testEconomy();
  testCatch();
  testRoll();
  testToParty();
  printf(bad ? "%d FAILED\n" : "all passed\n", bad);
  return bad ? 1 : 0;
}
