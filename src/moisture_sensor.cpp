#include "moisture_sensor.h"

#include <Arduino.h>
#include <Wire.h>
#include <ADS1X15.h>

#include "moisture.h"

static const uint8_t SENSOR_PWR_PIN = 14;          // D5 -> Gate Q1, R1 10k nach GND
static const uint8_t ADS_ADDRESS = 0x48;           // ADDR an GND
static const uint8_t ADS_CHANNEL = 0;              // Sensor AOUT an A0
static const unsigned long SENSOR_SETTLE_MS = 200; // Einschwingzeit des Sensors nach Power-on
static const int SAMPLES = 10;                     // Anzahl Wandlungen, die gemittelt werden

static ADS1115 ads(ADS_ADDRESS);
static bool adsReady = false;
static bool powered = false;

bool moistureSensorBegin() {
  pinMode(SENSOR_PWR_PIN, OUTPUT);
  digitalWrite(SENSOR_PWR_PIN, LOW);
  powered = false;

  Wire.begin();   // SDA GPIO4 (D2), SCL GPIO5 (D1)
  adsReady = ads.begin() && ads.isConnected();
  if (adsReady) {
    ads.setGain(1);       // +-4,096 V, 0,125 mV pro Schritt; Sensor (an 5 V) liefert 1,2..3 V
    ads.setMode(1);       // Single-Shot: schlaeft zwischen den Wandlungen
    ads.setDataRate(4);   // 128 SPS, rund 8 ms pro Wandlung
  }
  return adsReady;
}

void moistureSensorPower(bool on) {
  if (on == powered) return;
  digitalWrite(SENSOR_PWR_PIN, on ? HIGH : LOW);
  powered = on;
  if (on) delay(SENSOR_SETTLE_MS);
}

int moistureSensorRead() {
  if (!adsReady || !powered) return RAW_INVALID;
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    int16_t v = ads.readADC(ADS_CHANNEL);
    if (ads.getError() != ADS1X15_OK) return RAW_INVALID;
    if (v < 0) v = 0;   // leichtes Rauschen um 0 V nicht negativ werden lassen
    sum += v;
    delay(10);
  }
  return (int)(sum / SAMPLES);
}
