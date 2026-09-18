#include <unity.h>
#include <string.h>
#include "mqtt_payload.h"

void setUp() {}
void tearDown() {}

void test_topic() {
  Config c = defaultConfig();
  char buf[MQTT_TOPIC_SIZE];
  TEST_ASSERT_TRUE(buildStateTopic(c, buf, sizeof buf) > 0);
  TEST_ASSERT_EQUAL_STRING("soil/sensor1/state", buf);
  strcpy(c.topicPrefix, "haus/garten");
  strcpy(c.deviceName, "beet2");
  buildStateTopic(c, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("haus/garten/beet2/state", buf);
}

void test_payload_without_test_flag() {
  Reading r = { 202, 54, Level::Ok };
  char buf[MQTT_PAYLOAD_SIZE];
  TEST_ASSERT_TRUE(buildStatePayload(r, -61, false, buf, sizeof buf) > 0);
  TEST_ASSERT_EQUAL_STRING("{\"raw\":202,\"percent\":54,\"level\":\"ok\",\"rssi\":-61}", buf);
}

void test_payload_with_test_flag() {
  Reading r = { 218, 18, Level::Dry };
  char buf[MQTT_PAYLOAD_SIZE];
  buildStatePayload(r, -70, true, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("{\"raw\":218,\"percent\":18,\"level\":\"dry\",\"rssi\":-70,\"test\":true}", buf);
}

void test_topic_max_length() {
  Config c = defaultConfig();
  memset(c.topicPrefix, 'p', sizeof c.topicPrefix - 1); c.topicPrefix[sizeof c.topicPrefix - 1] = '\0';
  memset(c.deviceName, 'n', sizeof c.deviceName - 1); c.deviceName[sizeof c.deviceName - 1] = '\0';
  char buf[MQTT_TOPIC_SIZE];
  size_t len = buildStateTopic(c, buf, sizeof buf);
  TEST_ASSERT_EQUAL_UINT(71, len);
  TEST_ASSERT_TRUE(len < MQTT_TOPIC_SIZE);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_topic);
  RUN_TEST(test_payload_without_test_flag);
  RUN_TEST(test_payload_with_test_flag);
  RUN_TEST(test_topic_max_length);
  return UNITY_END();
}
