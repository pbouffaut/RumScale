#pragma once
#include <Arduino.h>
#include "config.h"

namespace Scale {
  void begin();

  // À appeler à chaque tour de loop(). Ne bloque jamais : on ne lit un HX711
  // que lorsqu'il annonce une conversion prête.
  void update();

  bool  ready();                      // au moins une fenêtre de mesures complète
  float grams();                      // poids total calibré, filtré
  int32_t filteredRaw(uint8_t ch);    // brut filtré d'un canal (diagnostic)
  int32_t deltaSum();                 // somme des (brut - offset), tous canaux
  bool  channelOk(uint8_t ch);        // false = HX711 muet (câblage ?)

  void tare();                              // la charge actuelle devient le zéro
  bool calibrateWithKnown(float knownGrams); // pose un poids connu, puis appelle
}
