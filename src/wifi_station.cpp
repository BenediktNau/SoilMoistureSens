#include "wifi_station.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "ipv4.h"

static IPAddress toIp(const char* s) {
  uint8_t b[4] = {0, 0, 0, 0};
  parseIpv4(s, b);
  return IPAddress(b[0], b[1], b[2], b[3]);
}

bool beginStation(const Config& cfg) {
  if (cfg.ssid[0] == '\0') return false;
  if (cfg.staticIp[0] != '\0' && parseIpv4(cfg.staticIp, nullptr) &&
      parseIpv4(cfg.gateway, nullptr) && parseIpv4(cfg.subnet, nullptr)) {
    IPAddress dns = cfg.dns[0] != '\0' && parseIpv4(cfg.dns, nullptr) ? toIp(cfg.dns) : toIp(cfg.gateway);
    WiFi.config(toIp(cfg.staticIp), toIp(cfg.gateway), toIp(cfg.subnet), dns);
    Serial.printf("Statische IP %s\n", cfg.staticIp);
  } else {
    // Zurueck auf DHCP, falls vorher eine statische Adresse gesetzt war
    WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
  }
  WiFi.begin(cfg.ssid, cfg.wifiPassword);
  return true;
}
