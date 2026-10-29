# Encuentros salvajes y captura — diseño

Rama `dylan-1.75c` (fork de DylanPDao portado a la Waveshare 1.75C).
Fecha: 2026-10-28. Estado: aprobado en conversación, pendiente de revisión escrita.

## Objetivo

Añadir una segunda forma de conseguir Pokémon además de los huevos: encuentros
salvajes que se combaten con el sistema de turnos existente y se capturan con
Pokéballs. Un Pokémon capturado puede guardarse en el equipo o la caja, o
ponerse a criar en el momento, intercambiándolo con la mascota activa. Ese
intercambio queda disponible siempre desde el equipo.

Fuera de alcance: varios tipos de Pokéball, encuentros por pasos (acelerómetro),
captura en combates LAN o de gimnasio, guardería que haga crecer a los guardados.

## Piezas que ya existen y se reutilizan

| Pieza | Dónde | Uso |
|---|---|---|
| Combate suelto contra cualquier especie | `startBattle(dex, lvl)` en `TamaPoke.ino` | Base del combate salvaje |
| Resolución de turnos, IA, tipos, estados | `battle.cpp/.h` | Sin cambios de lógica |
| Sorteo de especie por región y rareza | `Pet::rollInRegion`, `Pet::pickEggSpecies` | Elegir el salvaje |
| Equipo (6) y caja (18) | `party.cpp/.h` (`PartyMon`) | Destino de lo capturado |
| Pokémon guardado → mascota activa | `Pet::reviveFrom(const PartyMon&)` | Base de CRIAR |
| Mascota activa → registro guardado | `Pet::snapshotForParty()` | Base del intercambio |
| Lista de claves NVS de la copia de seguridad | `SAVE_FIELDS` en `save.cpp` | Registrar claves nuevas |

## Arquitectura

- **`wild.cpp/.h` (nuevo), solo lógica, sin pantalla ni hardware**, igual que
  `battle.cpp`: sorteo del encuentro, fórmula de captura y tambaleos, monedas y
  Pokéballs, contadores diarios. Recibe el azar y el día como parámetros para
  poder testearlo en el emulador (`tools/emu`).
- **`TamaPoke.ino`**: pantallas (EXPLORAR, tienda, burbuja «¡!», BALL con anillo,
  diálogo «¿Criarlo ahora?»), el disparo de encuentros y los comandos serie.
- **`pet.cpp/.h` y `party.cpp/.h`**: `PartyMon` ampliado, intercambio completo
  Pet ↔ PartyMon, huevo guardado.
- **`i18n.cpp/.h`**: textos nuevos en los 6 idiomas.

Toda la aritmética es entera, como en `battle.cpp` y `types.h`.

## 1. Cuándo aparece un encuentro

**Explorar a demanda.** Fila nueva **EXPLORAR** en el menú (al tocar el nombre).
Abre la pantalla Explorar con el botón **BUSCAR**: cada pulsación da un
encuentro seguro. **5 exploraciones por día**, que se reponen al cambiar de día.

**Avisos espontáneos.** Con la pantalla encendida, la mascota despierta y la
pantalla principal visible, cada minuto hay una probabilidad de que aparezca la
burbuja «¡!» sobre la mascota (con sonido), ajustada para salir **una vez por
hora de media**, con un **máximo de 3 al día**. La burbuja dura **30 s**: si se
toca, empieza el combate; si no, desaparece. No gasta exploraciones.

**Sin encuentros** (ni BUSCAR ni avisos) cuando: la mascota es un huevo, hay una
ceremonia en curso (evolución, despedida, liberación, huida), la mascota duerme,
hay otra pantalla abierta o un combate en curso. Con 0 Pokéballs sí se puede
explorar, pero la pantalla avisa de que no se podrá capturar.

**Día.** Se calcula igual que la racha de Dylan (clave `cday`). Sin RTC, si la
placa pierde la hora el reinicio diario puede retrasarse; no rompe nada.

## 2. Qué Pokémon aparece

1. **Legendario:** 1 % si hay 25 o más especies registradas. Nivel = nivel de la
   mascota + 10, mínimo 40. Se elige entre los legendarios de la región.
2. **Si no, rareza** con el mismo sorteo que los huevos (`rollInRegion`): común o
   raro, con los mismos bonus por racha y vínculo y la misma preferencia por
   líneas evolutivas incompletas.
3. **Región:** la elegida para los huevos (`region`); `REGION_ALL` sortea en todas.
4. **Nivel por especie** (como en los juegos): desde el nivel al que evoluciona su
   forma anterior (2 si es forma base) hasta su propio nivel de evolución − 1, o
   el mínimo + 15 si ya no evoluciona. Las especies que ni evolucionan ni vienen de
   otra, 5–35. Techo: el nivel de la mascota + 5 (salvo el mínimo de la especie).
   Así un Pokémon recién capturado **nunca está listo para evolucionar**: Caterpie
   2–6, Charmander 2–15, Charmeleon 16–35, Charizard 36–51. (Antes era el nivel
   de la mascota ±3, y casi todo salía listo para evolucionar.)
5. **Formas evolucionadas:** solo si la **mascota** ya alcanzó ese nivel de
   evolución, 30 % de que aparezca ya evolucionado (un paso; se repite mientras
   la mascota lo permita y vuelva a salir el 30 %).
6. **Shiny:** 1/48, con las mismas mejoras por racha y vínculo que los huevos.
7. **IVs:** al azar, como al eclosionar. **Movimientos:** los de su nivel
   (`relearnFromLevel`, como ya hace `startBattle`).
8. Las especies sin arte (`noart.h`) nunca aparecen.

## 3. Combate

- Se construye con `startBattle` (la mascota y el equipo luchan; IA fácil para el
  salvaje). Música de combate de siempre.
- Menú del combate salvaje: **LUCHA / BALL / CAMBIO / HUIR**. Los combates de
  gimnasio y LAN no muestran BALL.
- **HUIR** siempre funciona.
- **Debilitarlo:** no se captura. Premio: +5 monedas y +1 de entrenamiento al
  azar (como el premio de gimnasio, respetando los topes de entrenamiento).
- **Perder:** termina el encuentro sin más castigo.

## 4. Captura

**Lanzamiento.** BALL gasta una Pokéball y abre el anillo de timing: un círculo
que se encoge sobre el salvaje durante ~1,2 s. El toque se puntúa por la
distancia al círculo objetivo:

| Resultado | Multiplicador |
|---|---|
| Perfecto | ×2 |
| Bien | ×1,5 |
| Normal | ×1 |
| Fallo (tarde o sin tocar) | ×0,5 |

**Probabilidad** (sobre 255, entera):

```
a = ratio × (3·hpMax − 2·hp) × estado × punteria / (3·hpMax)
```

- `ratio`: el ratio de captura real de cada especie (0–255), como en los juegos:
  Rattata 255, los iniciales 45, los legendarios 3. Tabla `catchrate.h`, generada
  desde PokéAPI con `tools/gen_catch.py`. La rareza solo decide cuánto aparece.
- `estado`: dormido o congelado ×2; paralizado, quemado o envenenado ×1,5; si no ×1.
- Los multiplicadores fraccionarios (×3/2, ×1/2) se aplican en el numerador, antes
  de la única división entera.
- `a` se acota a [1, 255].

**Tambaleos.** Una sola tirada `rand(0..254) < a` decide la captura (probabilidad
a/255, como en los juegos). Los tambaleos son la animacion: 3 si entra; 0, 1 o 2
al azar si se escapa.

**Tras un fallo** el salvaje juega su turno. Desde el **cuarto** lanzamiento
fallido, cada fallo tiene un 25 % de que el salvaje **huya** y termine el encuentro.

**Captura lograda.**
1. «¡Atrapaste a X!». Se registra en la Pokédex (y en la de shinies si lo es).
2. Diálogo **«¿Criarlo ahora?»**:
   - **Sí:** la mascota de antes se guarda primero en un hueco libre del equipo
     o, si no hay, de la caja (o el huevo se aparta), y después el capturado pasa
     a criarse.
   - **No:** va al primer hueco libre del equipo; si está lleno, a la caja.
3. Con equipo y caja llenos, **Sí** se comporta como **No** (la mascota activa
   nunca queda solo en memoria) y el capturado va a la pantalla existente para
   elegir a quién soltar. Rechazarla suelta al capturado, igual que tras una
   despedida.
4. Hasta que se contesta, el capturado solo está en memoria. Una suspensión (PWR
   larga) lo guarda antes de dormir, salvo con equipo y caja llenos.

El capturado entra como `PartyMon` con `level` = nivel del encuentro, sus IVs,
movimientos, shiny, entrenamiento 0, vínculo 0 y edad equivalente a su nivel.

## 5. Monedas, Pokéballs y tienda

**Ganar monedas:**

| Evento | Monedas |
|---|---|
| Primer cuidado del día | +10, más +1 por día de racha (máx. +10 extra) |
| Ganar un combate salvaje (debilitándolo) | +5 |
| Medalla de gimnasio nueva | +50 |
| Repetir un gimnasio ya ganado | +15 |
| Medalla de cuidado nueva | +30 |

**Tienda** (dentro de la pantalla Explorar): 1 Pokéball = 10 monedas; botones
**+1** y **+10**. Si no alcanza o se llegaría al tope, el botón sale apagado.

**Límites:** 50 Pokéballs, 9.999 monedas. **Inicio:** 5 Pokéballs y 50 monedas.

La pantalla Explorar muestra: BUSCAR (n/5 hoy), Pokéballs, monedas y la tienda.

## 6. Criar: intercambio con el equipo

- En la ficha de un Pokémon del **equipo**, **BRING BACK** pasa a ser **CRIAR**
  y está disponible siempre, no solo con un huevo esperando. Desde la caja se usa
  primero AL EQUIPO (su ficha ya tiene dos botones).
- Es un **intercambio en el mismo hueco**: la mascota activa se convierte en
  `PartyMon` completo y ocupa el hueco del elegido, que pasa a ser la mascota
  activa. Nunca hace falta un hueco libre.
- **Orden ante cortes de luz:** si hay un hueco libre, la mascota actual se guarda
  allí primero y se mueve al hueco del elegido al final (un corte duplica, nunca
  pierde). Con todo lleno el intercambio es directo y hay una ventana documentada.
- **No disponible** durante una ceremonia, una evolución, un combate o con una
  oferta de movimiento pendiente.
- **La bendición de despedida se da una vez por criatura:** volver a criar a un
  Pokémon ya despedido y despedirlo otra vez cuenta como liberación (sin huevo
  bendecido). Evita la granja de huevos bendecidos.
- La mascota que entra **crece de verdad** (`frozen = false`): envejece, sube de
  nivel, evoluciona, entrena y puede despedirse. El comportamiento congelado de
  `reviveFrom` en Dylan desaparece para CRIAR.
- Los Pokémon guardados siguen **congelados** mientras estén en el equipo o la caja.
- Al entrar a criarse se restauran las necesidades: comida, alegría y energía al
  80 %, higiene al 100 %, sin cacas, despierto. Todo lo demás vuelve tal cual.
- Despedirse de un Pokémon criado tras un intercambio funciona igual que con
  cualquier otro: si la despedida o liberación lo guarda, va al equipo.

### Huevo guardado

- Si la mascota activa es un **huevo** al hacer CRIAR, el huevo **no se descarta**:
  se guarda en un hueco de huevo aparte con todo su estado (especie decidida si ya
  lo estaba, progreso de eclosión, toques, región, shiny del huevo y la
  bendición o maldición del último final). Solo cabe **un huevo guardado**.
- El equipo muestra una entrada **HUEVO** mientras exista. Tocarla y elegir
  CRIAR lo vuelve a poner como mascota activa y eclosiona como siempre. La
  mascota de ese momento se guarda antes en el primer hueco libre del equipo o
  de la caja. Con equipo y caja llenos la entrada HUEVO sale gris y no se puede
  usar.
- Si la mascota activa termina (despedida, liberación o huida) y hay un huevo
  guardado, **ese huevo pasa a ser el activo** en lugar de generar uno nuevo.

## 7. Datos y migración

**`PartyLife`, tabla paralela.** Los campos de vida que un `PartyMon` no guarda
(minutos de edad exactos, vínculo, peso, baya descubierta, descuidos y su
enfriamiento, nivel del último movimiento aprendido, rechazos de evolución y
despedida, ticks de defensa, bendición ya usada) viajan en `Party::life[6]` y
`Party::boxLife[18]`, con claves NVS nuevas `plife` y `blife`. `party`/`box` no
cambian de formato, así que no hay migración: un guardado antiguo no tiene esas
claves y sus huecos vuelven a criarse con la edad de su nivel. `PartyLife` mide
24 bytes con relleno explícito (`static_assert`); cambiar su tamaño descarta la
vida guardada. Quedan fuera las necesidades, las cacas, el sueño y la ceremonia,
que se restauran al criar.

**Huevo guardado:** clave nueva `egg2` (blob `StoredEgg`, leído por prefijo para
tolerar que crezca la lista de regiones). **Bendición:** clave `bless`.

**Economía y contadores:** claves nuevas en el mismo espacio NVS: monedas,
Pokéballs, exploraciones de hoy, avisos de hoy y día del último reinicio.

**Copia de seguridad:** todas las claves nuevas se añaden a `SAVE_FIELDS`; el test
`save_test` falla si falta alguna. `blife` ocupa 432 bytes, por debajo de
`MAX_VAL` (768). Los búferes de `EXPORT`/`IMPORT` por consola pasan a 4 KB
(estáticos, no en la pila) porque un guardado completo ya supera 2 KB.

## 8. Pantallas

- **Menú:** fila EXPLORAR. El panel pasa de 5 a 6 filas; se ajustan alto de fila
  y posición del panel para que quepan en la pantalla redonda.
- **Explorar:** BUSCAR (n/5), Pokéballs, monedas, tienda +1 / +10, volver.
- **Burbuja «¡!»:** sobre la mascota durante 30 s; tocarla abre el combate.
- **Combate salvaje:** opción BALL; anillo que se encoge; animación de 1 a 3
  tambaleos; mensaje de resultado.
- **Captura:** «¡Atrapaste a X!» y «¿Criarlo ahora?» con los diálogos de decisión
  que ya existen.
- **Equipo y caja:** botón CRIAR en la ficha; entrada HUEVO si hay huevo guardado.
- **Textos:** en los 6 idiomas de Dylan. Español e inglés revisados; el resto,
  traducción razonable a corregir después.

## 9. Errores y casos límite

- **NVS ilegible o vacío:** valores iniciales (5 bolas, 50 monedas, contadores a 0).
- **Corte de energía durante un intercambio:** se guarda primero el equipo o la
  caja y después la mascota. En el peor caso queda un Pokémon duplicado, nunca
  uno perdido.
- **Suspensión (PWR larga) durante un encuentro:** el encuentro se pierde; la bola
  ya gastada no se devuelve. Monedas y bolas ya ganadas están guardadas.
- **Especie salvaje sin sprite:** el sorteo ya solo elige especies con arte
  (`speciesHasArt`) y de regiones con su pack en la FAT (`regionAvailable`).
- **Equipo y caja llenos al capturar:** ver sección 4.
- **Escrituras en flash:** cada una congela la placa ~1 s. Las compras de la tienda
  y el aviso espontáneo se guardan aplazados (al salir de Explorar, al atenuarse
  la pantalla o al suspender); los gastos de bolas y premios, al momento.

## 10. Pruebas

**Tests de lógica** (`tools/emu/tests/wild_test.cpp`, con el sistema de Dylan):
- Sorteo: región, rareza, nivel ±3 y topes, legendario solo con 25+ registrados,
  formas evolucionadas, especies sin arte excluidas.
- Captura: fórmula con valores conocidos, acotación a [1, 255], tambaleos, huida
  desde el cuarto fallo.
- Economía: ganancias, compra, topes de 50 bolas y 9.999 monedas, reinicio diario
  de exploraciones y avisos.
- Datos (`raise_test.cpp`): la vida viaja con su hueco, intercambio Pet ↔ PartyMon
  sin pérdida de campos, huevo guardado y su vuelta tras una ceremonia, bendición
  una vez, `SAVE_FIELDS` completo.
- Pantallas (`release_test.cpp`, `explore_test.cpp`): CRIAR en los tres estados de
  almacenamiento, píldora HUEVO, Explorar, tienda y burbuja.

**Comandos serie** para probar en la placa:
- `WILD` fuerza un encuentro; `WILD <dex> <nivel>` uno concreto.
- `COINS <n>` y `BALLS <n>` fijan los valores.
- `WILDINFO` muestra monedas, bolas, exploraciones y avisos de hoy.

**Limitación conocida:** en el PC de trabajo no hay g++ ni SDL, así que el
emulador y sus tests no se pueden compilar ahí. Además, el emulador compila el
`.ino` y necesita stubs para las cabeceras de suspensión del port 1.75C
(`esp_sleep.h`, `driver/gpio.h`, `driver/rtc_io.h`, `pwrPoll`, `batPowerOff`).
La prueba de referencia es en la placa.
