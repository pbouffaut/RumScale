#include "history.h"
#include "store.h"
#include <LittleFS.h>

namespace {

const char* HIST_PATH  = "/hist.bin";
const char* EVENT_PATH = "/events.bin";

bool fsReady = false;
uint32_t lastEpoch = 0;
HistPoint lastPt{0, 0};

// Crée le fichier à sa taille définitive si besoin. Les anneaux s'écrivent
// ensuite par seek() sur une case, jamais par append : la taille ne bouge plus
// et il n'y a rien à compacter.
bool ensureFile(const char* path, size_t bytes) {
  if (LittleFS.exists(path)) {
    File f = LittleFS.open(path, "r");
    size_t sz = f ? f.size() : 0;
    if (f) f.close();
    if (sz == bytes) return true;
    Serial.printf("[hist] %s fait %u octets au lieu de %u, on recrée\n",
                  path, (unsigned)sz, (unsigned)bytes);
    LittleFS.remove(path);
  }

  File f = LittleFS.open(path, "w");
  if (!f) return false;
  uint8_t zeros[256] = {0};
  size_t written = 0;
  while (written < bytes) {
    size_t chunk = min(sizeof(zeros), bytes - written);
    if (f.write(zeros, chunk) != chunk) { f.close(); return false; }
    written += chunk;
  }
  f.close();
  Serial.printf("[hist] %s créé (%u octets)\n", path, (unsigned)bytes);
  return true;
}

template <typename T>
bool writeSlot(const char* path, uint16_t index, const T& rec) {
  File f = LittleFS.open(path, "r+");
  if (!f) return false;
  if (!f.seek((uint32_t)index * sizeof(T))) { f.close(); return false; }
  size_t n = f.write((const uint8_t*)&rec, sizeof(T));
  f.close();
  return n == sizeof(T);
}

} // namespace

bool History::begin() {
  if (!LittleFS.begin(true)) {
    Serial.println(F("[hist] LittleFS indisponible, historique désactivé"));
    return false;
  }
  fsReady = ensureFile(HIST_PATH,  (size_t)HIST_SLOTS  * sizeof(HistPoint))
         && ensureFile(EVENT_PATH, (size_t)EVENT_SLOTS * sizeof(EventRec));
  if (!fsReady) {
    Serial.println(F("[hist] création des fichiers impossible"));
    return false;
  }

  // Retrouve l'horodatage du dernier point pour ne pas en réécrire un
  // immédiatement après un redémarrage.
  uint16_t idx = Store::histIndex();
  uint16_t prev = (idx == 0) ? (HIST_SLOTS - 1) : (idx - 1);
  File f = LittleFS.open(HIST_PATH, "r");
  if (f && f.seek((uint32_t)prev * sizeof(HistPoint))) {
    HistPoint p;
    if (f.read((uint8_t*)&p, sizeof(p)) == sizeof(p)) {
      lastEpoch = p.t;
      lastPt = p;
    }
  }
  if (f) f.close();
  return true;
}

uint32_t History::lastPointEpoch() { return lastEpoch; }

bool History::lastPoint(HistPoint& out) {
  if (lastPt.t == 0) return false;
  out = lastPt;
  return true;
}

void History::addPoint(uint32_t epoch, float liquidG) {
  if (!fsReady || epoch == 0) return;
  HistPoint p{epoch, liquidG};
  uint16_t idx = Store::histIndex();
  if (writeSlot(HIST_PATH, idx, p)) {
    Store::setHistIndex((idx + 1) % HIST_SLOTS);
    lastEpoch = epoch;
    lastPt = p;
  }
}

void History::addEvent(uint32_t epoch, EventType type, float deltaG, float afterG) {
  if (!fsReady) return;
  EventRec e{};
  e.t = epoch;
  e.deltaG = deltaG;
  e.afterG = afterG;
  e.type = (uint8_t)type;
  uint16_t idx = Store::eventIndex();
  if (writeSlot(EVENT_PATH, idx, e))
    Store::setEventIndex((idx + 1) % EVENT_SLOTS);
}

void History::streamPointsJson(Print& out, uint32_t sinceEpoch, uint16_t maxPoints) {
  out.print('[');
  if (!fsReady || maxPoints == 0) { out.print(']'); return; }

  File f = LittleFS.open(HIST_PATH, "r");
  if (!f) { out.print(']'); return; }

  const uint16_t start = Store::histIndex();   // la case la plus ancienne

  // Première passe : combien de points retenus ? Il faut le savoir pour choisir
  // le pas de sous-échantillonnage.
  uint32_t kept = 0;
  for (uint16_t k = 0; k < HIST_SLOTS; k++) {
    uint16_t i = (start + k) % HIST_SLOTS;
    HistPoint p;
    if (!f.seek((uint32_t)i * sizeof(p))) break;
    if (f.read((uint8_t*)&p, sizeof(p)) != sizeof(p)) break;
    if (p.t == 0 || p.t < sinceEpoch) continue;
    kept++;
  }

  uint16_t step = (kept > maxPoints) ? (uint16_t)((kept + maxPoints - 1) / maxPoints) : 1;

  // Deuxième passe : on écrit. Le tout dernier point est toujours émis, même si
  // le pas l'aurait sauté — c'est la valeur que l'utilisateur voit à l'écran.
  uint32_t seen = 0;
  bool first = true;
  for (uint16_t k = 0; k < HIST_SLOTS; k++) {
    uint16_t i = (start + k) % HIST_SLOTS;
    HistPoint p;
    if (!f.seek((uint32_t)i * sizeof(p))) break;
    if (f.read((uint8_t*)&p, sizeof(p)) != sizeof(p)) break;
    if (p.t == 0 || p.t < sinceEpoch) continue;

    seen++;
    bool isLast = (seen == kept);
    if (!isLast && ((seen - 1) % step) != 0) continue;

    if (!first) out.print(',');
    first = false;
    out.printf("[%u,%.1f]", (unsigned)p.t, p.g);
  }
  f.close();
  out.print(']');
}

size_t History::readEvents(EventRec* outArr, size_t maxN) {
  if (!fsReady || maxN == 0) return 0;
  File f = LittleFS.open(EVENT_PATH, "r");
  if (!f) return 0;

  const uint16_t next = Store::eventIndex();   // prochaine case à écrire
  size_t n = 0;
  for (uint16_t k = 1; k <= EVENT_SLOTS && n < maxN; k++) {
    uint16_t i = (next + EVENT_SLOTS - k) % EVENT_SLOTS;   // du plus récent
    EventRec e;
    if (!f.seek((uint32_t)i * sizeof(e))) break;
    if (f.read((uint8_t*)&e, sizeof(e)) != sizeof(e)) break;
    if (e.t == 0 && e.type == EV_NONE) continue;
    outArr[n++] = e;
  }
  f.close();
  return n;
}
