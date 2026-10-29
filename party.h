#pragma once
#include <Arduino.h>
#include <Preferences.h>

// The party: pets that finished their life and were kept, rather than being
// dissolved into a single Pokedex bit like every previous ending did.
//
// Only the two endings the player CHOOSES bank a pet -- farewell and release.
// A runaway does not: it is the game's one punishing outcome, and letting a
// neglected pet come back as a team member would take the sting out of it.
#define PARTY_SLOTS 6
// The box: storage beyond the six that fight. Deliberately a SEPARATE NVS key
// rather than a bigger party blob -- growing that blob would change its stride
// and the length-based migration in begin() cannot tell a stride change from a
// slot-count change, so an existing party would be read back misaligned. A new
// key is purely additive and cannot corrupt anything.
#define BOX_SLOTS 18
#define MOVE_SLOTS 4    // the same four every trainer gets in the real games

// A retired pet. Its level is frozen at the moment it joined; it does not keep
// ageing, and nothing about it can be trained any further.
struct PartyMon {
  int16_t dex = 0;      // Pokedex number, 0 = empty slot
  uint16_t level = 1;   // frozen at the moment it was banked
  uint16_t medals = 0;  // what it earned in life
  uint8_t ivAtk = 0, ivDef = 0, ivSpe = 0, ivHp = 0;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  uint8_t shiny = 0;
  char nick[12] = "";
  // Frozen with everything else: the moves you chose while it was alive are
  // what it fights with forever. 0 = empty slot (MOVE_TBL[0] is the "-" filler).
  // Appended at the END of the struct on purpose -- Party::begin() migrates
  // older, shorter blobs by length, and that only works if nothing moved.
  uint8_t moves[MOVE_SLOTS] = { 0, 0, 0, 0 };

  bool empty() const { return dex < 1; }
};

// Lo que un PartyMon no guarda de la vida de la mascota (edad exacta, vinculo,
// peso, descuidos...), para que CRIAR la devuelva tal cual la dejaste. Va en
// claves NVS propias, "plife" y "blife", paralelas a "party" y "box": esas no
// cambian de formato (ver el aviso de begin() sobre agrandar PartyMon) y un
// guardado antiguo simplemente no tiene estas, asi que sus huecos quedan con
// valid = 0 y vuelven a criarse con la edad de su nivel.
struct PartyLife {
  uint8_t valid = 0;
  uint8_t bond = 0, bondToday = 0, weight = 0;
  uint8_t careMistakes = 0, mistakeCooldown = 0;
  uint8_t berryKnown = 0, lastLearnLevel = 0, evoPen = 0, evoDeclinedLv = 0;
  uint16_t goodTicks = 0;
  uint32_t ageMinutes = 0;
  uint32_t farDeclinedAge = 0;
};

class Party {
public:
  PartyMon slots[PARTY_SLOTS];
  PartyMon box[BOX_SLOTS];
  PartyLife life[PARTY_SLOTS];     // la vida de slots[i]
  PartyLife boxLife[BOX_SLOTS];    // la vida de box[i]

  void begin();                 // load from NVS
  uint8_t count() const;
  bool isFull() const { return count() >= PARTY_SLOTS; }
  int firstFree() const;        // index of the first empty slot, -1 if full
  bool add(const PartyMon &m, const PartyLife &l = PartyLife());  // into the first free slot; false if full
  void replaceAt(uint8_t i, const PartyMon &m, const PartyLife &l = PartyLife());
  void releaseAt(uint8_t i);    // free a slot again
  void save();
  uint8_t boxCount() const;
  int boxFirstFree() const;
  bool boxAdd(const PartyMon &m, const PartyLife &l = PartyLife());     // into the first free box slot
  void boxReleaseAt(uint8_t i);
  void boxSave();
  // Swaps a party slot with a box slot. Either may be empty, so this doubles as
  // deposit and withdraw rather than needing three separate operations.
  void swapPartyBox(uint8_t partyIdx, uint8_t boxIdx);

  // combat stats of a party member, same formula as the live pet's
  uint16_t atkOf(const PartyMon &m) const;
  uint16_t defOf(const PartyMon &m) const;
  uint16_t speOf(const PartyMon &m) const;
  uint16_t vitOf(const PartyMon &m) const;
  uint16_t spaOf(const PartyMon &m) const;
  uint16_t spdOf(const PartyMon &m) const;

private:
  Preferences prefs;
};

extern Party party;
