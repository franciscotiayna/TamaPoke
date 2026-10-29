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
// Un reloj que el test adelanta: una ceremonia termina cuando millis() pasa de
// ceremonyUntil, asi que con el reloj parado no acabaria nunca.
static uint32_t gNow = 1;
uint32_t millis() { return gNow; }
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

// Termina la ceremonia como lo hace el firmware: dejando pasar el tiempo hasta
// Pet::update(), no llamando a las dos mitades a mano.
static void finish(Pet &p) {
  gNow += CEREMONY_MS + 1000;
  p.update(gNow);
}

// Una mascota viva, adulta, lista para despedirse, con un huevo apartado.
static int16_t liveWithStoredEgg(Pet &p, int16_t dex) {
  p.begin();
  p.dbgHatchAs(4, false);
  p.registerCaught(4, false);
  p.newEgg();
  int16_t target = p.eggPeek();
  if (!p.stashEgg()) printf("FAIL  (preparacion) no se pudo guardar el huevo\n"), bad++;
  PartyMon m;
  m.dex = dex; m.level = 10;
  p.raiseFrom(m, PartyLife());
  p.fullness = p.joy = p.energy = p.hygiene = 100;
  return target;
}

static void testCeremonyRestoresEgg() {
  {  // a) farewell con huevo apartado: el huevo vuelve, la criatura se guarda
    Pet p;
    int16_t target = liveWithStoredEgg(p, 7);
    p.startFarewell();
    finish(p);
    ck(p.isEgg() && p.eggPeek() == target, "al terminar, el huevo apartado es el activo");
    ck(p.endedKind == CER_FAREWELL && p.endedLife.valid && p.endedMon.dex == 7,
       "y la criatura que se va sale con su vida");
    ck(!p.hasStoredEgg(), "ya no queda huevo apartado");
    Pet q;
    q.begin();
    ck(q.isEgg() && q.eggPeek() == target && !q.hasStoredEgg(),
       "tras reiniciar: el huevo vivo es ese y no hay apartado");
  }
  {  // b) retirada anticipada: la deuda cae sobre el huevo restaurado
    Pet p;
    int16_t target = liveWithStoredEgg(p, 7);
    ck(p.canRetireNow(), "(se puede retirar la criada)");
    p.startRetire();
    ck(p.retireIsEarly(), "(es una retirada anticipada)");
    finish(p);
    ck(p.isEgg() && p.eggPeek() == target, "el huevo apartado vuelve tambien tras retirar");
    ck(p.evoPenalty() == EVO_PENALTY_LEVELS, "con la deuda de la retirada anticipada");
    ck(p.endedKind == CER_NONE, "y una retirada anticipada no guarda nada");
  }
  {  // c) fuga: el huevo vuelve y nada se guarda
    Pet p;
    int16_t target = liveWithStoredEgg(p, 7);
    p.startRunaway();
    finish(p);
    ck(p.isEgg() && p.eggPeek() == target && !p.hasStoredEgg(), "tras una fuga vuelve el huevo apartado");
    ck(p.endedKind == CER_NONE, "y la huida no guarda nada");
  }
}

static void testBlessOnce() {
  Pet p;
  p.begin();
  PartyMon m;
  m.dex = 7; m.level = 10;
  p.raiseFrom(m, PartyLife());
  p.startFarewell();
  ck(p.lastEnd == CER_FAREWELL, "la primera despedida bendice el huevo siguiente");
  finish(p);
  ck(p.endedKind == CER_FAREWELL && p.endedLife.blessUsed == 1, "y la criatura sale marcada como ya bendecida");
  PartyMon m2 = p.endedMon;
  PartyLife l2 = p.endedLife;
  p.endedKind = CER_NONE;

  p.raiseFrom(m2, l2);
  p.fullness = p.joy = p.energy = p.hygiene = 100;
  p.startFarewell();
  ck(p.lastEnd == CER_RELEASE, "criada de nuevo, su despedida ya no bendice (neutra)");
  Pet q;
  q.begin();   // la marca se persiste
  ck(!q.isEgg() && q.lastEnd == CER_RELEASE, "(la despedida neutra sobrevive a un reinicio)");

  // y una criatura nueva vuelve a poder bendecir
  Pet r;
  r.begin();
  r.dbgHatchAs(1, false);
  r.fullness = r.joy = r.energy = r.hygiene = 100;
  r.startFarewell();
  ck(r.lastEnd == CER_FAREWELL, "una criatura recien nacida si bendice");
}

static void testNoLeaks() {
  Pet p;
  p.begin();
  PartyMon m;
  m.dex = 7; m.level = 10;
  p.raiseFrom(m, PartyLife());
  p.toggleLight();
  ck(p.sleepAuto == SLEEP_PLAYER, "(toggleLight deja SLEEP_PLAYER)");
  p.raiseFrom(m, PartyLife());
  ck(p.sleepAuto == SLEEP_NONE, "criar a otro no hereda el modo de sueno del jugador");
}

static void testLifeRoundTrip() {
  Pet p;
  p.begin();
  PartyMon m;
  m.dex = 4; m.level = 12;
  PartyLife l;
  l.valid = 1; l.bond = 40; l.bondToday = 9; l.weight = 7;
  l.careMistakes = 3; l.mistakeCooldown = 2; l.berryKnown = 1;
  l.lastLearnLevel = 17; l.evoPen = 5; l.evoDeclinedLv = 14;
  l.goodTicks = 321; l.ageMinutes = 5000; l.farDeclinedAge = 6000; l.blessUsed = 1;
  p.raiseFrom(m, l);
  PartyMon m2; PartyLife o;
  p.bankFull(m2, o);
  ck(o.valid && o.bond == 40 && o.weight == 7 && o.careMistakes == 3 && o.mistakeCooldown == 2 &&
     o.berryKnown == 1 && o.lastLearnLevel == 17 && o.evoPen == 5 && o.evoDeclinedLv == 14 &&
     o.goodTicks == 321 && o.ageMinutes == 5000 && o.farDeclinedAge == 6000 && o.blessUsed == 1,
     "toda la vida sobrevive a raiseFrom + bankFull");
  ck(o.bondToday == 0, "salvo el tope diario de vinculo, que no se restaura");
}

int main() {
  testPartyLife();
  testCeremonyRestoresEgg();
  testBlessOnce();
  testNoLeaks();
  testLifeRoundTrip();
  testBankRaise();
  testStoredEgg();
  printf(bad ? "%d FAILED\n" : "all passed\n", bad);
  return bad ? 1 : 0;
}
