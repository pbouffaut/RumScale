// Interface locale sur écran OLED SSD1306 monochrome 128x64.
// Compilé uniquement pour les cibles -DBOARD_OLED (voir platformio.ini).
#include "config.h"
#if defined(BOARD_OLED)

#include "ui.h"
#include "button.h"
#include "store.h"
#include "scale.h"
#include "barrel.h"
#include "notify.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <qrcode.h>
#include <time.h>

// Pour un écran SH1106 1,3", remplace la ligne suivante par :
//   U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE,
                                                PIN_I2C_SCL, PIN_I2C_SDA);

namespace {

bool oledOk = false;

enum Page : uint8_t { PAGE_MAIN = 0, PAGE_AGING, PAGE_NET, PAGE_DIAG, PAGE_COUNT };
uint8_t page = PAGE_MAIN;
uint32_t lastPageChangeMs = 0;
uint32_t lastDrawMs = 0;

uint32_t resetDoneAtMs = 0;       // affichage de la confirmation

// --- LED témoin -----------------------------------------------------------
uint32_t ledMs = 0;
bool     ledOn = false;

// --- écrans temporaires ---------------------------------------------------
char portalAp[32] = "";
bool portalMode = false;

void drawGauge(int x, int y, int w, int h, float pct) {
  u8g2.drawFrame(x, y, w, h);
  if (pct < 0) return;
  int inner = w - 4;
  int fill = (int)roundf(inner * constrain(pct, 0.0f, 100.0f) / 100.0f);
  if (fill > 0) u8g2.drawBox(x + 2, y + 2, fill, h - 4);
}

void drawMain() {
  const float pct = Barrel::percent();

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawUTF8(0, 10, Store::s().name);

  if (!Barrel::stable()) {
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawUTF8(96, 8, "~~~");   // mesure en train de se poser
  }

  if (pct < 0) {
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawUTF8(0, 32, "À initialiser");
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawUTF8(0, 46, "Ouvre l'app :");
    if (WiFi.status() == WL_CONNECTED)
      u8g2.drawUTF8(0, 56, WiFi.localIP().toString().c_str());
    else
      u8g2.drawUTF8(0, 56, "Wi-Fi non connecté");
    return;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "%d", (int)roundf(pct));
  u8g2.setFont(u8g2_font_logisoso28_tn);
  u8g2.drawStr(2, 42, buf);
  int wNum = u8g2.getStrWidth(buf);
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.drawUTF8(2 + wNum + 3, 42, "%");

  u8g2.setFont(u8g2_font_6x12_tf);
  snprintf(buf, sizeof(buf), "%.2f L", Barrel::volumeMl() / 1000.0f);
  for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
  u8g2.drawUTF8(74, 30, buf);

  int32_t days = Barrel::agingDays();
  if (days >= 0) {
    snprintf(buf, sizeof(buf), "J+%ld", (long)days);
    u8g2.drawUTF8(74, 43, buf);
  } else if (days == Barrel::AGING_UNKNOWN) {
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawUTF8(74, 43, "J+? heure");   // surtout pas « J+0 »
    u8g2.setFont(u8g2_font_6x12_tf);
  }

  drawGauge(0, 50, 128, 14, pct);
}

void drawAging() {
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawUTF8(0, 10, "Vieillissement");

  int32_t days = Barrel::agingDays();
  char buf[24];
  if (days == Barrel::AGING_UNKNOWN) {
    u8g2.drawUTF8(0, 30, "en cours");
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawUTF8(0, 46, "Le compte reprend au");
    u8g2.drawUTF8(0, 56, "retour du Wi-Fi.");
    return;
  }
  if (days < 0) {
    u8g2.drawUTF8(0, 34, "pas encore démarré");
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawUTF8(0, 60, "Maintenir 6 s = démarrer");
    return;
  }

  snprintf(buf, sizeof(buf), "%ld", (long)days);
  u8g2.setFont(u8g2_font_logisoso28_tn);
  u8g2.drawStr(2, 44, buf);
  int wNum = u8g2.getStrWidth(buf);
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.drawUTF8(2 + wNum + 4, 44, days > 1 ? "jours" : "jour");

  uint32_t start = Barrel::agingStartEpoch();
  u8g2.setFont(u8g2_font_4x6_tf);
  if (start) {
    time_t t = (time_t)start;
    struct tm tmv;
    localtime_r(&t, &tmv);
    snprintf(buf, sizeof(buf), "depuis le %02d/%02d/%04d",
             tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_year + 1900);
    u8g2.drawUTF8(0, 60, buf);
  } else {
    u8g2.drawUTF8(0, 60, "date inconnue (pas d'heure)");
  }
}

void drawQr(const char* text, int x, int y, uint8_t scale) {
  // Un QR clair sur fond sombre ne se lit pas : on peint un carré blanc et on
  // creuse les modules en noir, quiet zone comprise.
  QRCode qr;
  uint8_t version = 0;

  // qrcode_getBufferSize() est une fonction, pas une macro : on reprend sa
  // formule ((4·version + 17)² + 7) / 8 pour dimensionner le tampon à la
  // compilation, à la plus grande version qu'on essaiera.
  const uint8_t QR_MAX_VER = 4;
  const uint8_t QR_MAX_SIDE = 4 * QR_MAX_VER + 17;
  static uint8_t buf[(QR_MAX_SIDE * QR_MAX_SIDE + 7) / 8];

  for (uint8_t v = 2; v <= QR_MAX_VER; v++) {
    if (qrcode_initText(&qr, buf, v, ECC_LOW, text) == 0) { version = v; break; }
  }
  if (!version) return;

  const uint8_t quiet = 2;
  int side = (qr.size + 2 * quiet) * scale;
  u8g2.setDrawColor(1);
  u8g2.drawBox(x, y, side, side);
  u8g2.setDrawColor(0);
  for (uint8_t my = 0; my < qr.size; my++) {
    for (uint8_t mx = 0; mx < qr.size; mx++) {
      if (qrcode_getModule(&qr, mx, my)) {
        u8g2.drawBox(x + (mx + quiet) * scale, y + (my + quiet) * scale, scale, scale);
      }
    }
  }
  u8g2.setDrawColor(1);
}

void drawNet() {
  u8g2.setFont(u8g2_font_6x12_tf);

  if (WiFi.status() != WL_CONNECTED) {
    u8g2.drawUTF8(0, 10, "Wi-Fi");
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawUTF8(0, 26, "Non connecté.");
    u8g2.drawUTF8(0, 36, "Coupe et rallume, puis");
    u8g2.drawUTF8(0, 46, "rejoins le réseau :");
    u8g2.drawUTF8(0, 58, AP_NAME);
    return;
  }

  String ip = WiFi.localIP().toString();
  char url[40];
  snprintf(url, sizeof(url), "http://%s", ip.c_str());

  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawUTF8(0, 8, "Scanne pour");
  u8g2.drawUTF8(0, 16, "ouvrir l'app");
  u8g2.drawUTF8(0, 34, ip.c_str());
  u8g2.drawUTF8(0, 44, MDNS_NAME ".local");
  char rssi[16];
  snprintf(rssi, sizeof(rssi), "%d dBm", WiFi.RSSI());
  u8g2.drawUTF8(0, 60, rssi);

  drawQr(url, 70, 3, 2);
}

void drawDiag() {
  char buf[32];
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawUTF8(0, 7, "Diagnostic");

  snprintf(buf, sizeof(buf), "brut %.0f g", Barrel::totalG());
  u8g2.drawUTF8(0, 17, buf);
  snprintf(buf, sizeof(buf), "vide %.0f  plein %.0f",
           Store::s().emptyG, Store::s().fullG);
  u8g2.drawUTF8(0, 25, buf);
  snprintf(buf, sizeof(buf), "%.1f counts/g", Store::s().countsPerGram);
  u8g2.drawUTF8(0, 33, buf);
  snprintf(buf, sizeof(buf), "anges %.0f g", Store::s().evaporatedG);
  u8g2.drawUTF8(0, 41, buf);
  snprintf(buf, sizeof(buf), "HX A:%s B:%s",
           Scale::channelOk(0) ? "ok" : "--",
           NUM_CHANNELS > 1 ? (Scale::channelOk(1) ? "ok" : "--") : "n/a");
  u8g2.drawUTF8(0, 49, buf);
  snprintf(buf, sizeof(buf), "tg %s%s  heure %s",
           Notify::configured() ? "on" : "off",
           Notify::lastError() ? "!" : "",
           Barrel::timeValid() ? "ok" : "--");
  u8g2.drawUTF8(0, 57, buf);
}

void drawResetPrompt() {
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawUTF8(0, 12, "Remettre à zéro");
  u8g2.drawUTF8(0, 26, "le vieillissement ?");
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawUTF8(0, 40, "Maintenir pour confirmer,");
  u8g2.drawUTF8(0, 48, "relâcher pour annuler.");
  drawGauge(0, 54, 128, 10, Button::holdProgress());
}

void drawResetDone() {
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawUTF8(0, 22, "Vieillissement");
  u8g2.drawUTF8(0, 36, "redémarré : J+0");
}

void drawPortal() {
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawUTF8(0, 12, "Configuration Wi-Fi");
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawUTF8(0, 26, "Rejoins ce réseau avec");
  u8g2.drawUTF8(0, 34, "ton téléphone :");
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawUTF8(0, 50, portalAp);
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawUTF8(0, 62, "La page s'ouvre toute seule.");
}

void render() {
  u8g2.clearBuffer();

  if (portalMode) {
    drawPortal();
  } else if (resetDoneAtMs && millis() - resetDoneAtMs < 3000) {
    drawResetDone();
  } else if (Button::armed()) {
    drawResetPrompt();
  } else {
    switch (page) {
      case PAGE_AGING: drawAging(); break;
      case PAGE_NET:   drawNet();   break;
      case PAGE_DIAG:  drawDiag();  break;
      default:         drawMain();  break;
    }
  }

  u8g2.sendBuffer();
}

void doAgingReset() {
  int32_t prev = Barrel::agingDays();
  Barrel::resetAging();
  Notify::agingReset(prev);
  resetDoneAtMs = millis();
  page = PAGE_AGING;
  lastPageChangeMs = millis();
}

void handleButton() {
  Button::update();
  if (Button::takeShortPress()) {
    page = (page + 1) % PAGE_COUNT;
    lastPageChangeMs = millis();
    resetDoneAtMs = 0;
  }
  if (Button::takeHoldFired()) doAgingReset();
}

void handleLed() {
  if (PIN_LED < 0) return;
  uint32_t now = millis();
  // Battement lent quand tout va bien, rapide s'il reste à initialiser.
  uint32_t period = Barrel::initialized() ? 3000 : 700;
  if (!ledOn && now - ledMs > period) { ledOn = true;  ledMs = now; digitalWrite(PIN_LED, HIGH); }
  if (ledOn  && now - ledMs > 60)     { ledOn = false; ledMs = now; digitalWrite(PIN_LED, LOW); }
}

} // namespace

void Ui::begin() {
  Button::begin();
  if (PIN_LED >= 0) {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
  }

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  oledOk = u8g2.begin();
  if (!oledOk) {
    Serial.println(F("[ui] écran OLED absent — on continue sans"));
    return;
  }
  u8g2.setBusClock(400000);
  u8g2.enableUTF8Print();
  Serial.println(F("[ui] écran OLED prêt"));
}

bool Ui::displayPresent() { return oledOk; }

void Ui::showBootMessage(const char* line1, const char* line2) {
  if (!oledOk) return;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.drawUTF8(0, 26, line1);
  u8g2.setFont(u8g2_font_6x12_tf);
  if (line2) u8g2.drawUTF8(0, 46, line2);
  u8g2.sendBuffer();
}

void Ui::showPortalScreen(const char* apName) {
  strlcpy(portalAp, apName, sizeof(portalAp));
  portalMode = true;
  if (oledOk) render();
}

void Ui::loop() {
  handleButton();
  handleLed();

  if (portalMode && WiFi.status() == WL_CONNECTED) portalMode = false;

  // Retour automatique à la page principale.
  if (!portalMode && page != PAGE_MAIN && millis() - lastPageChangeMs > OLED_PAGE_TIMEOUT_MS)
    page = PAGE_MAIN;

  if (!oledOk) return;

  // 5 images/s : assez fluide pour la barre de confirmation, assez lent pour
  // laisser le bus I2C et le Wi-Fi tranquilles.
  if (millis() - lastDrawMs < 200) return;
  lastDrawMs = millis();
  render();
}

#endif  // BOARD_OLED
