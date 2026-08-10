#include "barrel.h"
#include "scale.h"
#include "store.h"
#include "history.h"
#include "notify.h"
#include <time.h>

namespace {

// --- palier en cours -------------------------------------------------------
float    bandMin = 0, bandMax = 0;
uint32_t bandStartMs = 0;
bool     bandActive = false;
bool     bandPublished = false;
bool     isStable = false;

// --- dernier palier confirmé ----------------------------------------------
float    refG = 0;
uint32_t refEpoch = 0;
bool     refValid = false;   // faux jusqu'au premier palier après démarrage

// --- événement en attente de confirmation ---------------------------------
struct Pending {
  bool     active = false;
  uint8_t  type = EV_NONE;
  float    deltaG = 0;
  float    afterG = 0;
  uint32_t epoch = 0;
  uint32_t confirmAtMs = 0;
};
Pending pending;

float    lastServeGrams = 0;
uint32_t lastServeAt = 0;

// L'évaporation s'accumule sans qu'aucun événement ne déclenche d'écriture. On
// la sauvegarde de temps en temps, assez rarement pour épargner la NVS.
float    savedEvapG = 0;
uint32_t lastEvapSaveMs = 0;

// millis() au moment où un démarrage de vieillissement a été demandé sans heure.
// Vaut 0 si la demande vient d'une session précédente : on la datera alors du
// démarrage de celle-ci, ce qui ne perd que la durée de la coupure.
uint32_t agingPendingSinceMs = 0;

float liquidFromTotal(float total) {
  float l = total - Store::s().emptyG;
  return (l > 0) ? l : 0.0f;
}

// Pourcentage correspondant à une masse de liquide donnée, -1 si non initialisé.
float pctFromLiquid(float liquid) {
  const Settings& st = Store::s();
  if (!st.calibrated || !st.initialized || st.fullG <= st.emptyG + 100.0f) return -1.0f;
  float pct = 100.0f * liquid / (st.fullG - st.emptyG);
  if (pct < 0) pct = 0;
  if (pct > 110) pct = 110;
  return pct;
}

float mlFromLiquid(float liquid) {
  float d = Store::s().densityGml;
  if (d < 0.1f) d = 0.94f;
  return liquid / d;
}

// Démarre le vieillissement maintenant. Sans heure, on enregistre une demande
// plutôt qu'un 0 : un 0 se lirait comme « jamais démarré », et le propriétaire du
// tonneau croirait ses trois mois perdus.
void startAging() {
  Settings& st = Store::s();
  const uint32_t now = Barrel::nowEpoch();
  if (now) {
    st.agingStartEpoch = now;
    st.agingPending = false;
  } else {
    st.agingStartEpoch = 0;
    st.agingPending = true;
    agingPendingSinceMs = millis();
    Serial.println(F("[barrel] vieillissement démarré, datation en attente du NTP"));
  }
}

// Dès que l'heure arrive, on date la demande en attente en remontant de la durée
// écoulée depuis qu'elle a été faite.
void resolveAgingPending() {
  Settings& st = Store::s();
  if (!st.agingPending) return;
  const uint32_t now = Barrel::nowEpoch();
  if (!now) return;

  const uint32_t elapsed = (millis() - agingPendingSinceMs) / 1000UL;
  st.agingStartEpoch = (now > elapsed) ? (now - elapsed) : now;
  st.agingPending = false;
  Store::save();
  Serial.printf("[barrel] vieillissement daté à -%lu s\n", (unsigned long)elapsed);
}

void updateThresholdAlerts(float pct, float ml) {
  Settings& st = Store::s();

  // Remontée franche : on réarme tous les seuils.
  if (pct > (float)st.lastThresholdPct + 5.0f) {
    st.lastThresholdPct = 100;
  }

  for (uint8_t i = 0; i < sizeof(ALERT_THRESHOLDS); i++) {
    uint8_t th = ALERT_THRESHOLDS[i];
    if (pct <= (float)th && st.lastThresholdPct > th) {
      st.lastThresholdPct = th;
      Notify::thresholdCrossed(th, pct, ml);
      break;   // un seul message, celui du seuil le plus haut franchi
    }
  }
}

void commitPending() {
  if (!pending.active) return;

  History::addEvent(pending.epoch, (EventType)pending.type,
                    pending.deltaG, pending.afterG);
  // Un point d'historique immédiat, pour que la courbe montre la marche.
  History::addPoint(pending.epoch, pending.afterG);

  const float pct = pctFromLiquid(pending.afterG);
  const float ml  = mlFromLiquid(pending.afterG);

  if (pending.type == EV_SERVE) {
    lastServeGrams = -pending.deltaG;
    lastServeAt = pending.epoch;
    if (Store::s().alertOnServe)
      Notify::served(lastServeGrams, mlFromLiquid(-pending.deltaG), pct, ml);
  }
  if (pct >= 0) updateThresholdAlerts(pct, ml);

  Serial.printf("[barrel] événement %s %.0f g -> reste %.0f g\n",
                pending.type == EV_SERVE ? "SERVICE" : "REMPLISSAGE",
                pending.deltaG, pending.afterG);

  pending.active = false;
  Store::save();
}

void queueEvent(EventType type, float deltaG, float afterG, uint32_t epoch) {
  // Une variation qui annule la précédente, c'est presque toujours un objet
  // posé sur la base puis retiré. On efface les deux.
  if (pending.active && fabsf(pending.deltaG + deltaG) < EVENT_MIN_G) {
    History::addEvent(epoch, EV_CANCELLED, deltaG, afterG);
    Serial.println(F("[barrel] variation annulée par son opposé, ignorée"));
    pending.active = false;
    return;
  }

  // Deux vrais événements rapprochés : on valide le premier sans attendre.
  if (pending.active) commitPending();

  pending.active = true;
  pending.type = (uint8_t)type;
  pending.deltaG = deltaG;
  pending.afterG = afterG;
  pending.epoch = epoch;
  pending.confirmAtMs = millis() + EVENT_CONFIRM_MS;
}

// Premier palier après un démarrage : on le compare au dernier point
// d'historique pour rattraper l'évaporation survenue pendant que l'appareil
// était éteint. Volontairement, on ne fabrique JAMAIS de service ici — un
// tonneau soulevé puis reposé pendant la coupure suffirait à en inventer un.
void reconcileAfterBoot(float settledTotal, uint32_t epoch) {
  HistPoint p;
  if (!epoch || !History::lastPoint(p) || p.t == 0 || epoch <= p.t) return;

  const float before = p.g;                       // liquide au dernier point
  const float after  = liquidFromTotal(settledTotal);
  const float lost   = before - after;
  if (lost < EVENT_MIN_G) return;

  const float days = (float)(epoch - p.t) / 86400.0f;
  const float ratePerDay = (days > 0.01f) ? (lost / days) : 1e6f;

  if (ratePerDay < EVAP_MAX_G_DAY) {
    Store::s().evaporatedG += lost;
    Serial.printf("[barrel] %.0f g d'évaporation rattrapés sur %.1f jours hors tension\n",
                  lost, days);
  } else {
    // Trop rapide pour de l'évaporation, mais impossible de dire si c'est un
    // service ou une manipulation. On se contente de le consigner.
    History::addEvent(epoch, EV_CANCELLED, -lost, after);
    Serial.printf("[barrel] %.0f g manquants au redémarrage, origine indéterminée\n", lost);
  }
  Store::save();
}

void onSettled(float settledTotal) {
  uint32_t epoch = Barrel::nowEpoch();

  if (!refValid) {   // premier palier depuis l'allumage
    reconcileAfterBoot(settledTotal, epoch);
    refG = settledTotal;
    refEpoch = epoch;
    refValid = true;
    return;
  }

  float delta = settledTotal - refG;
  float afterLiquid = liquidFromTotal(settledTotal);

  if (fabsf(delta) < EVENT_MIN_G) {
    // Trop petit pour un service : c'est de la dérive. La part descendante est
    // mise au compte de l'évaporation.
    if (delta < 0) Store::s().evaporatedG += -delta;
  } else {
    float days = 0.0f;
    if (epoch && refEpoch && epoch > refEpoch)
      days = (float)(epoch - refEpoch) / 86400.0f;
    float ratePerDay = (days > 0.01f) ? (fabsf(delta) / days) : 1e6f;

    if (delta < 0 && ratePerDay < EVAP_MAX_G_DAY) {
      // Baisse importante mais très étalée dans le temps : la part des anges,
      // typiquement après une longue coupure de courant.
      Store::s().evaporatedG += -delta;
    } else {
      queueEvent(delta < 0 ? EV_SERVE : EV_REFILL, delta, afterLiquid, epoch);
    }
  }

  refG = settledTotal;
  refEpoch = epoch;
}

} // namespace

void Barrel::begin() {
  savedEvapG = Store::s().evaporatedG;
  lastEvapSaveMs = millis();

  // Le dernier service ne vivait qu'en RAM : sans cela, la tuile « dernier
  // service » de l'app retombe à vide après chaque coupure de courant, alors
  // que le journal, lui, l'a bien conservé.
  EventRec recs[32];
  size_t n = History::readEvents(recs, 32);   // du plus récent au plus ancien
  for (size_t i = 0; i < n; i++) {
    if (recs[i].type == EV_SERVE) {
      lastServeGrams = -recs[i].deltaG;
      lastServeAt = recs[i].t;
      Serial.printf("[barrel] dernier service retrouvé : %.0f g\n", lastServeGrams);
      break;
    }
  }

  // Rien d'autre à restaurer : la référence de palier se reconstruit après ~20 s de
  // calme, ce qui évite d'inventer un événement au démarrage. L'heure est
  // demandée par Net::begin(), une fois le réseau disponible.
}

bool Barrel::timeValid() {
  return time(nullptr) > 1700000000;   // ~ novembre 2023
}

uint32_t Barrel::nowEpoch() {
  time_t t = time(nullptr);
  return (t > 1700000000) ? (uint32_t)t : 0;
}

void Barrel::update() {
  resolveAgingPending();
  if (!Scale::ready()) return;

  const float cur = Scale::grams();
  const uint32_t now = millis();

  if (!bandActive) {
    bandMin = bandMax = cur;
    bandStartMs = now;
    bandActive = true;
    bandPublished = false;
    isStable = false;
  } else {
    float lo = min(bandMin, cur);
    float hi = max(bandMax, cur);
    if (hi - lo > STABLE_BAND_G) {
      // On sort de la fenêtre : quelque chose se passe, le palier repart.
      bandMin = bandMax = cur;
      bandStartMs = now;
      bandPublished = false;
      isStable = false;
    } else {
      bandMin = lo;
      bandMax = hi;
    }
  }

  if (!bandPublished && (now - bandStartMs) >= STABLE_MS) {
    bandPublished = true;
    isStable = true;
    onSettled((bandMin + bandMax) * 0.5f);
  }

  if (pending.active && (int32_t)(now - pending.confirmAtMs) >= 0)
    commitPending();

  // Un point d'historique par heure, seulement quand la mesure est posée.
  if (isStable && timeValid()) {
    uint32_t epoch = nowEpoch();
    uint32_t last = History::lastPointEpoch();
    if (last == 0 || (epoch > last && epoch - last >= HIST_INTERVAL_S))
      History::addPoint(epoch, liquidG());
  }

  // Sauvegarde paresseuse du cumul d'évaporation : au plus une écriture toutes
  // les six heures, et seulement si le chiffre a bougé.
  if (fabsf(Store::s().evaporatedG - savedEvapG) > 10.0f &&
      (now - lastEvapSaveMs) > 6UL * 3600UL * 1000UL) {
    savedEvapG = Store::s().evaporatedG;
    lastEvapSaveMs = now;
    Store::save();
  }
}

float Barrel::totalG()  { return Scale::grams(); }
float Barrel::liquidG() { return liquidFromTotal(Scale::grams()); }
bool  Barrel::stable()  { return isStable; }

bool Barrel::initialized() {
  const Settings& st = Store::s();
  return st.calibrated && st.initialized && (st.fullG > st.emptyG + 100.0f);
}

float Barrel::percent()  { return pctFromLiquid(liquidG()); }
float Barrel::volumeMl() { return mlFromLiquid(liquidG()); }

int32_t Barrel::agingDays() {
  const Settings& st = Store::s();
  if (st.agingPending) return AGING_UNKNOWN;   // démarré, en attente de l'heure
  if (st.agingStartEpoch == 0) return AGING_NEVER;
  uint32_t now = nowEpoch();
  if (now == 0) return AGING_UNKNOWN;          // l'heure n'est pas revenue
  if (now < st.agingStartEpoch) return 0;
  return (int32_t)((now - st.agingStartEpoch) / 86400UL);
}

uint32_t Barrel::agingStartEpoch() { return Store::s().agingStartEpoch; }
float    Barrel::lastServeG()      { return lastServeGrams; }
uint32_t Barrel::lastServeEpoch()  { return lastServeAt; }

bool Barrel::markEmpty() {
  if (!Scale::ready() || !Store::s().calibrated) return false;
  Store::s().emptyG = Scale::grams();
  Store::save();
  Serial.printf("[barrel] tonneau vide : %.0f g\n", Store::s().emptyG);
  return true;
}

bool Barrel::markFull(uint32_t capacityMl) {
  if (!Scale::ready() || !Store::s().calibrated) return false;
  Settings& st = Store::s();
  float full = Scale::grams();
  if (full <= st.emptyG + 100.0f) return false;   // le tonneau n'a pas été rempli

  st.fullG = full;
  if (capacityMl >= 250 && capacityMl <= 200000) st.capacityMl = capacityMl;

  // La densité se déduit de la mesure : on connaît la masse de liquide ajoutée
  // et le volume annoncé. Plus juste qu'une valeur de table.
  float measured = (full - st.emptyG) / (float)st.capacityMl;
  if (measured > 0.7f && measured < 1.3f) st.densityGml = measured;

  st.initialized = true;
  st.lastThresholdPct = 100;
  if (st.agingStartEpoch == 0 && !st.agingPending) startAging();
  Store::save();

  // La référence de palier est obsolète : le tonneau vient d'être manipulé.
  refValid = false;
  bandActive = false;

  Serial.printf("[barrel] tonneau plein : %.0f g (densité %.3f g/ml)\n",
                st.fullG, st.densityGml);
  return true;
}

void Barrel::resetAging() {
  startAging();
  Store::save();
  History::addEvent(nowEpoch(), EV_AGING_RESET, 0, liquidG());
  Serial.println(F("[barrel] compteur de vieillissement remis à zéro"));
}
