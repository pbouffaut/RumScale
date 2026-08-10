#include "notify.h"
#include "store.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace {

const size_t QUEUE_LEN = 4;
const size_t MSG_LEN   = 200;

char  queueBuf[QUEUE_LEN][MSG_LEN];
uint8_t qHead = 0, qCount = 0;
uint32_t lastSendMs = 0;
uint32_t lastErr = 0;

void enqueue(const char* msg) {
  if (!Notify::configured()) return;
  if (qCount >= QUEUE_LEN) {
    Serial.println(F("[notify] file pleine, message abandonné"));
    return;
  }
  uint8_t slot = (qHead + qCount) % QUEUE_LEN;
  strlcpy(queueBuf[slot], msg, MSG_LEN);
  qCount++;
}

// Virgule décimale : c'est un tonneau de rhum, pas un tableur.
void frFloat(char* out, size_t n, float v, uint8_t decimals) {
  snprintf(out, n, "%.*f", decimals, v);
  for (char* p = out; *p; p++) if (*p == '.') *p = ',';
}

bool sendNow(const char* text) {
  const Settings& st = Store::s();
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();     // pas de vérification du certificat : le contenu n'est
  client.setTimeout(12);    // pas sensible. Attention, ce réglage est en secondes.

  char url[128];
  snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", st.tgToken);

  JsonDocument doc;
  doc["chat_id"] = st.tgChat;
  doc["text"] = text;
  doc["disable_notification"] = false;
  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.setConnectTimeout(6000);   // millisecondes, celui-ci
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  http.end();

  if (code == 200) {
    lastErr = 0;
    return true;
  }
  lastErr = (code > 0) ? (uint32_t)code : (uint32_t)(-code + 1000);
  Serial.printf("[notify] échec Telegram, code %d\n", code);
  return false;
}

} // namespace

void Notify::begin() { qHead = qCount = 0; }

bool Notify::configured() {
  const Settings& st = Store::s();
  return st.tgToken[0] != '\0' && st.tgChat[0] != '\0';
}

size_t   Notify::queued()    { return qCount; }
uint32_t Notify::lastError() { return lastErr; }

void Notify::loop() {
  if (qCount == 0) return;
  if (millis() - lastSendMs < 5000) return;   // on ne mitraille pas l'API
  lastSendMs = millis();

  // Qu'il réussisse ou non, le message quitte la file : mieux vaut en perdre un
  // que bloquer la file derrière une configuration erronée.
  const char* msg = queueBuf[qHead];
  sendNow(msg);
  qHead = (qHead + 1) % QUEUE_LEN;
  qCount--;
}

void Notify::served(float servedG, float servedMl, float pct, float remainingMl) {
  (void)servedG;
  char ml[16], p[16], rest[16], msg[MSG_LEN];
  frFloat(ml, sizeof(ml), servedMl, 0);
  frFloat(p, sizeof(p), pct, 0);
  frFloat(rest, sizeof(rest), remainingMl / 1000.0f, 2);
  snprintf(msg, sizeof(msg),
           "🥃 %s ml servis dans le %s.\nIl reste %s %% — %s L.",
           ml, Store::s().name, p, rest);
  enqueue(msg);
}

void Notify::thresholdCrossed(uint8_t thresholdPct, float pct, float remainingMl) {
  char p[16], rest[16], msg[MSG_LEN];
  frFloat(p, sizeof(p), pct, 0);
  frFloat(rest, sizeof(rest), remainingMl / 1000.0f, 2);
  snprintf(msg, sizeof(msg),
           "%s Le %s est descendu à %s %% (%s L).",
           thresholdPct <= 10 ? "🆘" : "⚠️", Store::s().name, p, rest);
  enqueue(msg);
}

void Notify::agingReset(int32_t previousDays) {
  char msg[MSG_LEN];
  if (previousDays > 0)
    snprintf(msg, sizeof(msg),
             "🛢 Nouveau vieillissement lancé dans le %s. "
             "Le précédent a duré %ld jours.",
             Store::s().name, (long)previousDays);
  else
    snprintf(msg, sizeof(msg), "🛢 Vieillissement démarré dans le %s.",
             Store::s().name);
  enqueue(msg);
}

bool Notify::sendTest() {
  if (!configured()) return false;
  char msg[MSG_LEN];
  snprintf(msg, sizeof(msg), "✅ %s est connecté. Les alertes arriveront ici.",
           Store::s().name);
  enqueue(msg);
  return true;
}
