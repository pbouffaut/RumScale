#pragma once
#include <Arduino.h>

// ============================================================================
//  Brochage — choisi par l'environnement PlatformIO (voir platformio.ini).
//  Mets PIN_LED à -1 si la carte n'a pas de LED simple : sur la plupart des
//  cartes ESP32-S3, la LED embarquée est une WS2812 adressable, qu'un
//  digitalWrite ne sait pas piloter.
// ============================================================================

#if defined(BOARD_TFT_ST7789)

// ideaspark ESP32-WROOM-32 avec écran couleur ST7789 1,9" 170x320.
// L'écran occupe des GPIO câblés en dur sur la carte : 23 (MOSI), 18 (SCK),
// 15 (CS), 2 (DC), 4 (RST), 32 (rétroéclairage). Il ne faut donc surtout pas
// reprendre le brochage OLED, qui utilisait 2 et 4.
  #define PIN_TFT_MOSI  23
  #define PIN_TFT_SCK   18
  #define PIN_TFT_CS    15
  #define PIN_TFT_DC     2
  #define PIN_TFT_RST    4
  #define PIN_TFT_BL    32

  #define TFT_W        170
  #define TFT_H        320
// Une dalle de 170 px de large sur un contrôleur ST7789 câblé pour 240 px doit
// être décalée. Si l'image apparaît glissée horizontalement, c'est ici que ça
// se règle — les valeurs vues en pratique sont 35 ou 0.
  #define TFT_COL_OFFSET 35
  #define TFT_ROW_OFFSET  0

// Rétroéclairage, 0..255. À fond, l'écran chauffe — et la chaleur fait dériver
// les cellules de charge posées juste en dessous. 170 reste très lisible.
  #define TFT_BACKLIGHT 170

// Le pont complet formé par les quatre demi-cellules utilise un seul HX711,
// câblé sur D25/D26. GPIO 16/17 restent disponibles pour une extension future.
  #define PIN_HX_A_DOUT 25
  #define PIN_HX_A_SCK  26
  #define PIN_HX_B_DOUT 16
  #define PIN_HX_B_SCK  17

  #define PIN_BUTTON    27   // bouton vers GND, pull-up interne
  #define PIN_LED       -1   // pas de LED libre sur cette carte

#elif defined(BOARD_S3)

// ESP32-S3 nu (Waveshare ESP32-S3-DEV-KIT, Freenove ESP32-S3-WROOM).
// À éviter absolument sur ces modules : GPIO 26..32 (flash interne) et
// GPIO 33..37, mangés par la PSRAM octale des versions « R8 ». GPIO 19/20 sont
// l'USB natif, 43/44 la console série, 0/45/46 des straps de boot.
  #define PIN_HX_A_DOUT  4
  #define PIN_HX_A_SCK   5
  #define PIN_HX_B_DOUT  6
  #define PIN_HX_B_SCK   7

  #define PIN_I2C_SDA    8
  #define PIN_I2C_SCL    9

  #define PIN_BUTTON    10   // bouton vers GND, pull-up interne
  #define PIN_LED       -1   // LED embarquée adressable : on ne s'en sert pas

#else

// ESP32 WROOM-32 DevKit.
// À éviter : GPIO 6..11 (flash interne), GPIO 0 / 2 / 12 / 15 (straps de boot).
// Les GPIO 34..39 sont en entrée seule : parfaits pour DOUT, inutilisables pour SCK.
  #define PIN_HX_A_DOUT 16
  #define PIN_HX_A_SCK   4
  #define PIN_HX_B_DOUT 17
  #define PIN_HX_B_SCK   5

  #define PIN_I2C_SDA   21
  #define PIN_I2C_SCL   22

  #define PIN_BUTTON    27
  #define PIN_LED        2

#endif

// Les quatre demi-cellules forment ensemble un pont complet lu par un HX711.
#define NUM_CHANNELS   1

// ============================================================================
//  Échantillonnage et filtrage
// ============================================================================

// Le HX711 sort 10 mesures/seconde (broche RATE à la masse, réglage d'usine).
static const uint8_t MEDIAN_WINDOW = 15;     // médiane glissante, ~1,5 s
static const float   EMA_ALPHA     = 0.08f;  // lissage exponentiel derrière

// Écran : le contrôleur reste éveillé pour continuer à peser et détecter deux
// coups brefs sur la base ; seul le rétroéclairage est coupé.
static const uint32_t TAP_WINDOW_MS    = 1200;
static const uint32_t TAP_LOCKOUT_MS   = 250;
static const int32_t  TAP_MIN_COUNTS   = 5000;

// ============================================================================
//  Détection d'événements
//
//  On ne se fie jamais au poids absolu instantané : sur des mois, une cellule de
//  charge dérive (température, fluage du métal). On raisonne donc en *paliers* —
//  le poids se stabilise, on compare au palier précédent, et l'écart devient un
//  événement. Une baisse rapide est un service ; une baisse très lente est de
//  l'évaporation (la « part des anges »).
// ============================================================================

static const float    STABLE_BAND_G   = 12.0f;   // amplitude tolérée pour un palier
static const float    STABLE_EXIT_G   = 20.0f;   // hystérésis visuelle : ignore la dérive lente
static const uint32_t STABLE_MS       = 20000;   // durée de calme avant de figer le palier
static const float    EVENT_MIN_G     = 25.0f;   // plus petit écart qui mérite un événement
static const float    EVAP_MAX_G_DAY  = 25.0f;   // au-delà, ce n'est plus de l'évaporation

// Un verre posé sur la base produit une hausse puis une baisse équivalentes.
// On attend un peu avant de confirmer un événement, le temps de voir venir son
// éventuel opposé.
static const uint32_t EVENT_CONFIRM_MS = 180000UL;  // 3 minutes

// Seuils de niveau qui déclenchent une alerte à la descente (%).
static const uint8_t ALERT_THRESHOLDS[] = {50, 25, 10};

// ============================================================================
//  Historique (LittleFS)
// ============================================================================

static const uint32_t HIST_INTERVAL_S = 3600;   // un point par heure
static const uint16_t HIST_SLOTS      = 8760;   // un an glissant (70 Ko)
static const uint16_t EVENT_SLOTS     = 128;    // derniers événements conservés

// ============================================================================
//  Divers
// ============================================================================

#define AP_NAME       "RumBarrel-setup"
#define MDNS_NAME     "rum"              // -> http://rum.local
#define NTP_SERVER_1  "pool.ntp.org"
#define NTP_SERVER_2  "time.nist.gov"

// Fuseau horaire pour les dates affichées (heure d'été comprise).
// Europe/Paris par défaut ; Montréal : "EST5EDT,M3.2.0,M11.1.0"
#define TZ_INFO       "CET-1CEST,M3.5.0,M10.5.0/3"

static const uint32_t OLED_PAGE_TIMEOUT_MS = 30000;  // retour à la page principale
static const uint32_t AGING_RESET_HOLD_MS  = 3000;   // appui long : armement
static const uint32_t AGING_RESET_CONFIRM_MS = 6000; // appui long : exécution
