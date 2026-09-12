#pragma once
#include <stdint.h>

// Prueft einen IPv4-String der Form "a.b.c.d" (nur Ziffern und Punkte,
// jede Gruppe 1..3 Ziffern, Wert 0..255). out darf nullptr sein.
inline bool parseIpv4(const char* s, uint8_t out[4]) {
  if (s == nullptr || *s == '\0') return false;
  int part = 0;
  int value = 0;
  int digits = 0;
  uint8_t tmp[4] = {0, 0, 0, 0};
  for (const char* p = s;; p++) {
    char c = *p;
    if (c >= '0' && c <= '9') {
      if (++digits > 3) return false;
      value = value * 10 + (c - '0');
      if (value > 255) return false;
    } else if (c == '.' || c == '\0') {
      if (digits == 0 || part > 3) return false;
      tmp[part++] = (uint8_t)value;
      value = 0;
      digits = 0;
      if (c == '\0') break;
    } else {
      return false;
    }
  }
  if (part != 4) return false;
  if (out != nullptr) {
    for (int i = 0; i < 4; i++) out[i] = tmp[i];
  }
  return true;
}
