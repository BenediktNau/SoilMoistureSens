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

// Wandelt 4 Bytes in einen 32-Bit-Wert (Netzwerk-Reihenfolge, Byte 0 oben).
inline uint32_t ipv4ToU32(const uint8_t b[4]) {
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}
// Gueltige Netzmaske: erstes Oktett 255 und Einsen zusammenhaengend von oben.
inline bool isValidNetmask(const uint8_t b[4]) {
  if (b[0] != 255) return false;
  uint32_t m = ipv4ToU32(b);
  uint32_t inv = ~m;
  return (inv & (inv + 1)) == 0;   // inv ist 0 oder 2^k - 1
}
