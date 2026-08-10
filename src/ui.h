#pragma once
#include <Arduino.h>

// L'interface locale : écran OLED et bouton unique.
namespace Ui {
  void begin();
  void loop();

  void showBootMessage(const char* line1, const char* line2);
  void showPortalScreen(const char* apName);
  bool displayPresent();
}
