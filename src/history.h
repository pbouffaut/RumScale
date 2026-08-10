#pragma once
#include <Arduino.h>
#include "config.h"

enum EventType : uint8_t {
  EV_NONE        = 0,
  EV_SERVE       = 1,   // baisse franche : quelqu'un s'est servi
  EV_REFILL      = 2,   // hausse franche : remplissage
  EV_AGING_RESET = 3,   // remise à zéro du compteur de vieillissement
  EV_CANCELLED   = 4,   // variation annulée par son opposé (objet posé sur la base)
};

struct HistPoint {
  uint32_t t;    // epoch UTC, 0 = case jamais écrite
  float    g;    // grammes de liquide
};

struct EventRec {
  uint32_t t;
  float    deltaG;   // variation de liquide (négative pour un service)
  float    afterG;   // liquide restant juste après
  uint8_t  type;
  uint8_t  _pad[3];
};

namespace History {
  bool begin();

  void addPoint(uint32_t epoch, float liquidG);
  void addEvent(uint32_t epoch, EventType type, float deltaG, float afterG);

  // Écrit `[[t,g],...]` en JSON, du plus ancien au plus récent, en
  // sous-échantillonnant pour ne pas dépasser maxPoints.
  void streamPointsJson(Print& out, uint32_t sinceEpoch, uint16_t maxPoints);

  // Les `max` événements les plus récents, du plus récent au plus ancien.
  size_t readEvents(EventRec* out, size_t max);

  uint32_t lastPointEpoch();

  // Dernier point enregistré, pour reprendre le fil après une coupure.
  bool lastPoint(HistPoint& out);
}
