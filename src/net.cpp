#include "net.h"
#include "config.h"
#include "store.h"
#include "scale.h"
#include "barrel.h"
#include "history.h"
#include "notify.h"
#include "ui.h"
#include "web_ui.h"          // INDEX_HTML — n'est inclus que par ce fichier

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>

namespace {

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
uint32_t lastPushMs = 0;
uint32_t rebootAtMs = 0;

void buildState(JsonDocument& doc) {
  const Settings& st = Store::s();
  float pct = Barrel::percent();

  doc["name"]        = st.name;
  doc["pct"]         = pct < 0 ? (float)-1 : roundf(pct * 10) / 10;
  doc["ml"]          = roundf(Barrel::volumeMl());
  doc["liquid_g"]    = roundf(Barrel::liquidG());
  doc["total_g"]     = roundf(Barrel::totalG());
  doc["capacity_ml"] = st.capacityMl;
  doc["density"]     = st.densityGml;
  doc["stable"]      = Barrel::stable();
  doc["calibrated"]  = st.calibrated;
  doc["initialized"] = Barrel::initialized();
  doc["empty_g"]     = roundf(st.emptyG);
  doc["full_g"]      = roundf(st.fullG);
  doc["counts_per_g"] = st.countsPerGram;

  doc["aging_days"]  = Barrel::agingDays();
  doc["aging_start"] = Barrel::agingStartEpoch();
  doc["epoch"]       = Barrel::nowEpoch();
  doc["time_valid"]  = Barrel::timeValid();

  doc["last_serve_g"]  = roundf(Barrel::lastServeG());
  doc["last_serve_at"] = Barrel::lastServeEpoch();
  doc["evaporated_g"]  = roundf(st.evaporatedG);

  doc["alert_on_serve"] = st.alertOnServe;
  doc["display_sleep_min"] = Store::displaySleepMinutes();
  doc["telegram"]       = Notify::configured();
  doc["tg_error"]       = Notify::lastError();
  doc["tg_chat"]        = st.tgChat;          // le jeton n'est jamais renvoyé

  JsonArray hx = doc["hx"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) hx.add(Scale::channelOk(i));

  // Valeurs brutes par canal : indispensables au montage pour voir vivre chaque
  // cellule séparément, avant que la moindre calibration ait du sens.
  JsonArray raw = doc["raw"].to<JsonArray>();
  JsonArray dlt = doc["raw_delta"].to<JsonArray>();
  JsonArray smp = doc["hx_samples"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    raw.add(Scale::filteredRaw(i));
    dlt.add(Scale::channelDelta(i));
    smp.add(Scale::sampleCount(i));
  }
  doc["scale_ready"] = Scale::ready();

  doc["ip"]        = WiFi.localIP().toString();
  doc["rssi"]      = WiFi.RSSI();
  doc["heap"]      = ESP.getFreeHeap();
  doc["uptime_s"]  = millis() / 1000;
}

void sendState(AsyncWebServerRequest* request) {
  JsonDocument doc;
  buildState(doc);
  AsyncResponseStream* res = request->beginResponseStream("application/json");
  serializeJson(doc, *res);
  request->send(res);
}

void sendOk(AsyncWebServerRequest* request, bool ok, const char* msg = nullptr) {
  JsonDocument doc;
  doc["ok"] = ok;
  if (msg) doc["message"] = msg;
  AsyncResponseStream* res = request->beginResponseStream("application/json");
  res->setCode(ok ? 200 : 400);
  serializeJson(doc, *res);
  request->send(res);
}

const char* eventTypeName(uint8_t t) {
  switch (t) {
    case EV_SERVE:       return "serve";
    case EV_REFILL:      return "refill";
    case EV_AGING_RESET: return "aging_reset";
    case EV_CANCELLED:   return "cancelled";
    default:             return "none";
  }
}

void handleEvents(AsyncWebServerRequest* request) {
  const size_t MAXE = 40;
  static EventRec recs[MAXE];
  size_t n = History::readEvents(recs, MAXE);

  AsyncResponseStream* res = request->beginResponseStream("application/json");
  float d = Store::s().densityGml > 0.1f ? Store::s().densityGml : 0.94f;
  res->print('[');
  for (size_t i = 0; i < n; i++) {
    if (i) res->print(',');
    res->printf("{\"t\":%u,\"type\":\"%s\",\"dg\":%.0f,\"dml\":%.0f,\"after_g\":%.0f}",
                (unsigned)recs[i].t, eventTypeName(recs[i].type),
                recs[i].deltaG, recs[i].deltaG / d, recs[i].afterG);
  }
  res->print(']');
  request->send(res);
}

void handleHistory(AsyncWebServerRequest* request) {
  uint32_t days = 30;
  uint16_t maxPts = 300;
  if (request->hasParam("days"))
    days = (uint32_t)request->getParam("days")->value().toInt();
  if (request->hasParam("max"))
    maxPts = (uint16_t)request->getParam("max")->value().toInt();
  if (maxPts == 0 || maxPts > 800) maxPts = 300;

  uint32_t now = Barrel::nowEpoch();
  uint32_t since = 0;
  if (days > 0 && now > 0 && (uint32_t)days * 86400UL < now)
    since = now - (uint32_t)days * 86400UL;

  AsyncResponseStream* res = request->beginResponseStream("application/json");
  History::streamPointsJson(*res, since, maxPts);
  request->send(res);
}

void registerRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* res = request->beginResponse(
        200, "text/html; charset=utf-8",
        (const uint8_t*)INDEX_HTML, strlen_P(INDEX_HTML));
    request->send(res);
  });

  server.on("/api/state",   HTTP_GET, sendState);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.on("/api/events",  HTTP_GET, handleEvents);

  server.on("/api/tare", HTTP_POST, [](AsyncWebServerRequest* request) {
    const char* err = Scale::tareChecked();
    sendOk(request, err == nullptr, err ? err : "Zéro enregistré.");
  });

  server.on("/api/empty", HTTP_POST, [](AsyncWebServerRequest* request) {
    bool ok = Barrel::markEmpty();
    sendOk(request, ok, ok ? "Poids du tonneau vide enregistré."
                           : "Calibre d'abord la balance avec un poids connu.");
  });

  server.on("/api/aging/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
    int32_t prev = Barrel::agingDays();
    Barrel::resetAging();
    Notify::agingReset(prev);
    sendOk(request, true, "Vieillissement redémarré.");
  });

  server.on("/api/telegram/test", HTTP_POST, [](AsyncWebServerRequest* request) {
    bool ok = Notify::sendTest();
    sendOk(request, ok, ok ? "Message de test envoyé."
                           : "Renseigne d'abord le jeton et l'identifiant de discussion.");
  });

  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* request) {
    sendOk(request, true, "Redémarrage…");
    rebootAtMs = millis() + 500;
  });

  server.on("/api/wifi/forget", HTTP_POST, [](AsyncWebServerRequest* request) {
    sendOk(request, true, "Réseau oublié, le portail de configuration va s'ouvrir.");
    WiFi.disconnect(true, true);
    rebootAtMs = millis() + 800;
  });

  server.on("/api/factory", HTTP_POST, [](AsyncWebServerRequest* request) {
    sendOk(request, true, "Remise à zéro complète, redémarrage…");
    Store::factoryReset();
    rebootAtMs = millis() + 800;
  });

  // --- requêtes avec corps JSON -------------------------------------------
  server.addHandler(new AsyncCallbackJsonWebHandler(
      "/api/calibrate", [](AsyncWebServerRequest* request, JsonVariant& json) {
        float known = json["known_g"] | 0.0f;
        const char* err = Scale::calibrateWithKnown(known);
        sendOk(request, err == nullptr, err ? err : "Balance calibrée.");
      }));

  server.addHandler(new AsyncCallbackJsonWebHandler(
      "/api/full", [](AsyncWebServerRequest* request, JsonVariant& json) {
        uint32_t cap = json["capacity_ml"] | Store::s().capacityMl;
        bool ok = Barrel::markFull(cap);
        sendOk(request, ok, ok ? "Tonneau plein enregistré."
                               : "Le tonneau ne semble pas plus lourd qu'à vide.");
      }));

  server.addHandler(new AsyncCallbackJsonWebHandler(
      "/api/config", [](AsyncWebServerRequest* request, JsonVariant& json) {
        Settings& st = Store::s();

        if (json["name"].is<const char*>())
          strlcpy(st.name, json["name"].as<const char*>(), sizeof(st.name));

        if (json["capacity_ml"].is<uint32_t>()) {
          uint32_t c = json["capacity_ml"].as<uint32_t>();
          if (c >= 250 && c <= 200000) st.capacityMl = c;
        }
        if (json["density"].is<float>()) {
          float d = json["density"].as<float>();
          if (d > 0.7f && d < 1.3f) st.densityGml = d;
        }
        if (json["alert_on_serve"].is<bool>())
          st.alertOnServe = json["alert_on_serve"].as<bool>();
        if (json["display_sleep_min"].is<uint16_t>()) {
          uint16_t minutes = json["display_sleep_min"].as<uint16_t>();
          if (minutes <= 1440) Store::setDisplaySleepMinutes(minutes);
        }

        // Une chaîne vide efface le réglage ; une chaîne absente le laisse tel quel.
        if (json["tg_token"].is<const char*>())
          strlcpy(st.tgToken, json["tg_token"].as<const char*>(), sizeof(st.tgToken));
        if (json["tg_chat"].is<const char*>())
          strlcpy(st.tgChat, json["tg_chat"].as<const char*>(), sizeof(st.tgChat));

        Store::save();
        sendOk(request, true, "Réglages enregistrés.");
      }));

  server.onNotFound([](AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Rien ici.");
  });
}

void onWsEvent(AsyncWebSocket* srv, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    JsonDocument doc;
    buildState(doc);
    String out;
    serializeJson(doc, out);
    client->text(out);
  }
}

} // namespace

void Net::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);       // le HX711 et le WebSocket préfèrent une radio éveillée
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.setAutoReconnect(true);

  // Les raisons de déconnexion (mot de passe, AP introuvable, handshake, etc.)
  // sont indispensables au diagnostic : WiFiManager ne les affiche pas.
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.printf("[net] Wi-Fi déconnecté, raison=%u\n",
                  (unsigned)info.wifi_sta_disconnected.reason);
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  const String savedSsid = WiFi.SSID();
  Serial.printf("[net] SSID mémorisé : %s\n",
                savedSsid.length() ? savedSsid.c_str() : "(aucun)");
  int found = WiFi.scanNetworks(false, true);
  if (found < 0) {
    Serial.printf("[net] scan Wi-Fi impossible (%d)\n", found);
  } else {
    bool seen = false;
    for (int i = 0; i < found; i++) {
      if (savedSsid.length() && WiFi.SSID(i) == savedSsid) {
        seen = true;
        Serial.printf("[net] AP trouvé : %s, %d dBm, canal %d, sécurité %d\n",
                      WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                      (int)WiFi.encryptionType(i));
      }
    }
    if (savedSsid.length() && !seen)
      Serial.println(F("[net] SSID mémorisé absent du scan 2,4 GHz"));
  }
  WiFi.scanDelete();

  WiFiManager wm;
  wm.setConfigPortalTimeout(300);
  wm.setConnectTimeout(20);
  wm.setConnectRetries(3);
  wm.setWiFiAutoReconnect(true);
  wm.setAPCallback([](WiFiManager* m) {
    Serial.println(F("[net] portail de configuration ouvert"));
    Ui::showPortalScreen(AP_NAME);
  });

  Ui::showBootMessage("RumScale", "Connexion Wi-Fi…");
  bool ok = wm.autoConnect(AP_NAME);

  if (!ok) {
    // Pas de Wi-Fi : on reste utilisable en point d'accès autonome. L'app est
    // accessible, mais les notifications Telegram devront attendre.
    Serial.println(F("[net] pas de Wi-Fi, bascule en point d'accès"));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_NAME);
    Ui::showPortalScreen(AP_NAME);
  } else {
    Serial.printf("[net] connecté, IP %s\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin(MDNS_NAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[net] http://%s.local\n", MDNS_NAME);
    }
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
    setenv("TZ", TZ_INFO, 1);
    tzset();
  }

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  registerRoutes();
  server.begin();
  Serial.println(F("[net] serveur web démarré"));
}

void Net::loop() {
  ws.cleanupClients();

  if (rebootAtMs && (int32_t)(millis() - rebootAtMs) >= 0) {
    Serial.println(F("[net] redémarrage"));
    delay(50);
    ESP.restart();
  }

  if (ws.count() && millis() - lastPushMs > 2000) {
    lastPushMs = millis();
    JsonDocument doc;
    buildState(doc);
    String out;
    serializeJson(doc, out);
    ws.textAll(out);
  }
}

bool   Net::connected() { return WiFi.status() == WL_CONNECTED; }
String Net::ipString()  {
  return (WiFi.getMode() & WIFI_AP) && WiFi.status() != WL_CONNECTED
       ? WiFi.softAPIP().toString()
       : WiFi.localIP().toString();
}
