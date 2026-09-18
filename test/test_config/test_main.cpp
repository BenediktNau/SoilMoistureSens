#include <unity.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "moisture.h"

void setUp() {}
void tearDown() {}

void test_defaults() {
  Config c = defaultConfig();
  TEST_ASSERT_EQUAL_STRING("", c.ssid);
  TEST_ASSERT_EQUAL_STRING("", c.mqttHost);
  TEST_ASSERT_EQUAL_UINT16(1883, c.mqttPort);
  TEST_ASSERT_EQUAL_STRING("soil", c.topicPrefix);
  TEST_ASSERT_EQUAL_STRING("sensor1", c.deviceName);
  TEST_ASSERT_EQUAL_INT(24000, c.dryRaw);
  TEST_ASSERT_EQUAL_INT(9600, c.wetRaw);
  TEST_ASSERT_EQUAL_INT(30, c.dryBelowPct);
  TEST_ASSERT_EQUAL_INT(70, c.wetAbovePct);
  TEST_ASSERT_EQUAL_INT(15, c.intervalMin);
  TEST_ASSERT_EQUAL_STRING("", c.staticIp);
  TEST_ASSERT_EQUAL_STRING("", c.gateway);
  TEST_ASSERT_EQUAL_STRING("255.255.255.0", c.subnet);
  TEST_ASSERT_EQUAL_STRING("", c.dns);
  TEST_ASSERT_FALSE(configIsValid(c));
}

void test_valid_needs_ssid_and_host() {
  Config c = defaultConfig();
  strcpy(c.ssid, "Garten");
  TEST_ASSERT_FALSE(configIsValid(c));
  strcpy(c.mqttHost, "192.168.1.10");
  TEST_ASSERT_TRUE(configIsValid(c));
}

void test_parse_full_json() {
  Config c = defaultConfig();
  const char* json =
    "{\"ssid\":\"Garten\",\"wifiPassword\":\"geheim\",\"mqttHost\":\"broker\","
    "\"mqttPort\":8883,\"mqttUser\":\"u\",\"mqttPassword\":\"p\",\"topicPrefix\":\"haus\","
    "\"deviceName\":\"beet2\",\"dryRaw\":300,\"wetRaw\":150,\"dryBelowPct\":20,"
    "\"wetAbovePct\":80,\"intervalMin\":30}";
  TEST_ASSERT_TRUE(parseConfig(json, c));
  TEST_ASSERT_EQUAL_STRING("Garten", c.ssid);
  TEST_ASSERT_EQUAL_STRING("geheim", c.wifiPassword);
  TEST_ASSERT_EQUAL_STRING("broker", c.mqttHost);
  TEST_ASSERT_EQUAL_UINT16(8883, c.mqttPort);
  TEST_ASSERT_EQUAL_STRING("u", c.mqttUser);
  TEST_ASSERT_EQUAL_STRING("p", c.mqttPassword);
  TEST_ASSERT_EQUAL_STRING("haus", c.topicPrefix);
  TEST_ASSERT_EQUAL_STRING("beet2", c.deviceName);
  TEST_ASSERT_EQUAL_INT(300, c.dryRaw);
  TEST_ASSERT_EQUAL_INT(150, c.wetRaw);
  TEST_ASSERT_EQUAL_INT(20, c.dryBelowPct);
  TEST_ASSERT_EQUAL_INT(80, c.wetAbovePct);
  TEST_ASSERT_EQUAL_INT(30, c.intervalMin);
}

void test_parse_partial_json_keeps_rest() {
  Config c = defaultConfig();
  strcpy(c.ssid, "Alt");
  TEST_ASSERT_TRUE(parseConfig("{\"intervalMin\":5}", c));
  TEST_ASSERT_EQUAL_INT(5, c.intervalMin);
  TEST_ASSERT_EQUAL_STRING("Alt", c.ssid);
  TEST_ASSERT_EQUAL_INT(24000, c.dryRaw);
}

void test_parse_empty_object_and_garbage() {
  Config c = defaultConfig();
  TEST_ASSERT_TRUE(parseConfig("{}", c));
  TEST_ASSERT_EQUAL_INT(15, c.intervalMin);
  strcpy(c.ssid, "Bleibt");
  TEST_ASSERT_FALSE(parseConfig("kein json", c));
  TEST_ASSERT_EQUAL_STRING("Bleibt", c.ssid);
  TEST_ASSERT_FALSE(parseConfig("", c));
}

void test_parse_empty_password_keeps_old() {
  Config c = defaultConfig();
  strcpy(c.wifiPassword, "alt1");
  strcpy(c.mqttPassword, "alt2");
  TEST_ASSERT_TRUE(parseConfig("{\"wifiPassword\":\"\",\"mqttPassword\":\"\",\"ssid\":\"\"}", c));
  TEST_ASSERT_EQUAL_STRING("alt1", c.wifiPassword);
  TEST_ASSERT_EQUAL_STRING("alt2", c.mqttPassword);
  TEST_ASSERT_EQUAL_STRING("", c.ssid);   // nur Passwoerter sind geschuetzt
}

void test_parse_clamps_numbers() {
  Config c = defaultConfig();
  TEST_ASSERT_TRUE(parseConfig("{\"intervalMin\":0,\"dryBelowPct\":-5,\"wetAbovePct\":150}", c));
  TEST_ASSERT_EQUAL_INT(1, c.intervalMin);
  TEST_ASSERT_EQUAL_INT(0, c.dryBelowPct);
  TEST_ASSERT_EQUAL_INT(100, c.wetAbovePct);
  TEST_ASSERT_TRUE(parseConfig("{\"intervalMin\":999}", c));
  TEST_ASSERT_EQUAL_INT(180, c.intervalMin);
}

void test_parse_truncates_long_strings() {
  Config c = defaultConfig();
  char json[200];
  char longSsid[50];
  memset(longSsid, 'a', 49); longSsid[49] = '\0';
  snprintf(json, sizeof json, "{\"ssid\":\"%s\"}", longSsid);
  TEST_ASSERT_TRUE(parseConfig(json, c));
  TEST_ASSERT_EQUAL_INT(32, (int)strlen(c.ssid));
}

void test_validate_thresholds() {
  Config c = defaultConfig();
  TEST_ASSERT_NULL(validateConfig(c));
  c.dryBelowPct = 70; c.wetAbovePct = 70;
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  c.dryBelowPct = 80;
  TEST_ASSERT_NOT_NULL(validateConfig(c));
}

void test_parse_static_ip_fields() {
  Config c = defaultConfig();
  TEST_ASSERT_TRUE(parseConfig(
    "{\"staticIp\":\"192.168.1.50\",\"gateway\":\"192.168.1.1\",\"subnet\":\"255.255.0.0\",\"dns\":\"1.1.1.1\"}", c));
  TEST_ASSERT_EQUAL_STRING("192.168.1.50", c.staticIp);
  TEST_ASSERT_EQUAL_STRING("192.168.1.1", c.gateway);
  TEST_ASSERT_EQUAL_STRING("255.255.0.0", c.subnet);
  TEST_ASSERT_EQUAL_STRING("1.1.1.1", c.dns);
}

void test_validate_static_ip() {
  Config c = defaultConfig();
  // DHCP: Gateway darf beliebig sein
  strcpy(c.gateway, "unsinn");
  TEST_ASSERT_NULL(validateConfig(c));
  // statische IP ohne Gateway ist ein Fehler
  strcpy(c.staticIp, "192.168.1.50");
  strcpy(c.gateway, "");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  // ungueltige IP
  strcpy(c.gateway, "192.168.1.1");
  strcpy(c.staticIp, "192.168.1.300");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  // ungueltige Maske
  strcpy(c.staticIp, "192.168.1.50");
  strcpy(c.subnet, "255.255.255");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  // alles gueltig, DNS leer ist erlaubt
  strcpy(c.subnet, "255.255.255.0");
  TEST_ASSERT_NULL(validateConfig(c));
  // DNS gesetzt, aber ungueltig
  strcpy(c.dns, "dns.local");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  strcpy(c.dns, "");
  // Maske ist eine gueltige IPv4-Adresse, aber keine echte Netzmaske
  strcpy(c.subnet, "192.168.1.0");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  // IP und Gateway liegen in verschiedenen Subnetzen
  strcpy(c.subnet, "255.255.255.0");
  strcpy(c.staticIp, "192.168.1.50");
  strcpy(c.gateway, "192.168.2.1");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  // mit groesserer Maske liegen beide im selben Subnetz
  strcpy(c.subnet, "255.255.0.0");
  TEST_ASSERT_NULL(validateConfig(c));
}

void test_validate_requires_device_and_prefix() {
  Config c = defaultConfig();
  strcpy(c.deviceName, "");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  c = defaultConfig();
  strcpy(c.topicPrefix, "");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
}

void test_serialize_roundtrip() {
  Config c = defaultConfig();
  strcpy(c.ssid, "Garten"); strcpy(c.wifiPassword, "pw");
  strcpy(c.mqttHost, "broker"); strcpy(c.mqttPassword, "mp");
  strcpy(c.staticIp, "10.0.0.5"); strcpy(c.gateway, "10.0.0.1"); strcpy(c.dns, "10.0.0.1");
  c.intervalMin = 42;
  char buf[CONFIG_JSON_SIZE];
  size_t n = serializeConfig(c, buf, sizeof buf, false);
  TEST_ASSERT_TRUE(n > 0);
  Config back = defaultConfig();
  TEST_ASSERT_TRUE(parseConfig(buf, back));
  TEST_ASSERT_EQUAL_STRING("Garten", back.ssid);
  TEST_ASSERT_EQUAL_STRING("pw", back.wifiPassword);
  TEST_ASSERT_EQUAL_STRING("mp", back.mqttPassword);
  TEST_ASSERT_EQUAL_STRING("10.0.0.5", back.staticIp);
  TEST_ASSERT_EQUAL_STRING("10.0.0.1", back.gateway);
  TEST_ASSERT_EQUAL_STRING("255.255.255.0", back.subnet);
  TEST_ASSERT_EQUAL_STRING("10.0.0.1", back.dns);
  TEST_ASSERT_EQUAL_INT(42, back.intervalMin);
}

void test_serialize_masks_secrets() {
  Config c = defaultConfig();
  strcpy(c.wifiPassword, "pw");
  char buf[CONFIG_JSON_SIZE];
  TEST_ASSERT_TRUE(serializeConfig(c, buf, sizeof buf, true) > 0);
  TEST_ASSERT_NULL(strstr(buf, "\"pw\""));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"wifiPassword\":\"\""));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"wifiPasswordSet\":true"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"mqttPasswordSet\":false"));
}

void test_serialize_too_small_buffer_returns_zero() {
  Config c = defaultConfig();
  char buf[16];
  TEST_ASSERT_EQUAL_UINT(0, serializeConfig(c, buf, sizeof buf, false));
}

void test_masked_roundtrip_keeps_passwords() {
  Config c = defaultConfig();
  strcpy(c.ssid, "Garten");
  strcpy(c.wifiPassword, "geheim1");
  strcpy(c.mqttHost, "broker");
  strcpy(c.mqttPassword, "geheim2");
  char buf[CONFIG_JSON_SIZE];
  TEST_ASSERT_TRUE(serializeConfig(c, buf, sizeof buf, true) > 0);
  TEST_ASSERT_TRUE(parseConfig(buf, c));
  TEST_ASSERT_EQUAL_STRING("geheim1", c.wifiPassword);
  TEST_ASSERT_EQUAL_STRING("geheim2", c.mqttPassword);
  TEST_ASSERT_EQUAL_STRING("Garten", c.ssid);
  TEST_ASSERT_EQUAL_STRING("broker", c.mqttHost);
}

void test_full_config_fits_buffer() {
  Config c = defaultConfig();
  memset(c.ssid, 'a', sizeof c.ssid - 1); c.ssid[sizeof c.ssid - 1] = '\0';
  memset(c.wifiPassword, 'b', sizeof c.wifiPassword - 1); c.wifiPassword[sizeof c.wifiPassword - 1] = '\0';
  memset(c.mqttHost, 'c', sizeof c.mqttHost - 1); c.mqttHost[sizeof c.mqttHost - 1] = '\0';
  memset(c.mqttUser, 'd', sizeof c.mqttUser - 1); c.mqttUser[sizeof c.mqttUser - 1] = '\0';
  memset(c.mqttPassword, 'e', sizeof c.mqttPassword - 1); c.mqttPassword[sizeof c.mqttPassword - 1] = '\0';
  memset(c.topicPrefix, 'f', sizeof c.topicPrefix - 1); c.topicPrefix[sizeof c.topicPrefix - 1] = '\0';
  memset(c.deviceName, 'g', sizeof c.deviceName - 1); c.deviceName[sizeof c.deviceName - 1] = '\0';
  strcpy(c.staticIp, "255.255.255.255");
  strcpy(c.gateway, "255.255.255.255");
  strcpy(c.subnet, "255.255.255.255");
  strcpy(c.dns, "255.255.255.255");
  char buf[CONFIG_JSON_SIZE];
  TEST_ASSERT_TRUE(serializeConfig(c, buf, sizeof buf, false) > 0);
  TEST_ASSERT_TRUE(serializeConfig(c, buf, sizeof buf, true) > 0);
}

void test_evaluate_reading() {
  Config c = defaultConfig();
  Reading r = evaluateReading(c.dryRaw, c);
  TEST_ASSERT_EQUAL_INT(c.dryRaw, r.raw);
  TEST_ASSERT_EQUAL_INT(0, r.percent);
  TEST_ASSERT_EQUAL(Level::Dry, r.level);
  r = evaluateReading(c.wetRaw, c);
  TEST_ASSERT_EQUAL_INT(100, r.percent);
  TEST_ASSERT_EQUAL(Level::Wet, r.level);
  // Mitte der ADS1115-Spanne liegt mit den Standardschwellen im Bereich ok
  r = evaluateReading((c.dryRaw + c.wetRaw) / 2, c);
  TEST_ASSERT_EQUAL_INT(50, r.percent);
  TEST_ASSERT_EQUAL(Level::Ok, r.level);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_valid_needs_ssid_and_host);
  RUN_TEST(test_parse_full_json);
  RUN_TEST(test_parse_partial_json_keeps_rest);
  RUN_TEST(test_parse_empty_object_and_garbage);
  RUN_TEST(test_parse_empty_password_keeps_old);
  RUN_TEST(test_parse_clamps_numbers);
  RUN_TEST(test_parse_truncates_long_strings);
  RUN_TEST(test_validate_thresholds);
  RUN_TEST(test_parse_static_ip_fields);
  RUN_TEST(test_validate_static_ip);
  RUN_TEST(test_serialize_roundtrip);
  RUN_TEST(test_serialize_masks_secrets);
  RUN_TEST(test_serialize_too_small_buffer_returns_zero);
  RUN_TEST(test_masked_roundtrip_keeps_passwords);
  RUN_TEST(test_full_config_fits_buffer);
  RUN_TEST(test_validate_requires_device_and_prefix);
  RUN_TEST(test_evaluate_reading);
  return UNITY_END();
}
