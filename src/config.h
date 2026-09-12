#pragma once
#include <stddef.h>
#include <stdint.h>

// Alle Einstellungen des Sensors. Quelle ist /config.json im LittleFS.
// Dieser Header ist Arduino-frei; nur loadConfig/saveConfig brauchen das Geraet.

struct Config {
  char ssid[33];
  char wifiPassword[65];
  char staticIp[16];     // leer = DHCP
  char gateway[16];
  char subnet[16];
  char dns[16];          // leer = Gateway
  char mqttHost[65];
  uint16_t mqttPort;
  char mqttUser[33];
  char mqttPassword[65];
  char topicPrefix[33];
  char deviceName[33];
  int dryRaw;
  int wetRaw;
  int dryBelowPct;
  int wetAbovePct;
  int intervalMin;
};

// Groesse eines Puffers, der serializeConfig sicher aufnimmt
constexpr size_t CONFIG_JSON_SIZE = 1024;
constexpr int INTERVAL_MIN_MINUTES = 1;
constexpr int INTERVAL_MAX_MINUTES = 180;

Config defaultConfig();

// Gueltig, wenn SSID und MQTT-Host gesetzt sind
bool configIsValid(const Config& c);

// Liest JSON ueber io. Fehlende Felder bleiben, wie sie in io stehen.
// Leere Passwort-Strings ueberschreiben nicht. Zahlen werden begrenzt.
// Liefert false bei unparsbarem JSON; io bleibt dann unveraendert.
bool parseConfig(const char* json, Config& io);

// nullptr, wenn gueltig, sonst ein deutscher Fehlertext fuer die API
const char* validateConfig(const Config& c);

// Schreibt die Konfiguration als JSON nach buf. Bei maskSecrets werden die
// Passwoerter leer ausgegeben und wifiPasswordSet/mqttPasswordSet ergaenzt.
// Rueckgabe: Laenge, oder 0, wenn buf zu klein ist.
size_t serializeConfig(const Config& c, char* buf, size_t n, bool maskSecrets);

// Nur auf dem Geraet (LittleFS). Implementierung in config.cpp unter #ifdef ARDUINO.
bool loadConfig(Config& io);          // false, wenn Datei fehlt oder unlesbar; io behaelt Standardwerte
bool saveConfig(const Config& c);     // false bei Schreibfehler
