// A caught creature is registered in the Pokedex -- in RAM and in NVS -- on both
// answers to "Criarlo ahora?", and survives a reboot. Driven through the real
// battle: startWildBattle(), a ball that holds three shakes, the narration tap,
// and the dialog's buttons.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "wild.h"
#include "dex.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=777; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render();
void onTap(int16_t,int16_t);
void startWildBattle(const WildMon &w);
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);
extern Pet pet;
extern WildState gWild;
extern bool battleOpen, btlWild, raiseAsk, caughtPending;
extern uint8_t throwStage, throwShakes;
extern uint32_t throwT0;

static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

// Catches `dex` and answers the dialog: yes = raise it now, no = bank it.
static void catchOne(int16_t dex, bool yes) {
  WildMon w;
  w.dex = dex; w.level = 10; w.tier = R_COMUN;
  startWildBattle(w);
  ck(battleOpen && btlWild, "wild battle started");
  throwShakes = 3; throwStage = 2; throwT0 = millis() - 60000;   // the ball holds
  render();                                                        // btlThrowTick -> caught
  ck(caughtPending, "caught");
  ck(pet.isRegistered(dex), "registered in RAM right after the catch");
  for (int i = 0; i < 4 && battleOpen; i++) { onTap(233, 300); render(); }  // close narration
  ck(!battleOpen && raiseAsk, "the raise dialog is up");
  for (int i = 0; i < 3; i++) render();
  int t1, b1, t2, b2;
  uiConfirmRects(&t1, &b1, &t2, &b2);
  onTap(233, yes ? (t1 + b1) / 2 : (t2 + b2) / 2);
  for (int i = 0; i < 3; i++) render();
  ck(!raiseAsk, "answered");
  ck(pet.isRegistered(dex), "still registered in RAM after answering");
}

int main(){
  setup();
  for (int i=0;i<4;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(6,false);
  pet.ageMinutes = 60UL*30;
  while (pet.hasLearnOffer()) pet.declineLearn();
  if (pet.sleeping) pet.toggleLight();
  gWild.balls = 10;

  catchOne(202, false);   // Wobbuffet -> party
  catchOne(45, false);    // Vileplume -> party
  catchOne(28, true);     // Sandslash -> raised now

  Pet p2; p2.begin();     // what a reboot would load
  ck(p2.isRegistered(202), "Wobbuffet registered after a reboot");
  ck(p2.isRegistered(45), "Vileplume registered after a reboot");
  ck(p2.isRegistered(28), "Sandslash registered after a reboot");

  printf(bad ? "%d FAILED\n" : "all passed\n", bad);
  return bad ? 1 : 0;
}
