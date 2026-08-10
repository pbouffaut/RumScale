#pragma once
#include <Arduino.h>

// Le bouton unique du tonneau, partagé par les deux interfaces (OLED et TFT).
//
//   appui court            -> page suivante
//   maintenu 3 s           -> l'écran demande confirmation
//   maintenu jusqu'à 6 s   -> remise à zéro du vieillissement
//   relâché avant la fin   -> annulé
namespace Button {
  void begin();
  void update();

  bool  takeShortPress();   // vrai une seule fois par appui court
  bool  takeHoldFired();    // vrai une seule fois par appui long confirmé
  bool  armed();            // un appui long est en cours, au-delà du seuil
  float holdProgress();     // 0..100 vers la confirmation
}
