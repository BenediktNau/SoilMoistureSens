#pragma once

// Reine Rechenlogik ohne Arduino-Abhaengigkeit, damit sie nativ testbar ist.

enum class Level { Dry, Ok, Wet };

// Prozent 0..100 aus einem Rohwert. dryRaw und wetRaw duerfen in beliebiger
// Reihenfolge liegen. Gleiche Kalibrierwerte liefern 0 statt Division durch Null.
inline int moisturePercent(int raw, int dryRaw, int wetRaw) {
  long span = (long)wetRaw - (long)dryRaw;
  if (span == 0) return 0;
  long pct = ((long)raw - (long)dryRaw) * 100L / span;
  if (pct < 0) return 0;
  if (pct > 100) return 100;
  return (int)pct;
}

inline Level classify(int percent, int dryBelowPct, int wetAbovePct) {
  if (percent < dryBelowPct) return Level::Dry;
  if (percent >= wetAbovePct) return Level::Wet;
  return Level::Ok;
}

inline const char* levelName(Level level) {
  switch (level) {
    case Level::Dry: return "dry";
    case Level::Wet: return "wet";
    default:         return "ok";
  }
}

inline const char* levelNameDe(Level level) {
  switch (level) {
    case Level::Dry: return "trocken";
    case Level::Wet: return "nass";
    default:         return "ok";
  }
}

#include "config.h"

struct Reading {
  int raw;
  int percent;
  Level level;
};

inline Reading evaluateReading(int raw, const Config& c) {
  Reading r;
  r.raw = raw;
  r.percent = moisturePercent(raw, c.dryRaw, c.wetRaw);
  r.level = classify(r.percent, c.dryBelowPct, c.wetAbovePct);
  return r;
}
