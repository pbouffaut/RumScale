#pragma once
#include <Arduino.h>
#include "config.h"

namespace Barrel {
  void begin();
  void update();

  float totalG();        // tout ce qui est posé sur la base
  float liquidG();       // ce qu'il reste de rhum
  float percent();       // 0..100 ; -1 si le tonneau n'a pas été initialisé
  float volumeMl();
  bool  initialized();
  bool  stable();        // le poids est posé et figé

  // Jours de vieillissement, ou :
  //   AGING_NEVER   le vieillissement n'a jamais été démarré
  //   AGING_UNKNOWN il court, mais l'heure n'est pas encore connue — surtout ne
  //                 pas afficher 0, qui se lirait comme « tout a été perdu »
  static const int32_t AGING_NEVER   = -1;
  static const int32_t AGING_UNKNOWN = -2;
  int32_t  agingDays();
  uint32_t agingStartEpoch();
  float    lastServeG();
  uint32_t lastServeEpoch();

  // Initialisation, pilotée depuis l'app.
  bool markEmpty();                     // tonneau vide sur la base
  bool markFull(uint32_t capacityMl);   // tonneau plein
  void resetAging();

  uint32_t nowEpoch();   // 0 si l'heure n'est pas encore synchronisée
  bool     timeValid();
}
