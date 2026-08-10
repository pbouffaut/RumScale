// RumScale — un tonneau de vieillissement qui sait ce qu'il lui reste dedans.
//
//   deux HX711 sous une base  ->  poids  ->  niveau, services, part des anges
//   écran OLED + bouton       ->  niveau, jours de vieillissement, remise à zéro
//   app web + WebSocket       ->  suivi en direct, historique, initialisation
//   Telegram                  ->  alertes quand le niveau baisse

#include <Arduino.h>
#include "config.h"
#include "store.h"
#include "scale.h"
#include "history.h"
#include "barrel.h"
#include "notify.h"
#include "ui.h"
#include "net.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("\n=== RumScale ==="));

  Store::begin();
  Ui::begin();
  Ui::showBootMessage("RumScale", "Démarrage…");

  Scale::begin();
  History::begin();
  Notify::begin();
  Barrel::begin();

  Net::begin();       // bloque le temps du portail de configuration, si besoin

  Serial.println(F("[main] prêt"));
}

void loop() {
  Scale::update();    // ne lit un HX711 que lorsqu'une conversion est prête
  Barrel::update();   // paliers, événements, historique
  Ui::loop();         // écran et bouton
  Net::loop();        // WebSocket, redémarrages différés
  Notify::loop();     // file Telegram

  delay(2);           // laisse la main aux tâches Wi-Fi
}
