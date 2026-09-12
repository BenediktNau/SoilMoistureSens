#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
extern "C" {
#include <user_interface.h>
}
#include "config.h"
#include "moisture.h"
#include "boot_mode.h"
#include "mqtt_sender.h"
#include "wifi_station.h"
#include "web_ui.h"

static const char* AP_NAME     = "SoilMoisture-Setup";
static const char* AP_PASSWORD = "bodenfeuchte";

static const unsigned long WIFI_TIMEOUT_MS   = 15000;
static const unsigned long CONFIG_TIMEOUT_MS = 5UL * 60UL * 1000UL;
static const unsigned long MEASURE_PERIOD_MS = 2000;
static const int SAMPLES = 10;   // Anzahl Messungen, die gemittelt werden

static Config cfg;

int readMoistureRaw() {
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(A0);
    delay(10);
  }
  return sum / SAMPLES;
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
  Reading r = evaluateReading(readMoistureRaw(), cfg);
  Serial.printf("Roh: %4d  Feuchte: %3d %%  %s\n", r.raw, r.percent, levelNameDe(r.level));

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
  ui.setRaw(readMoistureRaw());   // damit die Seite nicht 2 s lang Roh 0 zeigt

  unsigned long lastMeasure = 0;
  bool reportedSta = false;
  while (!ui.shouldExit()) {
    ui.handle();
    if (millis() - lastMeasure >= MEASURE_PERIOD_MS) {
      lastMeasure = millis();
      ui.setRaw(readMoistureRaw());
    }
    if (!reportedSta && WiFi.status() == WL_CONNECTED) {
      reportedSta = true;
      Serial.printf("Auch im Heimnetz erreichbar: http://%s/\n", WiFi.localIP().toString().c_str());
    }
    delay(2);
  }
  Serial.println("Konfigmodus beendet, wechsle in den Messbetrieb");
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

  uint32_t reason = ESP.getResetInfoPtr()->reason;
  BootMode mode = chooseBootMode(reason, configIsValid(cfg));
  Serial.printf("Reset-Grund %u, Konfiguration %s, Modus %s\n",
                reason, configIsValid(cfg) ? "gueltig" : "unvollstaendig", bootModeName(mode));

  if (mode == BootMode::Configure) {
    runConfigMode();
  } else {
    measureCycle();
  }
}

void loop() {
  // Alles passiert in setup(); measureCycle() endet immer im Deep Sleep.
}
