#pragma once
#include <stdint.h>

// Entscheidet anhand des Reset-Grunds, ob gemessen oder konfiguriert wird.
// Timer-Wakeup aus Deep Sleep: messen. Doppelter Druck auf den Reset-Taster
// (zweiter Druck innerhalb des Fensters, siehe double_reset.h): Konfigmodus.
// Alles andere (einfacher Reset, Einschalten, Software-Reset, Watchdog):
// messen, wenn eine gueltige Konfiguration vorliegt, sonst Konfigmodus.

enum class BootMode { Measure, Configure };

// Werte aus rst_reason (user_interface.h des ESP8266 Non-OS SDK)
constexpr uint32_t RST_REASON_DEEP_SLEEP_AWAKE = 5;
constexpr uint32_t RST_REASON_EXT_SYS = 6;

inline BootMode chooseBootMode(uint32_t resetReason, bool configValid, bool doubleReset) {
  if (resetReason == RST_REASON_DEEP_SLEEP_AWAKE) return BootMode::Measure;
  if (resetReason == RST_REASON_EXT_SYS && doubleReset) return BootMode::Configure;
  return configValid ? BootMode::Measure : BootMode::Configure;
}

inline const char* bootModeName(BootMode m) {
  return m == BootMode::Measure ? "Messbetrieb" : "Konfigmodus";
}
