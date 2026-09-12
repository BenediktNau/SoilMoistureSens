#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

const char* AP_NAME     = "SoilMoisture-Setup";
const char* AP_PASSWORD = "bodenfeuchte";
const int   PORTAL_TIMEOUT_S = 180;

const int DRY_VALUE = 226;   // Sensor an der Luft bzw. in trockener Erde
const int WET_VALUE = 181;   // Sensor im Wasserglas bzw. in nasser Erde

const int SAMPLES = 10;      // Anzahl Messungen, die gemittelt werden

void connectWifi() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);

  if (WiFi.SSID().length() == 0) {
    Serial.println("Kein WLAN gespeichert, open Access Point");
  } else {
    Serial.printf("Verbinde mit gespeichertem WLAN '%s'\n", WiFi.SSID().c_str());
  }

  // Probiert gespeicherte Zugangsdaten, sonst Portal. Blockiert bis Erfolg oder Timeout.
  if (!wm.autoConnect(AP_NAME, AP_PASSWORD)) {
    Serial.println("Keine Verbindung innerhalb des Timeouts, Neustart");
    delay(1000);
    ESP.restart();
  }

  Serial.print("Verbunden, IP: ");
  Serial.println(WiFi.localIP());
}

int readMoistureRaw() {
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(A0);
    delay(10);
  }
  return sum / SAMPLES;
}

int moisturePercent(int raw) {
  int pct = map(raw, DRY_VALUE, WET_VALUE, 0, 100);
  return constrain(pct, 0, 100);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  connectWifi();
}

void loop() {
  int raw = readMoistureRaw();
  int pct = moisturePercent(raw);

  Serial.printf("Roh: %4d  Feuchte: %3d %%  ", raw, pct);
  if (pct < 30)      Serial.println("trocken");
  else if (pct < 70) Serial.println("ok");
  else               Serial.println("nass");

  delay(2000);
}
