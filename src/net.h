#pragma once
#include <Arduino.h>

namespace Net {
  // Connexion Wi-Fi (portail de configuration au premier démarrage), puis
  // serveur web + WebSocket.
  void begin();
  void loop();

  bool connected();
  String ipString();
}
