#include <unity.h>
#include "boot_mode.h"

void setUp() {}
void tearDown() {}

void test_timer_wakeup_measures() {
  TEST_ASSERT_EQUAL(BootMode::Measure, chooseBootMode(RST_REASON_DEEP_SLEEP_AWAKE, true, false));
  TEST_ASSERT_EQUAL(BootMode::Measure, chooseBootMode(RST_REASON_DEEP_SLEEP_AWAKE, false, false));
  // Ein Doppel-Reset-Flag kann beim Timer-Wakeup nicht gesetzt sein, wird aber ignoriert.
  TEST_ASSERT_EQUAL(BootMode::Measure, chooseBootMode(RST_REASON_DEEP_SLEEP_AWAKE, true, true));
}

void test_single_reset_behaves_like_cold_start() {
  TEST_ASSERT_EQUAL(BootMode::Measure, chooseBootMode(RST_REASON_EXT_SYS, true, false));
  TEST_ASSERT_EQUAL(BootMode::Configure, chooseBootMode(RST_REASON_EXT_SYS, false, false));
}

void test_double_reset_configures() {
  TEST_ASSERT_EQUAL(BootMode::Configure, chooseBootMode(RST_REASON_EXT_SYS, true, true));
  TEST_ASSERT_EQUAL(BootMode::Configure, chooseBootMode(RST_REASON_EXT_SYS, false, true));
}

void test_other_reasons_depend_on_config() {
  const uint32_t powerOn = 0, softRestart = 4, watchdog = 1;
  TEST_ASSERT_EQUAL(BootMode::Measure, chooseBootMode(powerOn, true, false));
  TEST_ASSERT_EQUAL(BootMode::Configure, chooseBootMode(powerOn, false, false));
  TEST_ASSERT_EQUAL(BootMode::Measure, chooseBootMode(softRestart, true, false));
  TEST_ASSERT_EQUAL(BootMode::Configure, chooseBootMode(watchdog, false, false));
  // Flag nur bei Reset-Taster relevant.
  TEST_ASSERT_EQUAL(BootMode::Measure, chooseBootMode(powerOn, true, true));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_timer_wakeup_measures);
  RUN_TEST(test_single_reset_behaves_like_cold_start);
  RUN_TEST(test_double_reset_configures);
  RUN_TEST(test_other_reasons_depend_on_config);
  return UNITY_END();
}
