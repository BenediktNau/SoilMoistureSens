#include "web_ui.h"
#include <ESP8266WiFi.h>
#include "index_html.h"
#include "moisture.h"
#include "mqtt_payload.h"
#include "mqtt_sender.h"
#include "wifi_station.h"

static const char* JSON_TYPE = "application/json";
static const char* TEXT_TYPE = "text/plain; charset=utf-8";

WebUi::WebUi(Config& cfg, unsigned long idleTimeoutMs)
  : cfg_(cfg), server_(80), idleTimeoutMs_(idleTimeoutMs),
    lastRequestMs_(0), raw_(0), sleepRequested_(false) {}

void WebUi::begin() {
  lastRequestMs_ = millis();
  server_.on("/", HTTP_GET, [this] { handleRoot(); });
  server_.on("/api/status", HTTP_GET, [this] { handleStatus(); });
  server_.on("/api/config", HTTP_GET, [this] { handleGetConfig(); });
  server_.on("/api/config", HTTP_POST, [this] { handlePostConfig(); });
  server_.on("/api/scan", HTTP_GET, [this] { handleScan(); });
  server_.on("/api/mqtt-test", HTTP_POST, [this] { handleMqttTest(); });
  server_.on("/api/sleep", HTTP_POST, [this] { handleSleep(); });
  server_.onNotFound([this] { server_.send(404, TEXT_TYPE, "Nicht gefunden"); });
  server_.begin();
  Serial.println("Webserver auf Port 80 gestartet");
}

void WebUi::handle() { server_.handleClient(); }

void WebUi::setRaw(int raw) { raw_ = raw; }

bool WebUi::shouldExit() const {
  return sleepRequested_ || (millis() - lastRequestMs_) >= idleTimeoutMs_;
}

int WebUi::secondsLeft() const {
  unsigned long elapsed = millis() - lastRequestMs_;
  if (elapsed >= idleTimeoutMs_) return 0;
  return (int)((idleTimeoutMs_ - elapsed) / 1000UL);
}

void WebUi::touch() { lastRequestMs_ = millis(); }

void WebUi::handleRoot() {
  touch();
  server_.send_P(200, PSTR("text/html; charset=utf-8"), INDEX_HTML);
}

void WebUi::handleStatus() {
  // Absichtlich kein touch(): das Polling der Seite haelt den Konfigmodus nicht wach.
  String staIp = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String();
  char buf[192];
  snprintf(buf, sizeof buf,
           "{\"raw\":%d,\"rssi\":%d,\"staIp\":\"%s\",\"apIp\":\"%s\",\"secondsLeft\":%d}",
           raw_, WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0,
           staIp.c_str(), WiFi.softAPIP().toString().c_str(), secondsLeft());
  server_.send(200, JSON_TYPE, buf);
}

void WebUi::handleGetConfig() {
  touch();
  char buf[CONFIG_JSON_SIZE];
  if (serializeConfig(cfg_, buf, sizeof buf, true) == 0) {
    server_.send(500, TEXT_TYPE, "Konfiguration zu gross");
    return;
  }
  server_.send(200, JSON_TYPE, buf);
}

void WebUi::handlePostConfig() {
  touch();
  Config next = cfg_;
  if (!parseConfig(server_.arg("plain").c_str(), next)) {
    server_.send(400, TEXT_TYPE, "Ungueltiges JSON");
    return;
  }
  const char* problem = validateConfig(next);
  if (problem != nullptr) {
    server_.send(400, TEXT_TYPE, problem);
    return;
  }
  if (!saveConfig(next)) {
    server_.send(500, TEXT_TYPE, "Speichern fehlgeschlagen");
    return;
  }
  bool wifiChanged = strcmp(cfg_.ssid, next.ssid) != 0 ||
                     strcmp(cfg_.wifiPassword, next.wifiPassword) != 0 ||
                     strcmp(cfg_.staticIp, next.staticIp) != 0 ||
                     strcmp(cfg_.gateway, next.gateway) != 0 ||
                     strcmp(cfg_.subnet, next.subnet) != 0 ||
                     strcmp(cfg_.dns, next.dns) != 0;
  cfg_ = next;
  if (wifiChanged) {
    // Neue Zugangsdaten oder Adresse sofort probieren, nicht blockierend
    WiFi.disconnect();
    beginStation(cfg_);
  }
  Serial.println("Konfiguration gespeichert");
  server_.send(200, TEXT_TYPE, "ok");
}

void WebUi::handleScan() {
  touch();
  int n = WiFi.scanNetworks();
  if (n < 0) n = 0;
  // Nach Signalstaerke sortieren (kleine Liste, einfache Auswahl reicht)
  int idx[32];
  int count = n < 32 ? n : 32;
  for (int i = 0; i < count; i++) idx[i] = i;
  for (int i = 1; i < count; i++) {
    int k = idx[i], j = i - 1;
    while (j >= 0 && WiFi.RSSI(idx[j]) < WiFi.RSSI(k)) { idx[j + 1] = idx[j]; j--; }
    idx[j + 1] = k;
  }
  String out = "[";
  for (int i = 0; i < count; i++) {
    int id = idx[i];
    String ssid = WiFi.SSID(id);
    if (ssid.length() == 0) continue;
    ssid.replace("\\", "\\\\");
    ssid.replace("\"", "\\\"");
    if (out.length() > 1) out += ",";
    out += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(id)) +
           ",\"secure\":" + (WiFi.encryptionType(id) == ENC_TYPE_NONE ? "false" : "true") + "}";
  }
  out += "]";
  WiFi.scanDelete();
  server_.send(200, JSON_TYPE, out);
}

void WebUi::handleMqttTest() {
  touch();
  if (WiFi.status() != WL_CONNECTED) {
    server_.send(502, TEXT_TYPE, "Kein WLAN, Testnachricht nicht moeglich");
    return;
  }
  if (cfg_.mqttHost[0] == '\0') {
    server_.send(400, TEXT_TYPE, "Kein MQTT-Broker eingetragen");
    return;
  }
  Reading r = evaluateReading(raw_, cfg_);
  char err[96];
  if (!mqttPublishReading(cfg_, r, WiFi.RSSI(), true, err, sizeof err)) {
    server_.send(502, TEXT_TYPE, err);
    return;
  }
  // Praefixtext + Topic: groesser als MQTT_TOPIC_SIZE, damit nichts abgeschnitten wird.
  char msg[MQTT_TOPIC_SIZE + 32];
  snprintf(msg, sizeof msg, "Testnachricht gesendet an %s/%s/state", cfg_.topicPrefix, cfg_.deviceName);
  server_.send(200, TEXT_TYPE, msg);
}

void WebUi::handleSleep() {
  touch();
  server_.send(200, TEXT_TYPE, "ok");
  sleepRequested_ = true;
}
