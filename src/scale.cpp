#include "scale.h"
#include "store.h"
#include <HX711.h>

namespace {

struct Channel {
  HX711    hx;
  int32_t  window[MEDIAN_WINDOW];
  uint8_t  wcount = 0;         // combien de cases remplies
  uint8_t  wpos   = 0;         // prochaine case à écrire
  float    ema    = 0.0f;
  bool     emaInit = false;
  uint32_t lastSampleMs = 0;
  bool     alive = false;
};

Channel ch[NUM_CHANNELS];
bool everReady = false;

int32_t medianOf(const int32_t* src, uint8_t n) {
  int32_t tmp[MEDIAN_WINDOW];
  memcpy(tmp, src, n * sizeof(int32_t));
  // Tri par insertion : n vaut 15, autant rester simple et sans allocation.
  for (uint8_t i = 1; i < n; i++) {
    int32_t v = tmp[i];
    int8_t j = i - 1;
    while (j >= 0 && tmp[j] > v) { tmp[j + 1] = tmp[j]; j--; }
    tmp[j + 1] = v;
  }
  return tmp[n / 2];
}

} // namespace

void Scale::begin() {
  ch[0].hx.begin(PIN_HX_A_DOUT, PIN_HX_A_SCK, 128);
#if NUM_CHANNELS > 1
  ch[1].hx.begin(PIN_HX_B_DOUT, PIN_HX_B_SCK, 128);
#endif
  uint32_t now = millis();
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) ch[i].lastSampleMs = now;
  Serial.printf("[scale] %d canal/canaux HX711 initialisé(s)\n", NUM_CHANNELS);
}

void Scale::update() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    Channel& c = ch[i];

    if (!c.hx.is_ready()) {
      // Deux secondes sans conversion : le module ne répond pas.
      if (now - c.lastSampleMs > 2000) c.alive = false;
      continue;
    }

    int32_t raw = (int32_t)c.hx.read();
    c.lastSampleMs = now;
    c.alive = true;

    c.window[c.wpos] = raw;
    c.wpos = (c.wpos + 1) % MEDIAN_WINDOW;
    if (c.wcount < MEDIAN_WINDOW) c.wcount++;

    int32_t med = medianOf(c.window, c.wcount);
    if (!c.emaInit) { c.ema = (float)med; c.emaInit = true; }
    else            { c.ema += EMA_ALPHA * ((float)med - c.ema); }
  }

  if (!everReady) {
    bool all = true;
    for (uint8_t i = 0; i < NUM_CHANNELS; i++)
      if (ch[i].wcount < MEDIAN_WINDOW) all = false;
    everReady = all;
  }
}

bool Scale::ready() { return everReady; }

int32_t Scale::filteredRaw(uint8_t c) {
  if (c >= NUM_CHANNELS) return 0;
  return (int32_t)lroundf(ch[c].ema);
}

bool Scale::channelOk(uint8_t c) {
  if (c >= NUM_CHANNELS) return true;
  return ch[c].alive;
}

int32_t Scale::deltaSum() {
  int32_t sum = 0;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++)
    sum += filteredRaw(i) - Store::s().offsetRaw[i];
  return sum;
}

// Deux cellules sous une plateforme se partagent la charge. Comme le tonneau ne
// bouge plus une fois posé, la répartition est constante : une pente unique
// appliquée à la somme des écarts suffit, et évite d'avoir à calibrer chaque
// cellule séparément.
float Scale::grams() {
  float cpg = Store::s().countsPerGram;
  if (fabsf(cpg) < 1e-3f) return 0.0f;
  return (float)deltaSum() / cpg;
}

void Scale::tare() {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++)
    Store::s().offsetRaw[i] = filteredRaw(i);
  Store::save();
  Serial.printf("[scale] tare : offsets %ld / %ld\n",
                (long)Store::s().offsetRaw[0],
                (long)(NUM_CHANNELS > 1 ? Store::s().offsetRaw[1] : 0));
}

bool Scale::calibrateWithKnown(float knownGrams) {
  if (knownGrams < 100.0f) return false;      // trop léger pour une pente fiable
  int32_t d = deltaSum();
  if (labs(d) < 1000) return false;           // rien n'est posé, ou câblage muet

  Store::s().countsPerGram = (float)d / knownGrams;
  Store::s().calibrated = true;
  Store::save();
  Serial.printf("[scale] calibration : %.2f counts/g\n", Store::s().countsPerGram);
  return true;
}
