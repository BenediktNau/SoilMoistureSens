#include "mqtt_sender.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "mqtt_payload.h"

static const uint16_t MQTT_SOCKET_TIMEOUT_S = 5;

bool mqttPublishReading(const Config& cfg, const Reading& r, int rssi, bool test,
                        char* err, size_t errLen) {
  WiFiClient net;
  net.setTimeout(MQTT_SOCKET_TIMEOUT_S * 1000);
  PubSubClient mqtt(net);
  mqtt.setServer(cfg.mqttHost, cfg.mqttPort);
  mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);

  const char* user = cfg.mqttUser[0] ? cfg.mqttUser : nullptr;
  const char* pass = cfg.mqttPassword[0] ? cfg.mqttPassword : nullptr;
  if (!mqtt.connect(cfg.deviceName, user, pass)) {
    snprintf(err, errLen, "MQTT-Verbindung zu %s:%u fehlgeschlagen (Code %d)",
             cfg.mqttHost, cfg.mqttPort, mqtt.state());
    return false;
  }

  char topic[MQTT_TOPIC_SIZE];
  char payload[MQTT_PAYLOAD_SIZE];
  buildStateTopic(cfg, topic, sizeof topic);
  buildStatePayload(r, rssi, test, payload, sizeof payload);

  bool ok = mqtt.publish(topic, payload, true);
  mqtt.loop();
  mqtt.disconnect();
  if (!ok) {
    snprintf(err, errLen, "Veroeffentlichen auf %s fehlgeschlagen", topic);
    return false;
  }
  Serial.printf("MQTT %s <- %s\n", topic, payload);
  return true;
}
