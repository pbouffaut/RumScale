#pragma once
#include <Arduino.h>
#include "config.h"

namespace Scale {
  void begin();

  // À appeler à chaque tour de loop(). Ne bloque jamais : on ne lit un HX711
  // que lorsqu'il annonce une conversion prête.
  void update();

  bool  ready();                      // au moins un canal a une fenêtre complète
  float grams();                      // poids total calibré, filtré
  int32_t filteredRaw(uint8_t ch);    // brut filtré d'un canal (diagnostic)
  int32_t instantRaw(uint8_t ch);     // dernière conversion non filtrée (détection de chocs)
  uint32_t lastSampleMs(uint8_t ch);  // horodatage de cette conversion
  int32_t channelDelta(uint8_t ch);   // brut filtré - offset, par canal
  int32_t deltaSum();                 // somme des (brut - offset), tous canaux
  bool  channelOk(uint8_t ch);        // false = HX711 muet (câblage ?)
  uint8_t sampleCount(uint8_t ch);    // mesures accumulées ; 0 = le module ne répond pas

  void tare();
  // Renvoient nullptr si tout s'est bien passé, sinon la raison exacte de
  // l'échec — un diagnostic muet fait perdre une soirée au fer à souder.
  const char* tareChecked();
  const char* calibrateWithKnown(float knownGrams);
}
