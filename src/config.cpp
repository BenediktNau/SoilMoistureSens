#include "config.h"
#include "ipv4.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

static void setStr(char* dst, size_t n, const char* src) {
  snprintf(dst, n, "%s", src ? src : "");
}

static int clampInt(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

Config defaultConfig() {
  Config c;
  memset(&c, 0, sizeof c);
  c.mqttPort = 1883;
  setStr(c.subnet, sizeof c.subnet, "255.255.255.0");
  setStr(c.topicPrefix, sizeof c.topicPrefix, "soil");
  setStr(c.deviceName, sizeof c.deviceName, "sensor1");
  c.dryRaw = 226;
  c.wetRaw = 181;
  c.dryBelowPct = 30;
  c.wetAbovePct = 70;
  c.intervalMin = 15;
  return c;
}

bool configIsValid(const Config& c) {
  return c.ssid[0] != '\0' && c.mqttHost[0] != '\0';
}

// Uebernimmt einen JSON-String in dst, wenn er vorhanden ist.
// keepIfEmpty: leerer String laesst dst unveraendert (fuer Passwoerter).
static void readStr(JsonVariantConst v, char* dst, size_t n, bool keepIfEmpty) {
  if (!v.is<const char*>()) return;
  const char* s = v.as<const char*>();
  if (s == nullptr) return;
  if (keepIfEmpty && s[0] == '\0') return;
  setStr(dst, n, s);
}

static void readInt(JsonVariantConst v, int& dst) {
  if (v.is<int>()) dst = v.as<int>();
}

bool parseConfig(const char* json, Config& io) {
  if (json == nullptr) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err || !doc.is<JsonObjectConst>()) return false;
  JsonObjectConst o = doc.as<JsonObjectConst>();

  readStr(o["ssid"],         io.ssid,         sizeof io.ssid,         false);
  readStr(o["wifiPassword"], io.wifiPassword, sizeof io.wifiPassword, true);
  readStr(o["staticIp"],     io.staticIp,     sizeof io.staticIp,     false);
  readStr(o["gateway"],      io.gateway,      sizeof io.gateway,      false);
  readStr(o["subnet"],       io.subnet,       sizeof io.subnet,       false);
  readStr(o["dns"],          io.dns,          sizeof io.dns,          false);
  readStr(o["mqttHost"],     io.mqttHost,     sizeof io.mqttHost,     false);
  readStr(o["mqttUser"],     io.mqttUser,     sizeof io.mqttUser,     false);
  readStr(o["mqttPassword"], io.mqttPassword, sizeof io.mqttPassword, true);
  readStr(o["topicPrefix"],  io.topicPrefix,  sizeof io.topicPrefix,  false);
  readStr(o["deviceName"],   io.deviceName,   sizeof io.deviceName,   false);

  int port = io.mqttPort;
  readInt(o["mqttPort"], port);
  io.mqttPort = (uint16_t)clampInt(port, 1, 65535);

  readInt(o["dryRaw"], io.dryRaw);
  readInt(o["wetRaw"], io.wetRaw);
  readInt(o["dryBelowPct"], io.dryBelowPct);
  readInt(o["wetAbovePct"], io.wetAbovePct);
  readInt(o["intervalMin"], io.intervalMin);

  io.dryBelowPct = clampInt(io.dryBelowPct, 0, 100);
  io.wetAbovePct = clampInt(io.wetAbovePct, 0, 100);
  io.intervalMin = clampInt(io.intervalMin, INTERVAL_MIN_MINUTES, INTERVAL_MAX_MINUTES);
  return true;
}

const char* validateConfig(const Config& c) {
  if (c.dryBelowPct >= c.wetAbovePct) return "Schwelle 'trocken unter' muss kleiner sein als 'nass ab'";
  if (c.deviceName[0] == '\0') return "Geraetename darf nicht leer sein";
  if (c.topicPrefix[0] == '\0') return "Topic-Praefix darf nicht leer sein";
  if (c.staticIp[0] != '\0') {
    if (!parseIpv4(c.staticIp, nullptr)) return "Statische IP ist keine gueltige IPv4-Adresse";
    if (!parseIpv4(c.gateway, nullptr)) return "Gateway fehlt oder ist keine gueltige IPv4-Adresse";
    if (!parseIpv4(c.subnet, nullptr)) return "Subnetzmaske ist keine gueltige IPv4-Adresse";
    if (c.dns[0] != '\0' && !parseIpv4(c.dns, nullptr)) return "DNS ist keine gueltige IPv4-Adresse";
  }
  return nullptr;
}

size_t serializeConfig(const Config& c, char* buf, size_t n, bool maskSecrets) {
  JsonDocument doc;
  doc["ssid"] = c.ssid;
  doc["wifiPassword"] = maskSecrets ? "" : c.wifiPassword;
  doc["staticIp"] = c.staticIp;
  doc["gateway"] = c.gateway;
  doc["subnet"] = c.subnet;
  doc["dns"] = c.dns;
  doc["mqttHost"] = c.mqttHost;
  doc["mqttPort"] = c.mqttPort;
  doc["mqttUser"] = c.mqttUser;
  doc["mqttPassword"] = maskSecrets ? "" : c.mqttPassword;
  doc["topicPrefix"] = c.topicPrefix;
  doc["deviceName"] = c.deviceName;
  doc["dryRaw"] = c.dryRaw;
  doc["wetRaw"] = c.wetRaw;
  doc["dryBelowPct"] = c.dryBelowPct;
  doc["wetAbovePct"] = c.wetAbovePct;
  doc["intervalMin"] = c.intervalMin;
  if (maskSecrets) {
    doc["wifiPasswordSet"] = c.wifiPassword[0] != '\0';
    doc["mqttPasswordSet"] = c.mqttPassword[0] != '\0';
  }
  size_t need = measureJson(doc);
  if (need + 1 > n) return 0;
  return serializeJson(doc, buf, n);
}

#ifdef ARDUINO
#include <Arduino.h>
#include <LittleFS.h>

static const char* CONFIG_PATH = "/config.json";

bool loadConfig(Config& io) {
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) {
    Serial.println("Keine config.json, Standardwerte");
    return false;
  }
  char buf[CONFIG_JSON_SIZE];
  size_t n = f.readBytes(buf, sizeof buf - 1);
  buf[n] = '\0';
  f.close();
  if (!parseConfig(buf, io)) {
    Serial.println("config.json unlesbar, Standardwerte");
    return false;
  }
  return true;
}

bool saveConfig(const Config& c) {
  char buf[CONFIG_JSON_SIZE];
  size_t n = serializeConfig(c, buf, sizeof buf, false);
  if (n == 0) return false;
  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) {
    Serial.println("config.json kann nicht geschrieben werden");
    return false;
  }
  size_t written = f.write((const uint8_t*)buf, n);
  f.close();
  return written == n;
}
#endif
