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

int main() {
  testPartyLife();
  testBankRaise();
  testStoredEgg();
  printf(bad ? "%d FAILED\n" : "all passed\n", bad);
  return bad ? 1 : 0;
}
