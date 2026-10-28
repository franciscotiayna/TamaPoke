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
4. **Nivel:** el de la mascota ±3, mínimo 3, máximo 100.
5. **Formas evolucionadas:** si el nivel supera el de evolución de la especie
   sorteada, 30 % de que aparezca ya evolucionado (un paso; se repite mientras
   el nivel lo permita y vuelva a salir el 30 %).
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
a = tasa × (3·hpMax − 2·hp) / (3·hpMax) × estado × punteria
```

- `tasa`: común 45, raro 25, legendario 3.
- `estado`: dormido o congelado ×2; paralizado, quemado o envenenado ×1,5; si no ×1.
- Los multiplicadores fraccionarios se aplican como ×3/2 y ×1/2 en enteros.
- `a` se acota a [1, 255].

**Tambaleos.** Hasta 3. Cada uno tira `rand(0..255) < a`; el primero que falla
rompe la bola y el salvaje escapa de ella. Si pasan los 3, queda capturado.

**Tras un fallo** el salvaje juega su turno. Desde el **cuarto** lanzamiento
fallido, cada fallo tiene un 25 % de que el salvaje **huya** y termine el encuentro.

**Captura lograda.**
1. «¡Atrapaste a X!». Se registra en la Pokédex (y en la de shinies si lo es).
2. Diálogo **«¿Criarlo ahora?»**:
   - **Sí:** se hace el intercambio de la sección 6 con la mascota activa; la
     mascota de antes ocupa un hueco libre del equipo o, si no hay, de la caja.
   - **No:** va al primer hueco libre del equipo; si está lleno, a la caja.
3. Con equipo y caja llenos, en cualquiera de las dos respuestas, se abre la
   pantalla existente para elegir a quién soltar y hacer sitio (para el
   capturado si se respondió No, o para la mascota anterior si se respondió Sí).
   Si se cancela, se pide confirmar antes de soltar al que no tiene sitio.

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

- En la ficha de un Pokémon del equipo o la caja, **BRING BACK** pasa a ser
  **CRIAR** y está disponible siempre, no solo con un huevo esperando.
- Es un **intercambio en el mismo hueco**: la mascota activa se convierte en
  `PartyMon` completo y ocupa el hueco del elegido, que pasa a ser la mascota
  activa. Nunca hace falta un hueco libre.
- **No disponible** durante una ceremonia, una evolución o un combate.
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
  mascota de ese momento va al primer hueco libre del equipo o de la caja; si
  ambos están llenos, se aplica la misma pantalla de elegir a quién soltar que
  en la captura (sección 4).
- Si la mascota activa termina (despedida, liberación o huida) y hay un huevo
  guardado, **ese huevo pasa a ser el activo** en lugar de generar uno nuevo.

## 7. Datos y migración

**`PartyMon` ampliado.** Se añaden al final de la estructura los campos de vida
que hoy se pierden al guardar: minutos de edad exactos, vínculo y vínculo de
hoy, peso, baya descubierta, descuidos y su enfriamiento, nivel del último
movimiento aprendido y cualquier otro campo de la mascota que figure en
`SAVE_FIELDS` y afecte a evolución o despedida. Quedan fuera las necesidades,
las cacas, el sueño y la ceremonia, que se restauran al criar.

**Claves NVS nuevas.** Dylan avisa de que agrandar el blob `party`/`box` puede
desalinear lo guardado. Por eso el formato ampliado vive en claves nuevas
`party2` y `box2`:
1. Al arrancar, si existen `party2`/`box2`, se usan.
2. Si no, se leen `party`/`box` con la migración por longitud de siempre, se
   convierten (campos nuevos derivados: edad = nivel en minutos, resto a 0) y se
   escriben `party2`/`box2`. Las claves viejas no se borran.
3. Si la lectura falla, se arranca con equipo vacío sin tocar las claves viejas.

**Huevo guardado:** clave nueva `egg2` (blob con el estado del huevo).

**Economía y contadores:** claves nuevas en el mismo espacio NVS: monedas,
Pokéballs, exploraciones de hoy, avisos de hoy y día del último reinicio.

**Copia de seguridad:** todas las claves nuevas se añaden a `SAVE_FIELDS`; el test
`save_test` falla si falta alguna. `MAX_VAL` (768) va justo con 18 registros de
caja: si el `PartyMon` ampliado supera ~42 bytes, `box2` se divide en dos claves
(`box2a`/`box2b`, 9 registros cada una) en vez de subir `MAX_VAL`.

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
- **Especie salvaje sin sprite en la FAT:** se descarta y se vuelve a sortear (como
  mucho 10 intentos; si no, se cancela el encuentro sin gastar exploración).
- **Equipo y caja llenos al capturar:** ver sección 4.

## 10. Pruebas

**Tests de lógica** (`tools/emu/tests/wild_test.cpp`, con el sistema de Dylan):
- Sorteo: región, rareza, nivel ±3 y topes, legendario solo con 25+ registrados,
  formas evolucionadas, especies sin arte excluidas.
- Captura: fórmula con valores conocidos, acotación a [1, 255], tambaleos, huida
  desde el cuarto fallo.
- Economía: ganancias, compra, topes de 50 bolas y 9.999 monedas, reinicio diario
  de exploraciones y avisos.
- Datos: migración `party`/`box` → `party2`/`box2`, intercambio Pet ↔ PartyMon sin
  pérdida de campos, huevo guardado y su vuelta, `SAVE_FIELDS` completo.

**Comandos serie** para probar en la placa:
- `WILD` fuerza un encuentro; `WILD <dex> <nivel>` uno concreto.
- `COINS <n>` y `BALLS <n>` fijan los valores.
- `WILDINFO` muestra monedas, bolas, exploraciones y avisos de hoy.

**Limitación conocida:** en el PC de trabajo no hay g++ ni SDL, así que el
emulador y sus tests no se pueden compilar ahí. Además, el emulador compila el
`.ino` y necesita stubs para las cabeceras de suspensión del port 1.75C
(`esp_sleep.h`, `driver/gpio.h`, `driver/rtc_io.h`, `pwrPoll`, `batPowerOff`).
La prueba de referencia es en la placa.
