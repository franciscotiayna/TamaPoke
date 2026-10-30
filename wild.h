#pragma once
#include <Arduino.h>
#include "pet.h"
#include "party.h"

// Encuentros salvajes y captura. Solo logica, sin pantalla ni hardware, igual
// que battle.cpp, para poder probarla entera en tools/emu. El azar sale de
// random(), que el emulador siembra con g_seed.

#define WILD_EXPLORES_PER_DAY 5    // busquedas a demanda (fila EXPLORAR)
#define WILD_AMBIENT_PER_DAY 3     // avisos espontaneos "!"
#define WILD_BALL_PRICE 10         // monedas por Pokeball
#define WILD_BALLS_MAX 50
#define WILD_COINS_MAX 9999
#define WILD_START_BALLS 5
#define WILD_START_COINS 50
#define WILD_EVOLVED_PCT 30        // un salvaje con nivel de sobra sale evolucionado
#define WILD_LEGEND_PCT 1
#define WILD_LEGEND_MIN_REG 25     // igual que los huevos: legendarios desde 25 registrados
#define WILD_BIOMES 6               // los de Dylan: DEX_TBL[d].biome (0..5)

// punteria del anillo al lanzar la bola
enum : uint8_t { AIM_MISS = 0, AIM_OK, AIM_GOOD, AIM_PERFECT };

struct WildMon {
  int16_t dex = 0;
  uint8_t level = 1;
  uint8_t tier = R_COMUN;     // R_COMUN / R_RARO / R_LEGENDARIO: cuanto aparece y su nivel
  bool shiny = false;
  uint8_t ivAtk = 16, ivDef = 16, ivSpe = 16, ivHp = 16;
};

// Monedas, bolas y contadores del dia. Del jugador, como la racha: sobrevive a
// todas las crianzas.
struct WildState {
  uint16_t coins = WILD_START_COINS;
  uint8_t balls = WILD_START_BALLS;
  uint8_t explores = 0;       // busquedas gastadas hoy
  uint8_t ambient = 0;        // avisos espontaneos de hoy
  uint32_t day = 0;           // dia (epoch / 86400) de esos dos contadores
  uint32_t paidCareDay = 0;   // ultimo "primer cuidado del dia" ya pagado
  uint16_t paidMedals = 0;    // pet.totalMedals ya pagadas
  uint8_t biome = 0;          // bioma elegido en EXPLORAR (valor de DEX_TBL.biome)
};

void wildNewDay(WildState &w, uint32_t day);   // repone contadores si cambio el dia
void wildEarn(WildState &w, uint16_t coins);   // suma con tope
bool wildCanBuy(const WildState &w, uint8_t n);
bool wildBuy(WildState &w, uint8_t n);         // false = no alcanza o pasaria del tope
uint16_t wildCareCoins(uint16_t streak);       // premio del primer cuidado del dia
bool wildUseExplore(WildState &w);             // gasta una busqueda; false si no quedan

// Ratio de captura de la especie (0-255), el de los juegos: Rattata 255, los
// iniciales 45, los legendarios 3. Tabla en catchrate.h (tools/gen_catch.py).
uint8_t wildCatchRate(int16_t dex);
// Valor de captura 1..255, la formula de los juegos (gen 3 en adelante):
//   ratio x (3*hpMax - 2*hp) / (3*hpMax) x estado x punteria
// Dormido/congelado x2; paralizado, quemado o envenenado x3/2. La punteria del
// anillo hace de tipo de bola: perfecto x2, bien x3/2, fallo x1/2.
uint8_t wildCatchValue(uint8_t rate, uint16_t hp, uint16_t maxHp, uint8_t ailment, uint8_t aim);
// Una sola tirada decide la captura (probabilidad a/255); los tambaleos son solo
// animacion: 3 si entra, 0-2 al azar si escapa.
uint8_t wildShakes(uint8_t a);
// Tras un lanzamiento fallido: desde el cuarto fallo, 25% de que huya.
bool wildFlees(uint8_t failedThrows);

// Sortea el salvaje de un BIOMA (DEX_TBL[d].biome; fuera de rango = pradera),
// de las 809 especies sin mirar la region. Legendario 1% con 25+ registrados
// (solo los del bioma; nivel +10, minimo 40); si no, rareza como los huevos
// (comun / raro con el bonus de cuidado) prefiriendo lineas incompletas. Nivel por
// especie: desde el nivel de evolucion de su forma anterior hasta el suyo - 1 (nunca
// sale listo para evolucionar), con techo en la mascota + 5. Sale evolucionado (30%)
// solo si la mascota ya llego a ese nivel; la forma evolucionada puede ser de otro
// bioma (misma linea). Shiny 1/48 mejorado por el cuidado.
// false si la mascota es un huevo o no hay nada que sacar.
bool wildRoll(Pet &p, WildMon &out, uint8_t biome);
// Siguiente bioma en el orden de la pantalla (PRADERA BOSQUE AGUA CUEVA VOLCAN
// NIEVE), en circulo; dir +1 o -1. Un valor invalido cuenta como pradera.
uint8_t wildBiomeStep(uint8_t biome, int dir);
// Nivel al que evoluciona la forma anterior de una especie (2 si es forma base).
int wildPreEvoLevel(int16_t dex);

// El capturado como miembro del equipo: entrenamiento a 0, edad = su nivel.
void wildToParty(const WildMon &w, const uint8_t moves[MOVE_SLOTS], PartyMon &m, PartyLife &l);

// NVS, en el espacio "tamapoke" de siempre (WIPE lo borra con todo lo demas).
// Sin guardado previo arranca con los valores iniciales y marca como ya
// pagados el cuidado de hoy y las medallas que ya tiene el jugador.
void wildLoad(WildState &w, const Pet &p);
void wildSave(const WildState &w);
