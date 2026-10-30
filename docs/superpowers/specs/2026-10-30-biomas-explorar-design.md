# Biomas en EXPLORAR — diseño

Rama `dylan-1.75c`. Fecha: 2026-10-30. Estado: aprobado en conversación, pendiente
de revisión escrita. Amplía `2026-10-28-encuentros-salvajes-captura-design.md`.

## Objetivo

Al explorar, el jugador elige un **bioma** y solo aparecen especies de ese hábitat:
así puede buscar a propósito lo que le falta (una cueva para Geodude, agua para
Psyduck). Sin selector de dificultad (el nivel por especie y el techo de la mascota
+ 5 ya la dan) y sin regiones (explorar usa las 809 especies).

Fuera de alcance: zonas por dificultad, biomas nuevos o especiales, cambiar el
bioma asignado a cada especie, regiones en la exploración.

## Lo que ya existe

| Pieza | Dónde | Uso |
|---|---|---|
| Bioma de cada especie (0 pradera, 1 playa, 2 bosque, 3 volcán, 4 montaña, 5 nieve) | `DEX_TBL[d].biome` en `dex.h` | Filtro del sorteo |
| Fondo del combate según el bioma del rival | `drawBattleBack()` en `TamaPoke.ino` | Ya funciona: no se toca |
| Sorteo del salvaje | `wildRoll()` en `wild.cpp` | Recibe el bioma |
| Pantalla Explorar | `renderExplore()` / `exploreTap()` | Gana la fila del bioma |

## 1. Biomas

Los seis de Dylan, con nombres para el jugador:

| `biome` | Nombre (ES) | Formas base salvajes (809 especies) |
|---|---|---|
| 0 | PRADERA | 141 |
| 2 | BOSQUE | 83 |
| 1 | AGUA (la «playa» de Dylan) | 60 |
| 4 | CUEVA (la «montaña» de Dylan) | 45 |
| 3 | VOLCÁN | 20 |
| 5 | NIEVE | 10 |

El orden en pantalla es el de la tabla (de más a menos poblado). Internamente se
guarda el valor `biome` de Dylan, no la posición.

## 2. Sorteo

`wildRoll(Pet &p, WildMon &m, uint8_t biome)`:
- Candidatos: formas base (rareza común/rara) **de las 809 especies, sin mirar la
  región**, con `DEX_TBL[d].biome == biome`. Se mantienen el sorteo de rareza, la
  preferencia por líneas incompletas, la exclusión de especies sin arte
  (`speciesHasArt`) y de packs ausentes (`regionAvailable(regionOfDex(d))`).
- **Legendarios:** solo los de ese bioma. Si no hay ninguno disponible en el bioma,
  el sorteo baja de rareza como ya hace.
- **Evoluciones:** el paso de evolución sigue igual (30 %, solo si la mascota ya
  llegó a ese nivel). La forma evolucionada puede tener otro bioma: es la misma
  línea.
- **Nivel:** el nivel por especie actual, sin cambios.
- Si el bioma no tiene ningún candidato (no debería pasar con 809 especies), se
  devuelve false y Explorar no gasta la búsqueda.

## 3. Pantalla Explorar

- Encima de BUSCAR, una fila **«<  BOSQUE  >»**: tocar la flecha izquierda o
  derecha cambia de bioma (en círculo). El nombre va centrado a tamaño 2.
- Se recuerda el último bioma elegido: nuevo campo `uint8_t biome` en `WildState`,
  guardado en NVS (clave `wbio`, `SK_U8`, en `SAVE_FIELDS`) con el mismo guardado
  aplazado que las compras (`wildDirty`). Inicio: PRADERA.
- El resto de la pantalla no cambia (búsquedas del día, Pokéballs, monedas, tienda).
  Si hace falta sitio, los botones se reordenan verticalmente sin cambiar su tamaño
  táctil.

## 4. Avisos «¡!»

El aviso espontáneo usa el último bioma explorado (`gWild.biome`).

## 5. Textos

Seis nombres nuevos en los 6 idiomas (`S_BIOME_GRASS`, `S_BIOME_FOREST`,
`S_BIOME_WATER`, `S_BIOME_CAVE`, `S_BIOME_VOLCANO`, `S_BIOME_SNOW`), ASCII, que quepan
a tamaño 2 entre las flechas.

## 6. Comandos serie

- `WILD` sortea en el bioma actual; `WILD <dex> [nivel]` no cambia.
- `WILDINFO` muestra también el bioma actual.

## 7. Pruebas

En `wild_test.cpp`:
- Con cada bioma, todas las especies que salen (antes de evolucionar) son de ese
  bioma, y aparecen especies de varias regiones aunque la mascota tenga una región
  concreta.
- Los legendarios que salen pertenecen al bioma pedido.
- El bioma se guarda y se carga con `wildSave`/`wildLoad`; un guardado sin `wbio`
  arranca en PRADERA.

En `explore_test.cpp`:
- Las flechas recorren los seis biomas en círculo y BUSCAR usa el elegido.
- Un aviso «¡!» saca una especie del último bioma explorado.
