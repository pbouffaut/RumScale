#include "button.h"
#include "config.h"

namespace {
  bool     stable = false;      // true = enfoncé
  bool     lastRead = false;
  uint32_t changeMs = 0;
  uint32_t pressedAtMs = 0;
  bool     holdConsumed = false;   // l'appui long a agi, on attend le relâchement
  bool     isArmed = false;
  bool     shortPressPending = false;
  bool     holdFiredPending = false;
}

void Button::begin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
}

void Button::update() {
  const bool raw = (digitalRead(PIN_BUTTON) == LOW);   // pull-up : appuyé = LOW
  const uint32_t now = millis();

  if (raw != lastRead) {
    lastRead = raw;
    changeMs = now;
  } else if (raw != stable && (now - changeMs) > 30) {
    stable = raw;
    if (stable) {
      pressedAtMs = now;
      isArmed = false;
      holdConsumed = false;
    } else {
      if (!holdConsumed && (now - pressedAtMs) < 600) shortPressPending = true;
      isArmed = false;   // relâché avant la fin : annulé
    }
  }

  if (stable && !holdConsumed) {
    const uint32_t held = now - pressedAtMs;
    if (held >= AGING_RESET_HOLD_MS) isArmed = true;
    if (held >= AGING_RESET_CONFIRM_MS) {
      isArmed = false;
      holdConsumed = true;
      holdFiredPending = true;
    }
  }
}

bool Button::takeShortPress() {
  bool v = shortPressPending;
  shortPressPending = false;
  return v;
}

bool Button::takeHoldFired() {
  bool v = holdFiredPending;
  holdFiredPending = false;
  return v;
}

bool Button::armed() { return isArmed; }

float Button::holdProgress() {
  if (!isArmed) return 0.0f;
  const uint32_t span = AGING_RESET_CONFIRM_MS - AGING_RESET_HOLD_MS;
  const uint32_t done = millis() - pressedAtMs - AGING_RESET_HOLD_MS;
  if (span == 0) return 100.0f;
  float p = 100.0f * (float)done / (float)span;
  return p > 100.0f ? 100.0f : p;
}
