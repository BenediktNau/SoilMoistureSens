#pragma once
#include <ESP8266WebServer.h>
#include "config.h"

// Webserver des Konfigmodus. Bedient die Seite und die JSON-Schnittstelle.
// Jede Anfrage ausser /api/status setzt den Leerlauf-Timer zurueck.
class WebUi {
 public:
  WebUi(Config& cfg, unsigned long idleTimeoutMs);
  void begin();
  void handle();
  void setRaw(int raw);
  bool shouldExit() const;
  int secondsLeft() const;

 private:
  void touch();
  void handleRoot();
  void handleStatus();
  void handleGetConfig();
  void handlePostConfig();
  void handleScan();
  void handleMqttTest();
  void handleSleep();

  Config& cfg_;
  ESP8266WebServer server_;
  unsigned long idleTimeoutMs_;
  unsigned long lastRequestMs_;
  int raw_;
  bool sleepRequested_;
  bool wifiReconfigPending_;
  unsigned long wifiReconfigAtMs_;
};
