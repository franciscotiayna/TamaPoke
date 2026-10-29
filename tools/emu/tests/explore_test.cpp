// The EXPLORE row, its shop, the spontaneous "!" bubble, and CRIAR's refusal
// while a move offer is pending -- driven through the sketch's own tap handlers,
// at the coordinates the screens are drawn with.
//
//   * menu row 4 opens EXPLORE; SEARCH starts a wild battle and spends one of
//     the day's searches, and at 5/5 it refuses without starting anything
//   * +1 / +10 respect both the coins and the ball cap, and a purchase waits in
//     wildDirty until EXPLORE is left (an NVS write freezes the flash ~1 s)
//   * the bubble starts a fight when tapped -- but NOT while a main-screen
//     dialog is up: it is hidden then, and a hidden target must not be live
//   * CRIAR is refused while a move offer is pending, and changes nothing
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "wild.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=4242; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render();
void onTap(int16_t,int16_t); void partyTap(int16_t,int16_t);
int uiMenuRowY(int i);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
extern Pet pet;
extern WildState gWild;
extern bool wildDirty, exploreOpen, menuOpen, battleOpen, btlWild, partyOpen;
extern uint32_t wildBubbleUntil, feedMenuUntil, confirmUntil, partyBannerUntil;
extern uint8_t choiceKind, partyDetail, boxSwapFrom;

static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

// The sketch's layout, restated once here: EXP_* (search 146+56, buys 250/306
// +48), WILD_BUB_* (CX+90, 132), the party grid and the sheet's left button.
#define EXP_SEARCH_CY (146 + 56/2)
#define EXP_BUY1_CY (250 + 48/2)
#define EXP_BUY10_CY (306 + 48/2)
#define EXP_BACK_Y 404
#define BUB_X (233 + 90)
#define BUB_Y 132
#define PCELL_X(i) (78 + ((i) % 2) * 160)
#define PCELL_Y(i) (88 + ((i) / 2) * 78)
#define SHEET_L_X 160
#define SHEET_BTN_Y (336 + 48/2)

static void endBattle(){ battleOpen=false; btlWild=false; }
static void quiet(){
  exploreOpen=menuOpen=partyOpen=false;
  feedMenuUntil=confirmUntil=partyBannerUntil=0; choiceKind=0;
  wildBubbleUntil=0;
  endBattle();
}

int main(){
  setup();
  for (int i=0;i<4;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(6,false);
  pet.ageMinutes = 60UL*30;
  while (pet.hasLearnOffer()) pet.declineLearn();
  if (pet.sleeping) pet.toggleLight();
  quiet();
  ck(!strcmp(SCREEN_NAME[uiCurrentScreen()],"main"), "starting on the main screen");

  // ---- the menu row and SEARCH
  {
    gWild.explores = 0; gWild.balls = 5;
    menuOpen = true;
    onTap(233, uiMenuRowY(4) + 22);
    ck(exploreOpen && !menuOpen, "menu row 4 opens EXPLORE");
    ck(!strcmp(SCREEN_NAME[uiCurrentScreen()],"explore"), "and the crumb names it");
    onTap(233, EXP_SEARCH_CY);
    ck(battleOpen && btlWild, "SEARCH starts a wild battle");
    ck(gWild.explores == 1, "and spends one of the day's searches");
    ck(!exploreOpen, "and EXPLORE closes behind it");
    endBattle();

    exploreOpen = true;
    gWild.explores = WILD_EXPLORES_PER_DAY;
    onTap(233, EXP_SEARCH_CY);
    ck(!battleOpen, "at 5/5 SEARCH refuses and starts nothing");
    ck(gWild.explores == WILD_EXPLORES_PER_DAY, "and spends nothing");
    ck(exploreOpen, "and the screen stays up");
    gWild.explores = 0;
  }

  // ---- the shop: coins, the ball cap, and the deferred save
  {
    exploreOpen = true; wildDirty = false;
    gWild.coins = 5; gWild.balls = 0;
    onTap(233, EXP_BUY1_CY);
    ck(gWild.balls == 0 && gWild.coins == 5, "+1 refused with too few coins");
    gWild.coins = 100;
    onTap(233, EXP_BUY10_CY);
    ck(gWild.balls == 10 && gWild.coins == 0, "+10 costs 100 and gives 10");
    ck(wildDirty, "the purchase waits in wildDirty instead of writing NVS now");
    gWild.coins = 1000; gWild.balls = WILD_BALLS_MAX - 5;
    onTap(233, EXP_BUY10_CY);
    ck(gWild.balls == WILD_BALLS_MAX - 5 && gWild.coins == 1000,
       "+10 refused when it would pass the ball cap");
    onTap(233, EXP_BUY1_CY);
    ck(gWild.balls == WILD_BALLS_MAX - 4 && gWild.coins == 1000 - WILD_BALL_PRICE,
       "+1 still fits under the cap");
    onTap(233, EXP_BACK_Y);
    ck(!exploreOpen, "tapping the bottom leaves EXPLORE");
    ck(!wildDirty, "and leaving flushes the purchase to NVS");
    WildState back; wildLoad(back, pet);
    ck(back.balls == gWild.balls && back.coins == gWild.coins, "which really is in NVS");
    gWild.balls = 5;
  }

  // ---- the "!" bubble
  {
    quiet();
    wildBubbleUntil = millis() + 30000;
    onTap(BUB_X, BUB_Y);
    ck(battleOpen && btlWild, "tapping the bubble starts a wild battle");
    ck(wildBubbleUntil == 0, "and uses the bubble up");
    endBattle();

    // The feed menu takes the tap first anyway; what matters is that the bubble
    // survives it rather than being burned by a tap the player aimed at it.
    quiet();
    wildBubbleUntil = millis() + 30000;
    feedMenuUntil = millis() + 6000;
    onTap(BUB_X, BUB_Y);
    ck(!battleOpen, "with the feed menu up the bubble's spot does not start a fight");
    ck(wildBubbleUntil != 0, "and the hidden bubble is still waiting");

    // The party banner has no tap handler of its own: only the gate stops this.
    quiet();
    wildBubbleUntil = millis() + 30000;
    partyBannerUntil = millis() + 3500;
    onTap(BUB_X, BUB_Y);
    ck(!battleOpen, "with the party banner up the hidden bubble is not tappable");
    ck(wildBubbleUntil != 0, "and it is not spent either");
    quiet();
  }

  // ---- CRIAR refuses while a move offer is pending
  {
    quiet();
    for (int i=0;i<PARTY_SLOTS;i++) party.releaseAt(i);
    PartyMon m; m.dex=25; m.level=30; m.ivAtk=m.ivDef=m.ivSpe=m.ivHp=20;
    snprintf(m.nick,sizeof(m.nick),"PIKA");
    party.slots[0]=m; party.save();
    pet.learnQueue[0] = pet.moves[0] ? pet.moves[0] : 1;   // any move id will do
    pet.learnQCount = 1;
    int16_t dex = pet.speciesId;
    partyOpen = true; partyDetail = 0; boxSwapFrom = 0;
    partyTap(PCELL_X(0)+40, PCELL_Y(0)+30);
    ck(partyDetail == 1, "PIKA's sheet opens");
    partyTap(SHEET_L_X, SHEET_BTN_Y);
    ck(pet.speciesId == dex && pet.hasLearnOffer(), "CRIAR refuses with a move offer pending");
    ck(party.slots[0].dex == 25 && party.count() == 1, "and the party is untouched");
    ck(partyOpen && partyDetail == 1, "and the sheet stays up");
    pet.learnQCount = 0;
    partyTap(SHEET_L_X, SHEET_BTN_Y);
    ck(pet.speciesId == 25, "with the offer answered CRIAR works again");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
