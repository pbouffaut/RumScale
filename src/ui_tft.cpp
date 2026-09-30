// Interface locale sur écran couleur ST7789 320x170 en paysage
// (carte ideaspark 1,9").
// Compilé uniquement pour la cible -DBOARD_TFT_ST7789 (voir platformio.ini).
//
// L'écran reprend le vocabulaire visuel de l'app web : fond bois sombre, liquide
// ambré, texte crème. On ne redessine que ce qui a changé — un rafraîchissement
// complet du tonneau à chaque image ferait clignoter l'affichage.
#include "config.h"
#if defined(BOARD_TFT_ST7789)

#include "ui.h"
#include "button.h"
#include "store.h"
#include "scale.h"
#include "barrel.h"
#include "notify.h"
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>        // uniquement pour les tables de polices
#include <WiFi.h>
#include <qrcode.h>
#include <time.h>

namespace {

Arduino_DataBus* bus = nullptr;
Arduino_GFX*     gfx = nullptr;
bool             tftOk = false;

// --- palette, reprise de l'app web ----------------------------------------
uint16_t COL_BG, COL_CARD, COL_LINE, COL_INK, COL_INK2, COL_INK3;
uint16_t COL_AMBER, COL_AMBER_HI, COL_AMBER_LO, COL_WOOD, COL_WOOD_D, COL_HOOP;
uint16_t COL_TEAL, COL_DANGER;

// --- géométrie ------------------------------------------------------------
const int SCREEN_W    = 320;
const int SCREEN_H    = 170;
const int HEADER_H    = 22;
const int LEFT_W      = 138;
const int BARREL_CX   = 69;
const int BARREL_TOP  = 31;
const int BARREL_H    = 124;
const int BARREL_HALF = 47;
const int INFO_X      = 148;
const int INFO_W      = SCREEN_W - INFO_X - 8;
const int PCT_BASE    = 70;
const int GAUGE_Y     = 145;

enum Page : uint8_t { PAGE_MAIN = 0, PAGE_AGING, PAGE_NET, PAGE_DIAG, PAGE_COUNT };
uint8_t  page = PAGE_MAIN;
uint32_t lastPageChangeMs = 0;
uint32_t resetDoneAtMs = 0;

char portalAp[32] = "";
bool portalMode = false;

// Ce qui est actuellement à l'écran, pour ne réécrire que les différences.
struct Shown {
  int8_t   page = -1;
  bool     portal = false;
  bool     prompt = false;
  bool     done = false;
  bool     setup = false;
  int      pct10 = INT32_MIN;
  int      ml = -1;
  int32_t  days = INT32_MIN;
  bool     stable = true;
  bool     wifi = false;
  int      holdPct = -1;
  char     name[24] = "\x01";
  uint32_t diagMs = 0;
  bool     netDrawn = false;
};
Shown shown;

bool     displayAwake = true;
uint32_t lastDisplayActivityMs = 0;
uint32_t seenSampleMs = 0;
uint32_t lastTapEdgeMs = 0;
uint32_t firstTapMs = 0;
uint8_t  tapCount = 0;
int32_t  previousInstantRaw = 0;
bool     instantRawReady = false;

// -------------------------------------------------------------------------

void setDisplayAwake(bool awake) {
  if (displayAwake == awake) return;
  displayAwake = awake;
  ledcWrite(0, awake ? TFT_BACKLIGHT : 0);
  if (awake) {
    lastDisplayActivityMs = millis();
    shown.page = -1;  // le prochain rendu repart d'un écran propre
  }
}

void updateTapWake() {
  const uint32_t sampleMs = Scale::lastSampleMs(0);
  if (!sampleMs || sampleMs == seenSampleMs) return;
  seenSampleMs = sampleMs;

  const int32_t raw = Scale::instantRaw(0);
  if (!instantRawReady) {
    previousInstantRaw = raw;
    instantRawReady = true;
    return;
  }
  const int32_t jump = labs(raw - previousInstantRaw);
  previousInstantRaw = raw;
  if (displayAwake || jump < TAP_MIN_COUNTS) return;

  const uint32_t now = millis();
  // Le retour élastique d'un même coup crée une seconde arête : le verrouillage
  // l'empêche d'être pris pour le deuxième coup demandé par l'utilisateur.
  if (now - lastTapEdgeMs < TAP_LOCKOUT_MS) return;
  lastTapEdgeMs = now;

  if (!tapCount || now - firstTapMs > TAP_WINDOW_MS) {
    tapCount = 1;
    firstTapMs = now;
    return;
  }
  tapCount = 0;
  setDisplayAwake(true);
}

uint16_t lerp565(uint16_t a, uint16_t b, float t) {
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  int ar = (a >> 11) & 0x1f, ag = (a >> 5) & 0x3f, ab = a & 0x1f;
  int br = (b >> 11) & 0x1f, bg = (b >> 5) & 0x3f, bb = b & 0x1f;
  int r = ar + (int)roundf((br - ar) * t);
  int g = ag + (int)roundf((bg - ag) * t);
  int bl = ab + (int)roundf((bb - ab) * t);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

void initPalette() {
  COL_BG       = gfx->color565(0x14, 0x10, 0x0e);
  COL_CARD     = gfx->color565(0x26, 0x1e, 0x19);
  COL_LINE     = gfx->color565(0x33, 0x29, 0x1f);
  COL_INK      = gfx->color565(0xf2, 0xe8, 0xdc);
  COL_INK2     = gfx->color565(0xb8, 0xa8, 0x94);
  COL_INK3     = gfx->color565(0x9a, 0x8b, 0x78);
  COL_AMBER    = gfx->color565(0xe8, 0xa3, 0x3d);
  COL_AMBER_HI = gfx->color565(0xf5, 0xbe, 0x6b);
  COL_AMBER_LO = gfx->color565(0x9c, 0x5f, 0x1c);
  COL_WOOD     = gfx->color565(0x4a, 0x3a, 0x2b);
  COL_WOOD_D   = gfx->color565(0x24, 0x1c, 0x17);
  COL_HOOP     = gfx->color565(0x6e, 0x5a, 0x45);
  COL_TEAL     = gfx->color565(0x5f, 0xb0, 0xa5);
  COL_DANGER   = gfx->color565(0xe2, 0x70, 0x5f);
}

// Écrit une chaîne dans une bande horizontale entièrement repeinte : c'est ce
// qui évite les résidus quand « 100 » devient « 99 ».
void bandText(int y, int h, const uint8_t* font, uint16_t color,
              const char* s, int align = 0 /*-1 gauche, 0 centre, 1 droite*/,
              int baseline = -1, uint16_t bg = 0xFFFF) {
  gfx->fillRect(0, y, SCREEN_W, h, bg == 0xFFFF ? COL_BG : bg);
  if (!s || !*s) return;
  gfx->setFont(font);
  gfx->setTextColor(color);
  int16_t x1, y1; uint16_t w, hh;
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &hh);
  int x = (align < 0) ? 8 : (align > 0 ? SCREEN_W - 8 - (int)w : (SCREEN_W - (int)w) / 2);
  gfx->setCursor(x, baseline < 0 ? y + h - 4 : baseline);
  gfx->print(s);
}

void regionText(int x, int y, int w, int h, const uint8_t* font,
                uint16_t color, const char* s, int align = -1,
                int baseline = -1) {
  gfx->fillRect(x, y, w, h, COL_BG);
  if (!s || !*s) return;
  gfx->setFont(font);
  gfx->setTextColor(color);
  int16_t x1, y1; uint16_t tw, th;
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);
  int tx = align < 0 ? x : (align > 0 ? x + w - (int)tw : x + (w - (int)tw) / 2);
  gfx->setCursor(tx, baseline < 0 ? y + h - 3 : baseline);
  gfx->print(s);
}

void comma(char* s) { for (char* p = s; *p; p++) if (*p == '.') *p = ','; }

// Défini plus bas avec la page réseau, mais aussi utilisé par l'écran
// d'initialisation de la page principale.
void drawQr(const char* text, int y, uint8_t scale);

// --- le tonneau -----------------------------------------------------------

int barrelHalfWidth(int i) {           // i = ligne, 0 en haut du tonneau
  float f = (float)i / (float)BARREL_H;
  float t = 2.0f * f - 1.0f;
  float bulge = 1.0f - 0.24f * t * t;  // étroit aux extrémités, bombé au milieu
  return (int)roundf(BARREL_HALF * bulge);
}

void drawHoop(int i) {
  const int cx = BARREL_CX;
  for (int k = 0; k < 8; k++) {
    int hw = barrelHalfWidth(i + k);
    gfx->drawFastHLine(cx - hw, BARREL_TOP + i + k, 2 * hw,
                       (k == 0 || k == 7) ? COL_WOOD : COL_HOOP);
  }
}

void drawBarrel(float pct) {
  const int cx = BARREL_CX;
  const int bottom = BARREL_TOP + BARREL_H;
  const int levelY = (pct < 0)
      ? bottom
      : bottom - (int)roundf(BARREL_H * (pct < 0 ? 0 : (pct > 100 ? 100 : pct)) / 100.0f);

  for (int i = 0; i < BARREL_H; i++) {
    const int y = BARREL_TOP + i;
    const int hw = barrelHalfWidth(i);
    uint16_t fill;
    if (y >= levelY) {
      float g = (float)(y - levelY) / (float)max(1, bottom - levelY);
      fill = lerp565(COL_AMBER_HI, COL_AMBER_LO, g);
    } else {
      fill = COL_WOOD_D;
    }
    gfx->drawFastHLine(cx - hw, y, 2 * hw, fill);
    gfx->drawPixel(cx - hw, y, COL_WOOD);          // douves
    gfx->drawPixel(cx + hw - 1, y, COL_WOOD);
  }

  // Les deux cerclages à égale distance des extrémités : un cerclage haut à 20
  // et un cerclage bas à 30 se voyait comme un défaut de dessin.
  drawHoop(20);
  drawHoop(BARREL_H - 28);

  // Le fond du haut, qui donne le volume au dessin.
  const int hw0 = barrelHalfWidth(0);
  gfx->fillEllipse(cx, BARREL_TOP, hw0 - 2, 6, COL_WOOD_D);
  gfx->drawEllipse(cx, BARREL_TOP, hw0 - 2, 6, COL_WOOD);
}

// --- pages ----------------------------------------------------------------

void drawHeader(bool force) {
  const bool wifi = (WiFi.status() == WL_CONNECTED);
  const char* nm = Store::s().name;
  if (!force && wifi == shown.wifi && strcmp(nm, shown.name) == 0) return;
  shown.wifi = wifi;
  strlcpy(shown.name, nm, sizeof(shown.name));

  gfx->fillRect(0, 0, SCREEN_W, HEADER_H, COL_BG);
  gfx->setFont(u8g2_font_helvB12_tf);
  gfx->setTextColor(COL_INK);
  gfx->setCursor(8, 17);
  // Au-delà d'une vingtaine de caractères, le nom irait mordre la pastille
  // d'état à droite.
  char shortName[19];
  strlcpy(shortName, nm, sizeof(shortName));
  gfx->print(shortName);
  gfx->fillCircle(SCREEN_W - 10, 10, 4, wifi ? COL_TEAL : COL_DANGER);
  gfx->drawFastHLine(0, HEADER_H - 2, SCREEN_W, COL_LINE);
}

void drawGaugeBar(float pct) {
  gfx->fillRect(INFO_X, GAUGE_Y, INFO_W, 11, COL_BG);
  gfx->drawRoundRect(INFO_X, GAUGE_Y, INFO_W, 11, 4, COL_LINE);
  if (pct < 0) return;
  int inner = INFO_W - 4;
  int fill = (int)roundf(inner * (pct > 100 ? 100 : pct) / 100.0f);
  if (fill > 0) gfx->fillRoundRect(INFO_X + 2, GAUGE_Y + 2, fill, 7, 3, COL_AMBER);
}

void drawSetupMain(bool force, bool wifiChanged) {
  const bool wifi = WiFi.status() == WL_CONNECTED;
  if (!force && !wifiChanged && shown.netDrawn) return;
  shown.netDrawn = true;

  gfx->fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, COL_BG);

  if (!wifi) {
    regionText(14, 48, 292, 22, u8g2_font_helvB12_tf, COL_INK,
               "Connexion Wi-Fi…", 0);
    regionText(14, 82, 292, 18, u8g2_font_6x12_tf, COL_INK2,
               "Sinon, rejoins " AP_NAME, 0);
    return;
  }

  String ip = WiFi.localIP().toString();
  char url[48];
  snprintf(url, sizeof(url), "http://%s", ip.c_str());
  drawQr(url, 28, 3);

  regionText(151, 37, 160, 22, u8g2_font_helvB12_tf, COL_AMBER,
             "Configurer", 0);
  regionText(151, 59, 160, 22, u8g2_font_helvB12_tf, COL_AMBER,
             "la balance", 0);
  regionText(151, 91, 160, 18, u8g2_font_6x12_tf, COL_INK2,
             "Scanne avec le téléphone", 0);
  regionText(151, 119, 160, 18, u8g2_font_6x12_tf, COL_INK3,
             ip.c_str(), 0);
}

void drawMain(bool force) {
  const float pct = Barrel::percent();
  const int pct10 = (pct < 0) ? -10 : (int)roundf(pct * 10);
  const int ml = (int)roundf(Barrel::volumeMl());
  const int32_t days = Barrel::agingDays();
  const bool st = Barrel::stable();
  char buf[32];

  const bool setup = !Barrel::initialized();
  const bool setupChanged = setup != shown.setup;
  if (setupChanged) {
    shown.setup = setup;
    shown.netDrawn = false;
    shown.pct10 = INT32_MIN;
    shown.ml = -1;
    shown.days = INT32_MIN;
    gfx->fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, COL_BG);
    force = true;
  }

  const bool wifiChanged = shown.wifi != (WiFi.status() == WL_CONNECTED);
  drawHeader(force);

  // Tant que la séquence tare / calibration / tonneau vide n'est pas terminée,
  // l'action utile est d'ouvrir l'app, pas d'afficher quatre états techniques.
  if (setup) {
    drawSetupMain(force, wifiChanged);
    return;
  }

  if (force) {
    gfx->drawFastVLine(LEFT_W, HEADER_H + 6, SCREEN_H - HEADER_H - 12, COL_LINE);
  }

  if (force || abs(pct10 - shown.pct10) >= 5) {     // 0,5 % de changement
    drawBarrel(pct);

    if (pct < 0) {
      regionText(INFO_X, 38, INFO_W, 27, u8g2_font_helvB12_tf, COL_INK2,
                 "à initialiser", -1, 58);
    } else {
      snprintf(buf, sizeof(buf), "%d", (int)roundf(pct));
      gfx->fillRect(INFO_X, PCT_BASE - 42, INFO_W, 48, COL_BG);
      gfx->setFont(u8g2_font_logisoso38_tn);
      gfx->setTextColor(COL_INK);
      int16_t x1, y1; uint16_t w, h;
      gfx->getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
      gfx->setFont(u8g2_font_helvB14_tf);
      int16_t x2, y2; uint16_t wp, hp;
      gfx->getTextBounds("%", 0, 0, &x2, &y2, &wp, &hp);
      int x = INFO_X + (INFO_W - (int)w - 4 - (int)wp) / 2;
      gfx->setFont(u8g2_font_logisoso38_tn);
      gfx->setCursor(x, PCT_BASE);
      gfx->print(buf);
      gfx->setFont(u8g2_font_helvB14_tf);
      gfx->setTextColor(COL_INK2);
      gfx->setCursor(x + w + 4, PCT_BASE);
      gfx->print("%");
    }
    drawGaugeBar(pct);
    shown.pct10 = pct10;
  }

  if (force || ml != shown.ml) {
    if (pct < 0) {
      regionText(INFO_X, 76, INFO_W, 18, u8g2_font_6x12_tf, COL_INK3,
                 Store::s().calibrated ? "app : étape 3" : "app : étape 1", 0);
    } else {
      snprintf(buf, sizeof(buf), "%.2f L sur %.2f L",
               ml / 1000.0f, Store::s().capacityMl / 1000.0f);
      comma(buf);
      regionText(INFO_X, 76, INFO_W, 18, u8g2_font_6x12_tf, COL_INK2, buf, 0);
    }
    shown.ml = ml;
  }

  if (force || days != shown.days) {
    if (days >= 0)
      snprintf(buf, sizeof(buf), "J+%ld", (long)days);
    else if (days == Barrel::AGING_UNKNOWN)
      snprintf(buf, sizeof(buf), "en attente de l'heure");
    else
      snprintf(buf, sizeof(buf), "vieillissement à démarrer");
    regionText(INFO_X, 103, INFO_W, 20,
               days >= 0 ? u8g2_font_helvB12_tf : u8g2_font_6x12_tf,
               days >= 0 ? COL_AMBER : COL_INK3, buf, 0);
    shown.days = days;
  }

  if (force || st != shown.stable) {
    regionText(INFO_X, 126, INFO_W, 13, u8g2_font_5x8_tf, COL_INK3,
               st ? "" : "stabilisation en cours…", 0);
    shown.stable = st;
  }
}

void drawAging(bool force) {
  if (!force && Barrel::agingDays() == shown.days) return;
  shown.days = Barrel::agingDays();

  gfx->fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, COL_BG);
  drawHeader(true);
  regionText(12, 31, 132, 20, u8g2_font_helvB12_tf, COL_INK2, "Vieillissement", 0);

  char buf[40];
  if (shown.days == Barrel::AGING_UNKNOWN) {
    regionText(12, 72, 132, 22, u8g2_font_helvB12_tf, COL_INK, "en cours", 0);
    regionText(160, 57, 150, 18, u8g2_font_6x12_tf, COL_INK3, "horloge en attente", 0);
    regionText(160, 79, 150, 18, u8g2_font_6x12_tf, COL_INK3, "du retour du Wi-Fi", 0);
    return;
  }
  if (shown.days < 0) {
    regionText(12, 72, 132, 22, u8g2_font_helvB12_tf, COL_INK, "pas démarré", 0);
    regionText(160, 62, 150, 18, u8g2_font_6x12_tf, COL_INK3, "maintenir 6 s", 0);
    regionText(160, 84, 150, 18, u8g2_font_6x12_tf, COL_INK3, "pour démarrer", 0);
    return;
  }

  snprintf(buf, sizeof(buf), "%ld", (long)shown.days);
  gfx->setFont(u8g2_font_logisoso38_tn);
  gfx->setTextColor(COL_AMBER);
  int16_t x1, y1; uint16_t w, h;
  gfx->getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(78 - w / 2, 105);
  gfx->print(buf);
  regionText(12, 112, 132, 20, u8g2_font_helvB12_tf, COL_INK2,
             shown.days > 1 ? "jours" : "jour", 0);

  uint32_t start = Barrel::agingStartEpoch();
  if (start) {
    time_t t = (time_t)start;
    struct tm tmv;
    localtime_r(&t, &tmv);
    snprintf(buf, sizeof(buf), "depuis le %02d/%02d/%04d",
             tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_year + 1900);
  } else {
    snprintf(buf, sizeof(buf), "date inconnue");
  }
  regionText(160, 56, 150, 18, u8g2_font_6x12_tf, COL_INK3, buf, 0);

  float pct = Barrel::percent();
  if (pct >= 0) {
    snprintf(buf, sizeof(buf), "%.0f %% restants", pct);
    regionText(160, 86, 150, 18, u8g2_font_6x12_tf, COL_INK2, buf, 0);
  }
}

void drawQr(const char* text, int y, uint8_t scale) {
  QRCode qr;
  uint8_t version = 0;
  const uint8_t QR_MAX_VER = 4;
  const uint8_t QR_MAX_SIDE = 4 * QR_MAX_VER + 17;
  static uint8_t buf[(QR_MAX_SIDE * QR_MAX_SIDE + 7) / 8];

  for (uint8_t v = 2; v <= QR_MAX_VER; v++) {
    if (qrcode_initText(&qr, buf, v, ECC_LOW, text) == 0) { version = v; break; }
  }
  if (!version) return;

  // Un lecteur de QR attend du sombre sur clair : carré blanc, modules noirs,
  // marge de silence comprise.
  const uint8_t quiet = 2;
  const int side = (qr.size + 2 * quiet) * scale;
  const int x = 12;
  gfx->fillRect(x, y, side, side, WHITE);
  for (uint8_t my = 0; my < qr.size; my++)
    for (uint8_t mx = 0; mx < qr.size; mx++)
      if (qrcode_getModule(&qr, mx, my))
        gfx->fillRect(x + (mx + quiet) * scale, y + (my + quiet) * scale,
                      scale, scale, BLACK);
}

void drawNet(bool force) {
  if (!force && shown.netDrawn && shown.wifi == (WiFi.status() == WL_CONNECTED)) return;
  shown.netDrawn = true;

  gfx->fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, COL_BG);
  drawHeader(true);

  if (WiFi.status() != WL_CONNECTED) {
    bandText(46, 20, u8g2_font_helvB12_tf, COL_INK, "Wi-Fi absent");
    bandText(76, 18, u8g2_font_6x12_tf, COL_INK2, "Rejoins ce réseau avec ton téléphone :");
    bandText(106, 20, u8g2_font_helvB12_tf, COL_AMBER, AP_NAME);
    return;
  }

  String ip = WiFi.localIP().toString();
  char url[48];
  snprintf(url, sizeof(url), "http://%s", ip.c_str());

  drawQr(url, 28, 3);
  regionText(152, 40, 160, 18, u8g2_font_6x12_tf, COL_INK2, "Scanne pour ouvrir", 0);
  regionText(152, 68, 160, 20, u8g2_font_helvB12_tf, COL_INK, ip.c_str(), 0);
  regionText(152, 94, 160, 18, u8g2_font_6x12_tf, COL_INK3, MDNS_NAME ".local", 0);
  char rssi[24];
  snprintf(rssi, sizeof(rssi), "%d dBm", WiFi.RSSI());
  regionText(152, 118, 160, 18, u8g2_font_6x12_tf, COL_INK3, rssi, 0);
}

void drawDiag(bool force) {
  if (!force && millis() - shown.diagMs < 1000) return;
  shown.diagMs = millis();

  if (force) {
    gfx->fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, COL_BG);
    drawHeader(true);
    bandText(27, 18, u8g2_font_helvB12_tf, COL_INK2, "Diagnostic");
  }

  char buf[40];
  int col = 0, row = 0;
  auto line = [&](const char* s) {
    int x = col ? 166 : 10;
    int y = 53 + row * 22;
    regionText(x, y, 144, 18, u8g2_font_6x12_tf, COL_INK2, s);
    if (++row == 5) { row = 0; col = 1; }
  };

  snprintf(buf, sizeof(buf), "brut %.0f g", Barrel::totalG());          line(buf);
  snprintf(buf, sizeof(buf), "vide %.0f g", Store::s().emptyG);          line(buf);
  snprintf(buf, sizeof(buf), "plein %.0f g", Store::s().fullG);          line(buf);
  snprintf(buf, sizeof(buf), "%.1f counts/g", Store::s().countsPerGram); line(buf);
  snprintf(buf, sizeof(buf), "anges %.0f g", Store::s().evaporatedG);    line(buf);
  snprintf(buf, sizeof(buf), "HX A:%s  B:%s",
           Scale::channelOk(0) ? "ok" : "muet",
           NUM_CHANNELS > 1 ? (Scale::channelOk(1) ? "ok" : "muet") : "n/a");
  line(buf);
  snprintf(buf, sizeof(buf), "telegram %s%s",
           Notify::configured() ? "actif" : "inactif",
           Notify::lastError() ? " (erreur)" : "");
  line(buf);
  snprintf(buf, sizeof(buf), "heure %s", Barrel::timeValid() ? "synchro" : "absente");
  line(buf);
  snprintf(buf, sizeof(buf), "%lu h en marche", (unsigned long)(millis() / 3600000UL));
  line(buf);
}

void drawResetPrompt(bool force) {
  const int hp = (int)Button::holdProgress();
  if (force) {
    gfx->fillScreen(COL_BG);
    bandText(34, 22, u8g2_font_helvB12_tf, COL_INK, "Remettre à zéro le vieillissement ?");
    bandText(68, 18, u8g2_font_6x12_tf, COL_INK3, "Maintiens pour confirmer — relâche pour annuler");
  }
  if (force || hp != shown.holdPct) {
    shown.holdPct = hp;
    gfx->drawRoundRect(36, 112, SCREEN_W - 72, 16, 5, COL_LINE);
    int inner = SCREEN_W - 72 - 4;
    int fill = (int)roundf(inner * hp / 100.0f);
    gfx->fillRect(38, 114, inner, 12, COL_BG);
    if (fill > 0) gfx->fillRoundRect(38, 114, fill, 12, 4, COL_DANGER);
  }
}

void drawResetDone() {
  gfx->fillScreen(COL_BG);
  bandText(48, 24, u8g2_font_helvB14_tf, COL_AMBER, "Vieillissement redémarré");
  bandText(86, 22, u8g2_font_helvB12_tf, COL_INK2, "J+0");
}

void drawPortal() {
  gfx->fillScreen(COL_BG);
  bandText(30, 22, u8g2_font_helvB14_tf, COL_INK, "Configuration Wi-Fi");
  bandText(65, 18, u8g2_font_6x12_tf, COL_INK2, "Rejoins ce réseau avec ton téléphone :");
  bandText(91, 24, u8g2_font_helvB14_tf, COL_AMBER, AP_NAME);
  bandText(126, 18, u8g2_font_6x12_tf, COL_INK3, "La page s'ouvre toute seule.");
}

void render() {
  const bool prompt = Button::armed();
  const bool done = resetDoneAtMs && (millis() - resetDoneAtMs < 3000);

  // Tout changement de contexte repart d'un écran propre : c'est le seul moment
  // où l'on se permet un effacement complet.
  const bool ctxChanged = (portalMode != shown.portal) || (prompt != shown.prompt)
                       || (done != shown.done) || (page != shown.page);
  if (ctxChanged) {
    shown.portal = portalMode;
    shown.prompt = prompt;
    shown.done = done;
    shown.page = page;
    shown.pct10 = INT32_MIN;
    shown.ml = -1;
    shown.days = INT32_MIN;
    shown.holdPct = -1;
    shown.netDrawn = false;
    shown.name[0] = '\x01';
    if (!prompt) gfx->fillScreen(COL_BG);
  }

  if (portalMode)    { if (ctxChanged) drawPortal(); return; }
  if (done)          { if (ctxChanged) drawResetDone(); return; }
  if (prompt)        { drawResetPrompt(ctxChanged); return; }

  switch (page) {
    case PAGE_AGING: drawAging(ctxChanged); break;
    case PAGE_NET:   drawNet(ctxChanged);   break;
    case PAGE_DIAG:  drawDiag(ctxChanged);  break;
    default:         drawMain(ctxChanged);  break;
  }
}

void doAgingReset() {
  int32_t prev = Barrel::agingDays();
  Barrel::resetAging();
  Notify::agingReset(prev);
  resetDoneAtMs = millis();
  page = PAGE_AGING;
  lastPageChangeMs = millis();
}

} // namespace

void Ui::begin() {
  Button::begin();
  lastDisplayActivityMs = millis();

  // Rétroéclairage en PWM : à pleine puissance il chauffe, et la chaleur fait
  // dériver les cellules de charge juste en dessous.
  ledcSetup(0, 5000, 8);
  ledcAttachPin(PIN_TFT_BL, 0);
  ledcWrite(0, TFT_BACKLIGHT);

  bus = new Arduino_ESP32SPI(PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_SCK,
                             PIN_TFT_MOSI, GFX_NOT_DEFINED);
  gfx = new Arduino_ST7789(bus, PIN_TFT_RST, 1 /*rotation paysage*/, true /*IPS*/,
                           TFT_W, TFT_H,
                           TFT_COL_OFFSET, TFT_ROW_OFFSET,
                           TFT_COL_OFFSET, TFT_ROW_OFFSET);

  tftOk = gfx->begin(40000000);
  if (!tftOk) {
    Serial.println(F("[ui] écran ST7789 injoignable — on continue sans"));
    return;
  }
  // Arduino_GFX désactive le décodage UTF-8 par défaut. Sans ceci, « é »,
  // « à » et les autres caractères français sont interprétés comme deux
  // glyphes séparés malgré l'utilisation de polices U8g2 Unicode.
  gfx->setUTF8Print(true);
  initPalette();
  gfx->fillScreen(COL_BG);
  Serial.println(F("[ui] écran ST7789 prêt"));
}

bool Ui::displayPresent() { return tftOk; }

void Ui::showBootMessage(const char* line1, const char* line2) {
  if (!tftOk) return;
  gfx->fillScreen(COL_BG);
  bandText(58, 26, u8g2_font_helvB14_tf, COL_AMBER, line1);
  if (line2) bandText(92, 20, u8g2_font_6x12_tf, COL_INK2, line2);
}

void Ui::showPortalScreen(const char* apName) {
  strlcpy(portalAp, apName, sizeof(portalAp));
  portalMode = true;
  if (tftOk) render();
}

void Ui::loop() {
  Button::update();
  updateTapWake();

  if (Button::takeShortPress()) {
    if (!displayAwake) {
      setDisplayAwake(true);
    } else {
      page = (page + 1) % PAGE_COUNT;
      lastPageChangeMs = millis();
      lastDisplayActivityMs = millis();
      resetDoneAtMs = 0;
    }
  }
  if (Button::takeHoldFired()) {
    setDisplayAwake(true);
    lastDisplayActivityMs = millis();
    doAgingReset();
  }

  if (portalMode && WiFi.status() == WL_CONNECTED) portalMode = false;

  if (!portalMode && page != PAGE_MAIN &&
      millis() - lastPageChangeMs > OLED_PAGE_TIMEOUT_MS)
    page = PAGE_MAIN;

  if (!tftOk) return;

  const uint16_t sleepMin = Store::displaySleepMinutes();
  if (!sleepMin && !displayAwake) {
    setDisplayAwake(true);
  } else if (sleepMin && displayAwake && !portalMode && !Button::armed() &&
             millis() - lastDisplayActivityMs >= (uint32_t)sleepMin * 60000UL) {
    setDisplayAwake(false);
  }

  if (!displayAwake) return;

  static uint32_t lastDrawMs = 0;
  if (millis() - lastDrawMs < 120) return;   // ~8 images/s, suffisant
  lastDrawMs = millis();
  render();
}

#endif  // BOARD_TFT_ST7789
