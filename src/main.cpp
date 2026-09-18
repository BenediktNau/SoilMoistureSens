#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
extern "C" {
#include <user_interface.h>
}
#include "config.h"
#include "moisture.h"
#include "moisture_sensor.h"
#include "boot_mode.h"
#include "double_reset.h"
#include "mqtt_sender.h"
#include "wifi_station.h"
#include "web_ui.h"

static const char* AP_NAME     = "SoilMoisture-Setup";
static const char* AP_PASSWORD = "bodenfeuchte";

static const unsigned long WIFI_TIMEOUT_MS   = 15000;
static const unsigned long CONFIG_TIMEOUT_MS = 5UL * 60UL * 1000UL;
static const unsigned long MEASURE_PERIOD_MS = 2000;

static Config cfg;

// Sensor kurz einschalten, messen, wieder ausschalten.
static int measureOnce() {
  moistureSensorPower(true);
  int raw = moistureSensorRead();
  moistureSensorPower(false);
  return raw;
}

// Verbindet mit dem gespeicherten WLAN, blockiert hoechstens timeoutMs.
static bool connectStation(unsigned long timeoutMs) {
  if (!beginStation(cfg)) return false;
  Serial.printf("Verbinde mit '%s'", cfg.ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(100);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("Verbunden, IP %s, RSSI %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

// Kehrt nicht zurueck. Wakeup ueber GPIO16 -> RST.
[[noreturn]] static void goToSleep() {
  uint64_t us = (uint64_t)cfg.intervalMin * 60ULL * 1000000ULL;
  uint64_t maxUs = ESP.deepSleepMax();
  if (us > maxUs) {
    Serial.printf("Intervall auf Maximum begrenzt (%lu s)\n", (unsigned long)(maxUs / 1000000ULL));
    us = maxUs;
  }
  Serial.printf("Deep Sleep fuer %d Minuten (wach seit %lu ms)\n", cfg.intervalMin, millis());
  Serial.flush();
  WiFi.mode(WIFI_OFF);
  ESP.deepSleep(us);
  for (;;) delay(1000);   // deepSleep braucht einen Moment; kehrt nie wirklich zurueck
}

// Messen, senden, schlafen. Jeder Fehler fuehrt trotzdem zum Schlafen.
static void measureCycle() {
  int raw = measureOnce();
  if (!rawValid(raw)) {
    Serial.println("Kein Messwert, ADS1115 antwortet nicht");
    goToSleep();
  }
  Reading r = evaluateReading(raw, cfg);
  Serial.printf("Roh: %5d  Feuchte: %3d %%  %s\n", r.raw, r.percent, levelNameDe(r.level));

  if (!configIsValid(cfg)) {
    Serial.println("Keine gueltige Konfiguration, nichts zu senden");
    goToSleep();
  }
  WiFi.mode(WIFI_STA);
  if (!connectStation(WIFI_TIMEOUT_MS)) {
    Serial.println("WLAN nicht erreichbar");
    goToSleep();
  }
  char err[96];
  if (!mqttPublishReading(cfg, r, WiFi.RSSI(), false, err, sizeof err)) {
    Serial.println(err);
  } else {
    Serial.println("Gesendet");
  }
  goToSleep();
}

// Access Point plus Heimnetz, Webserver, bis Timeout oder /api/sleep.
static void runConfigMode() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_NAME, AP_PASSWORD);
  beginStation(cfg);   // nicht blockierend, Erfolg egal
  Serial.printf("Konfigmodus: AP '%s', Passwort '%s', http://%s/\n",
                AP_NAME, AP_PASSWORD, WiFi.softAPIP().toString().c_str());

  WebUi ui(cfg, CONFIG_TIMEOUT_MS);
  ui.begin();
  moistureSensorPower(true);       // im Konfigmodus bleibt der Sensor an, Live-Anzeige
  ui.setRaw(moistureSensorRead()); // damit die Seite nicht 2 s lang Roh 0 zeigt

  unsigned long lastMeasure = 0;
  bool reportedSta = false;
  while (!ui.shouldExit()) {
    ui.handle();
    if (millis() - lastMeasure >= MEASURE_PERIOD_MS) {
      lastMeasure = millis();
      ui.setRaw(moistureSensorRead());
    }
    if (!reportedSta && WiFi.status() == WL_CONNECTED) {
      reportedSta = true;
      Serial.printf("Auch im Heimnetz erreichbar: http://%s/\n", WiFi.localIP().toString().c_str());
    }
    delay(2);
  }
  Serial.println("Konfigmodus beendet, wechsle in den Messbetrieb");
  moistureSensorPower(false);
  WiFi.softAPdisconnect(true);
  measureCycle();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  WiFi.persistent(false);   // Zugangsdaten nur aus config.json, kein Flash-Schreiben durch den Core

  if (!LittleFS.begin()) {
    Serial.println("LittleFS konnte nicht gemountet werden");
  }
  cfg = defaultConfig();
  loadConfig(cfg);

  if (!moistureSensorBegin()) {
    Serial.println("ADS1115 nicht gefunden (I2C 0x48), Messwerte bleiben ungueltig");
  }

  uint32_t reason = ESP.getResetInfoPtr()->reason;
  bool doubleReset = false;
  if (reason == RST_REASON_EXT_SYS) {
    doubleReset = doubleResetPending();
    if (!doubleReset) {
      // Erster Druck: Fenster fuer einen zweiten Druck offen halten.
      doubleResetArm();
      Serial.printf("Reset-Taster erkannt, nochmal druecken innerhalb von %lu s fuer den Konfigmodus\n",
                    DOUBLE_RESET_WINDOW_MS / 1000);
      delay(DOUBLE_RESET_WINDOW_MS);
      doubleResetDisarm();
    }
  }
  BootMode mode = chooseBootMode(reason, configIsValid(cfg), doubleReset);
  Serial.printf("Reset-Grund %u%s, Konfiguration %s, Modus %s\n",
                reason, doubleReset ? " (Doppel-Reset)" : "",
                configIsValid(cfg) ? "gueltig" : "unvollstaendig", bootModeName(mode));

  if (mode == BootMode::Configure) {
    runConfigMode();
  } else {
    measureCycle();
  }
}

// setup() kehrt nie zurueck (Deep Sleep oder Konfigmodus), loop() bleibt leer.
void loop() {}
