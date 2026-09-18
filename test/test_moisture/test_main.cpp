#include <unity.h>
#include "moisture.h"

void setUp() {}
void tearDown() {}

void test_percent_at_dry_is_zero() {
  TEST_ASSERT_EQUAL_INT(0, moisturePercent(226, 226, 181));
}

void test_percent_at_wet_is_hundred() {
  TEST_ASSERT_EQUAL_INT(100, moisturePercent(181, 226, 181));
}

void test_percent_in_between() {
  // Mitte zwischen 226 und 181 ist 203.5, Ganzzahl-Rundung liefert 51
  TEST_ASSERT_EQUAL_INT(51, moisturePercent(203, 226, 181));
}

void test_percent_is_clamped_below() {
  TEST_ASSERT_EQUAL_INT(0, moisturePercent(300, 226, 181));
}

void test_percent_is_clamped_above() {
  TEST_ASSERT_EQUAL_INT(100, moisturePercent(100, 226, 181));
}

void test_percent_with_swapped_calibration() {
  // Sensor, bei dem nass den hoeheren Rohwert hat
  TEST_ASSERT_EQUAL_INT(0, moisturePercent(100, 100, 900));
  TEST_ASSERT_EQUAL_INT(100, moisturePercent(900, 100, 900));
  TEST_ASSERT_EQUAL_INT(50, moisturePercent(500, 100, 900));
}

void test_percent_with_equal_calibration_is_zero() {
  TEST_ASSERT_EQUAL_INT(0, moisturePercent(200, 200, 200));
}

void test_classify_dry_ok_wet() {
  TEST_ASSERT_EQUAL(Level::Dry, classify(0, 30, 70));
  TEST_ASSERT_EQUAL(Level::Dry, classify(29, 30, 70));
  TEST_ASSERT_EQUAL(Level::Ok, classify(30, 30, 70));
  TEST_ASSERT_EQUAL(Level::Ok, classify(69, 30, 70));
  TEST_ASSERT_EQUAL(Level::Wet, classify(70, 30, 70));
  TEST_ASSERT_EQUAL(Level::Wet, classify(100, 30, 70));
}

void test_invalid_raw_marker() {
  // Sensor-Modul liefert RAW_INVALID, wenn der ADS1115 nicht antwortet
  TEST_ASSERT_FALSE(rawValid(RAW_INVALID));
  TEST_ASSERT_TRUE(rawValid(0));
  TEST_ASSERT_TRUE(rawValid(24000));
}

void test_level_names() {
  TEST_ASSERT_EQUAL_STRING("dry", levelName(Level::Dry));
  TEST_ASSERT_EQUAL_STRING("ok", levelName(Level::Ok));
  TEST_ASSERT_EQUAL_STRING("wet", levelName(Level::Wet));
  TEST_ASSERT_EQUAL_STRING("trocken", levelNameDe(Level::Dry));
  TEST_ASSERT_EQUAL_STRING("nass", levelNameDe(Level::Wet));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_percent_at_dry_is_zero);
  RUN_TEST(test_percent_at_wet_is_hundred);
  RUN_TEST(test_percent_in_between);
  RUN_TEST(test_percent_is_clamped_below);
  RUN_TEST(test_percent_is_clamped_above);
  RUN_TEST(test_percent_with_swapped_calibration);
  RUN_TEST(test_percent_with_equal_calibration_is_zero);
  RUN_TEST(test_classify_dry_ok_wet);
  RUN_TEST(test_invalid_raw_marker);
  RUN_TEST(test_level_names);
  return UNITY_END();
}
