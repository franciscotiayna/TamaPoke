#include "sdmon.h"
#include "noart.h"
#include "pin_config.h"
#include "pet.h"   // gRegionArt, REGIONS -- the mask this narrows
#include <FS.h>
#include <FFat.h>
#include <miniz.h>  // tinfl de la ROM del ESP32-S3 (sprites TPKZ)

// La 1.75C no tiene microSD: los sprites viven en la particion FAT "ffat" de la
// flash interna (partitions.csv) y se suben por USB con tools/send_sd.py.
#define SPRITE_FS FFat

bool sdReady = false;
bool sdDirty = false;
bool sdArtDirty = false;
SdThumbs thumbs;

// Descomprime un TPKZ: "TPKZ" + uint32 tamano descomprimido + flujo zlib.
// Usa el miniz de la ROM. El tinfl_decompressor (~11 KB) va al heap: en la pila
// de loop() no cabe. Devuelve el TPK2 en PSRAM o nullptr si falla.
static uint8_t *tpkzInflate(const uint8_t *src, uint32_t srcLen, uint32_t *outLen) {
  uint32_t rawLen;
  memcpy(&rawLen, src + 4, 4);
  if (rawLen < 7 || rawLen > 3UL * 1024 * 1024) return nullptr;
  uint8_t *out = (uint8_t *)ps_malloc(rawLen);
  tinfl_decompressor *d = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
  if (!out || !d) { free(out); free(d); return nullptr; }
  tinfl_init(d);
  size_t inBytes = srcLen - 8, outBytes = rawLen;
  tinfl_status st = tinfl_decompress(d, src + 8, &inBytes, out, out, &outBytes,
                                     TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  free(d);
  if (st != TINFL_STATUS_DONE || outBytes != rawLen) { free(out); return nullptr; }
  *outLen = rawLen;
  return out;
}

bool PmdMon::load(int16_t dexNum, bool shiny) {
  // int16_t, NOT uint8_t. The dex reached 386 and this did not follow, so
  // everything from 256 up wrapped into Kanto: MARSHTOMP (258) opened
  // p002.bin and drew an IVYSAUR. Same trap that caught DexEntry::evolvesTo
  // and TrainerMon::dex when the expansion landed.
  if (dexNum < 1 || dexNum > 999) return false;
  unload();
  if (!sdReady) return false;

  char path[28];
  snprintf(path, sizeof(path), "/mons/p%s%03u.bin", shiny ? "s" : "", (unsigned)dexNum);
  File f = SPRITE_FS.open(path, FILE_READ);
  if (!f && shiny) {  // sin shiny PMD: usa el normal
    snprintf(path, sizeof(path), "/mons/p%03u.bin", (unsigned)dexNum);
    f = SPRITE_FS.open(path, FILE_READ);
  }
  if (!f) return false;

  uint32_t size = f.size();
  if (size < 7 || size > 3UL * 1024 * 1024) { f.close(); return false; }
  blob = (uint8_t *)ps_malloc(size);
  if (!blob || f.read(blob, size) != size) {
    if (blob) { free(blob); blob = nullptr; }
    f.close();
    return false;
  }
  f.close();
  // TPKZ = TPK2 comprimido con zlib (tools/send_sd.py): asi caben los packs en la FAT
  if (size >= 8 && memcmp(blob, "TPKZ", 4) == 0) {
    uint8_t *raw = tpkzInflate(blob, size, &size);
    free(blob);
    blob = raw;
    if (!blob) { Serial.printf("%s: TPKZ corrupto\n", path); return false; }
  }
  if (size < 7 || memcmp(blob, "TPK2", 4) != 0) { unload(); return false; }

  dex = dexNum;                  // what is actually in here, for the tests
  uint8_t nActs = blob[4];
  memcpy(&palCount, blob + 5, 2);
  if (palCount > 256 || (uint32_t)7 + palCount * 2 > size) { unload(); return false; }
  memcpy(pal, blob + 7, palCount * 2);

  const uint8_t *p = blob + 7 + palCount * 2;
  const uint8_t *end = blob + size;
  for (uint8_t i = 0; i < nActs && p + 4 <= end; i++) {
    uint8_t id = p[0], w = p[1], h = p[2], nf = p[3];
    p += 4;
    if (id >= PMD_NACTS || nf > 24) { unload(); return false; }
    // valida que ms[] y los datos del frame caben en el blob (archivo truncado)
    uint32_t bytes = (uint32_t)nf * 2 + (uint32_t)w * h * nf;
    if (w == 0 || h == 0 || nf == 0 || p + bytes > end) { unload(); return false; }
    PmdAct &a = acts[id];
    a.w = w;
    a.h = h;
    a.frames = nf;
    for (uint8_t k = 0; k < nf; k++) {
      a.ms[k] = p[0] | (p[1] << 8);
      p += 2;
    }
    a.data = p;
    p += (uint32_t)w * h * nf;
    // fila mas baja con contenido en cualquier frame: anclar por los pies
    uint8_t base = 1;
    for (uint8_t f = 0; f < nf; f++) {
      const uint8_t *fr = a.data + (uint32_t)f * w * h;
      for (int r = h - 1; r >= 0; r--) {
        bool any = false;
        for (int c = 0; c < w && !any; c++)
          if (fr[r * w + c] != 0xFF) any = true;
        if (any) { if (r + 1 > base) base = r + 1; break; }
      }
    }
    a.base = base;
  }
  loaded = true;
  Serial.printf("cargado %s (%u KB)\n", path, size / 1024);
  return true;
}

void PmdMon::unload() {
  if (blob) {
    free(blob);
    blob = nullptr;
  }
  for (auto &a : acts) {
    a.w = a.h = a.frames = a.base = 0;
    a.data = nullptr;
  }
  loaded = false;
}

bool SdThumbs::load() {
  if (!sdReady) return false;
  File f = SPRITE_FS.open("/mons/thumbs.bin", FILE_READ);
  if (!f) {
    Serial.println("sin thumbs.bin (galeria sin miniaturas)");
    return false;
  }
  uint32_t size = f.size();
  data = (uint8_t *)ps_malloc(size);
  if (!data || f.read(data, size) != size || memcmp(data, "TPTH", 4) != 0) {
    Serial.println("thumbs.bin invalido");
    if (data) { free(data); data = nullptr; }
    f.close();
    return false;
  }
  f.close();
  memcpy(&count, data + 4, 2);
  loaded = true;
  Serial.printf("miniaturas cargadas: %u (%u KB)\n", count, size / 1024);
  return true;
}

const uint8_t *SdThumbs::get(int16_t dex) const {
  if (!loaded || dex < 1 || dex > count) return nullptr;
  uint32_t off;
  memcpy(&off, data + 6 + 4 * (dex - 1), 4);
  return data + off;
}

// Which regions actually have their sprite pack on the card.
//
// Three probes per region, not one: the realistic failure is a half-finished
// copy of a 100 MB pack, and a single probe would call that region present.
// Start, middle and end, so a copy that stopped partway fails the check.
//
// This NARROWS gRegionArt, which starts as everything. A board with no SD is
// therefore untouched and keeps today's behaviour, which is the documented
// requirement -- an empty game would be a worse answer than a graceful one.
void sdScanRegionArt(bool verbose) {
  if (!sdReady) return;                 // no card: leave every region enabled
  uint16_t mask = 0;
  for (uint8_t r = 0; r < REGION_COUNT; r++) {
    if (r == REGION_ALL) continue;      // derived from the others, never probed
    const RegionInfo &rg = REGIONS[r];
    const int16_t probe[3] = { sdRegionProbe(r, 0), sdRegionProbe(r, 1),
                               sdRegionProbe(r, 2) };
    bool all = true;
    for (int i = 0; i < 3 && all; i++) {
      char path[28];
      snprintf(path, sizeof(path), "/mons/p%03u.bin", (unsigned)probe[i]);
      File f = SPRITE_FS.open(path, FILE_READ);
      if (!f) all = false; else f.close();
    }
    if (all) mask |= (uint16_t)(1u << r);
    if (verbose)
      Serial.printf("art: %-6s %s\n", rg.name, all ? "si" : "NO (falta el pack)");
  }
  gRegionArt = mask;
}

bool sdBegin() {
  sdReady = SPRITE_FS.begin(true /* formatea si no monta */, "/ffat", 4, "ffat");
  if (sdReady) {
    Serial.printf("FAT interna montada: %u/%u KB usados\n",
                  (unsigned)(SPRITE_FS.usedBytes() / 1024), (unsigned)(SPRITE_FS.totalBytes() / 1024));
    SPRITE_FS.mkdir("/mons");
    sdScanRegionArt();
  } else {
    Serial.println("particion ffat no montada (el juego usa los sprites de flash)");
  }
  return sdReady;
}

bool SdMon::load(int16_t dexNum, bool shiny) {
  if (dexNum < 1 || dexNum > 999) return false;
  unload();
  if (!sdReady) return false;

  char path[24];
  snprintf(path, sizeof(path), "/mons/%s%03u.bin", shiny ? "s" : "", (unsigned)dexNum);
  File f = SPRITE_FS.open(path, FILE_READ);
  if (!f && shiny) {  // sin variante shiny: usa la normal
    snprintf(path, sizeof(path), "/mons/%03u.bin", (unsigned)dexNum);
    f = SPRITE_FS.open(path, FILE_READ);
  }
  if (!f) {
    Serial.printf("no existe %s\n", path);
    return false;
  }

  char magic[4];
  uint16_t header[4];
  if (f.read((uint8_t *)magic, 4) != 4 || memcmp(magic, "TPK1", 4) != 0 ||
      f.read((uint8_t *)header, 8) != 8) {
    f.close();
    return false;
  }
  w = header[0];
  h = header[1];
  frames = header[2];
  frameMs = header[3];
  // acota dimensiones: evita size desbordado o absurdo con archivo corrupto
  if (f.read((uint8_t *)&palCount, 2) != 2 || palCount > 256 ||
      w == 0 || w > 256 || h == 0 || h > 256 || frames == 0 || frames > 64) {
    f.close();
    return false;
  }
  if (f.read((uint8_t *)pal, palCount * 2) != palCount * 2) {
    f.close();
    return false;
  }

  uint32_t size = (uint32_t)w * h * frames;
  data = (uint8_t *)ps_malloc(size);
  if (!data) {
    Serial.println("sin PSRAM para el sprite");
    f.close();
    return false;
  }
  uint32_t got = f.read(data, size);
  f.close();
  if (got != size) {
    Serial.printf("%s truncado (%u de %u)\n", path, got, size);
    unload();
    return false;
  }

  // zoom entero para que el bicho mida ~200 px de alto en pantalla
  scale = 200 / h;
  if (scale < 2) scale = 2;
  if (scale > 5) scale = 5;

  Serial.printf("cargado %s: %ux%u x%u frames @%ums, escala %u\n",
                path, w, h, frames, frameMs, scale);
  loaded = true;
  return true;
}

void SdMon::unload() {
  if (data) {
    free(data);
    data = nullptr;
  }
  loaded = false;
}

// ---------------------------------------------------------------------------
// Protocolo de carga por USB (para llenar la SD sin sacarla de la placa):
//   PUT <ruta> <bytes>\n  + datos crudos   -> "OK" ... "DONE"
//   LS\n                                   -> listado de /mons
// Usar con tools/send_sd.py
// ---------------------------------------------------------------------------

bool sdSerialCommand(const String &line) {
  if (line.startsWith("PUT ")) {
    int sp = line.lastIndexOf(' ');
    String path = line.substring(4, sp);
    uint32_t size = line.substring(sp + 1).toInt();
    if (!sdReady || size == 0 || size > 4 * 1024 * 1024) {
      Serial.println("ERR");
      return true;
    }
    if (!path.startsWith("/")) path = "/" + path;
    // acota la escritura a /mons/ (la ruta llega sin sanear por la linea serie)
    if (!path.startsWith("/mons/") || path.indexOf("..") >= 0) {
      Serial.println("ERR");
      return true;
    }
    // reemplaza en vez de anadir: un reintento no debe alargar un fichero a medias
    if (SPRITE_FS.exists(path)) SPRITE_FS.remove(path);
    File f = SPRITE_FS.open(path, FILE_WRITE);
    if (!f) {
      Serial.println("ERR");
      return true;
    }
    Serial.println("OK");
    static uint8_t buf[2048];
    uint32_t remaining = size;
    Serial.setTimeout(5000);
    while (remaining > 0) {
      size_t want = remaining > sizeof(buf) ? sizeof(buf) : remaining;
      size_t n = Serial.readBytes(buf, want);
      if (n == 0) break;  // timeout
      f.write(buf, n);
      remaining -= n;
      Serial.println("#");  // ack: listo para el siguiente bloque
    }
    f.close();
    Serial.setTimeout(1000);
    sdDirty = (remaining == 0);
    // A pack file just landed, so which regions are playable may have changed.
    // Flag it rather than rescanning here: this runs between the last data block
    // and the DONE the host is waiting on, and 15 file opens belong nowhere near
    // that. loop() picks it up.
    if (remaining == 0) sdArtDirty = true;
    Serial.println(remaining == 0 ? "DONE" : "ERR");
    return true;
  } else if (line == "SDINFO") {  // uso de la FAT interna
    Serial.printf("total=%llu used=%llu\n", (unsigned long long)SPRITE_FS.totalBytes(),
                  (unsigned long long)SPRITE_FS.usedBytes());
    Serial.println("DONE");
    return true;
  } else if (line == "LS") {
    File dir = SPRITE_FS.open("/mons");
    if (dir) {
      File e;
      while ((e = dir.openNextFile())) {
        Serial.printf("%s %u\n", e.name(), (uint32_t)e.size());
        e.close();
      }
      dir.close();
    }
    Serial.println("DONE");
    return true;
  }
  return false;
}
