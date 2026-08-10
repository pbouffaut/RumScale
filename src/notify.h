#pragma once
#include <Arduino.h>

// Notifications Telegram. Les messages sont mis en file et envoyés depuis
// loop() : la détection d'événement ne doit jamais attendre le réseau.
namespace Notify {
  void begin();
  void loop();

  bool configured();

  void served(float servedG, float servedMl, float pct, float remainingMl);
  void thresholdCrossed(uint8_t thresholdPct, float pct, float remainingMl);
  void agingReset(int32_t previousDays);
  bool sendTest();

  size_t queued();
  uint32_t lastError();   // code HTTP du dernier échec, 0 si tout va bien
}
