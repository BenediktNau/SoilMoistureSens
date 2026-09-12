#pragma once
#include <stdio.h>
#include "config.h"
#include "moisture.h"

constexpr size_t MQTT_TOPIC_SIZE = 80;
constexpr size_t MQTT_PAYLOAD_SIZE = 128;

// Topic: "<topicPrefix>/<deviceName>/state"
inline size_t buildStateTopic(const Config& c, char* buf, size_t n) {
  int len = snprintf(buf, n, "%s/%s/state", c.topicPrefix, c.deviceName);
  return len < 0 ? 0 : (size_t)len;
}

// Payload: {"raw":N,"percent":N,"level":"dry|ok|wet","rssi":N}, bei test zusaetzlich ,"test":true
inline size_t buildStatePayload(const Reading& r, int rssi, bool test, char* buf, size_t n) {
  int len = snprintf(buf, n, "{\"raw\":%d,\"percent\":%d,\"level\":\"%s\",\"rssi\":%d%s}",
                     r.raw, r.percent, levelName(r.level), rssi, test ? ",\"test\":true" : "");
  return len < 0 ? 0 : (size_t)len;
}
