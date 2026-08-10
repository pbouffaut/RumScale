#include "store.h"
#include <Preferences.h>
#include <LittleFS.h>

namespace {
  Preferences prefs;
  Settings settings;
  const char* NS = "rumscale";

  // Les réglages tiennent dans un seul blob : une écriture, une lecture, et
  // aucune clé à maintenir en double quand la structure évolue.
  const char* KEY_BLOB  = "cfg";
  const char* KEY_HIST  = "histIdx";
  const char* KEY_EVT   = "evtIdx";
  // « RSC » + numéro de format. À INCRÉMENTER à chaque modification de la
  // structure Settings : sans cela, un blob d'ancien format de même taille
  // serait relu de travers, et la calibration lue comme du bruit. Le prix d'un
  // incrément est une recalibration ; le prix de l'oubli est une balance qui
  // mesure faux sans le dire.
  const uint32_t MAGIC  = 0x52534332;  // RSC2 : ajout de Settings::agingPending
  const char* KEY_MAGIC = "magic";
}

void Store::begin() {
  prefs.begin(NS, false);

  if (prefs.getUInt(KEY_MAGIC, 0) == MAGIC) {
    Settings loaded;
    size_t n = prefs.getBytes(KEY_BLOB, &loaded, sizeof(loaded));
    if (n == sizeof(loaded)) {
      settings = loaded;
      Serial.println(F("[store] réglages chargés"));
    } else {
      Serial.println(F("[store] blob de taille inattendue, réglages par défaut"));
    }
  } else {
    Serial.println(F("[store] première utilisation, réglages par défaut"));
  }
}

void Store::save() {
  prefs.putBytes(KEY_BLOB, &settings, sizeof(settings));
  prefs.putUInt(KEY_MAGIC, MAGIC);
}

void Store::factoryReset() {
  prefs.clear();
  settings = Settings();
  if (LittleFS.begin(true)) {
    LittleFS.remove("/hist.bin");
    LittleFS.remove("/events.bin");
  }
  Serial.println(F("[store] remise à zéro complète"));
}

Settings& Store::s() { return settings; }

uint16_t Store::histIndex()             { return prefs.getUShort(KEY_HIST, 0); }
void     Store::setHistIndex(uint16_t i){ prefs.putUShort(KEY_HIST, i); }
uint16_t Store::eventIndex()            { return prefs.getUShort(KEY_EVT, 0); }
void     Store::setEventIndex(uint16_t i){ prefs.putUShort(KEY_EVT, i); }
