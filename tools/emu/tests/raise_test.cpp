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

int main() {
  testPartyLife();
  printf(bad ? "%d FAILED\n" : "all passed\n", bad);
  return bad ? 1 : 0;
}
