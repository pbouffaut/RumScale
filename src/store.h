#pragma once
#include <Arduino.h>
#include "config.h"

// Tout ce qui doit survivre à une coupure de courant. Stocké en NVS.
struct Settings {
  // --- calibration de la balance ---
  int32_t offsetRaw[2] = {0, 0};      // zéro brut de chaque HX711 (base à vide)
  float   countsPerGram = 100.0f;     // pente, sur la somme des canaux
  bool    calibrated = false;

  // --- initialisation du tonneau ---
  float   emptyG = 0.0f;              // tonneau vide posé sur la base
  float   fullG  = 0.0f;              // tonneau plein
  bool    initialized = false;

  // --- contenu ---
  uint32_t capacityMl = 5000;         // capacité nominale annoncée
  float    densityGml = 0.94f;        // rhum ~40 % vol. ≈ 0,94 g/ml

  // --- vieillissement ---
  uint32_t agingStartEpoch = 0;       // 0 = jamais démarré
  // Un démarrage demandé alors que l'heure n'était pas connue : sans cela, un
  // appui sur le bouton hors ligne effacerait le vieillissement au lieu de le
  // relancer. Résolu dès la première synchronisation NTP.
  bool     agingPending = false;

  // --- identité et notifications ---
  char  name[24]    = "Tonneau";
  char  tgToken[64] = "";
  char  tgChat[24]  = "";
  bool  alertOnServe = true;
  uint8_t lastThresholdPct = 100;     // dernier seuil d'alerte franchi

  // --- cumuls ---
  float evaporatedG = 0.0f;           // total attribué à la part des anges
};

namespace Store {
  void begin();
  void save();                 // écrit les réglages en NVS
  void factoryReset();         // efface tout, y compris l'historique
  Settings& s();

  // Index d'écriture des anneaux d'historique, persistés à part.
  uint16_t histIndex();
  void     setHistIndex(uint16_t i);
  uint16_t eventIndex();
  void     setEventIndex(uint16_t i);
}
