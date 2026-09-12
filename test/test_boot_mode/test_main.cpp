#include <unity.h>
#include "boot_mode.h"

void setUp() {}
void tearDown() {}

void test_timer_wakeup_measures_even_without_valid_config() {
  TEST_ASSERT_EQUAL(BootMode::Measure, chooseBootMode(RST_REASON_DEEP_SLEEP_AWAKE, true));
  TEST_ASSERT_EQUAL(BootMode::Measure, chooseBootMode(RST_REASON_DEEP_SLEEP_AWAKE, false));
}

void test_reset_button_configures() {
  TEST_ASSERT_EQUAL(BootMode::Configure, chooseBootMode(RST_REASON_EXT_SYS, true));
  TEST_ASSERT_EQUAL(BootMode::Configure, chooseBootMode(RST_REASON_EXT_SYS, false));
}

void test_cold_boot_depends_on_config() {
  const uint32_t powerOn = 0, softRestart = 4, watchdog = 1;
  TEST_ASSERT_EQUAL(BootMode::Measure, chooseBootMode(powerOn, true));
  TEST_ASSERT_EQUAL(BootMode::Configure, chooseBootMode(powerOn, false));
  TEST_ASSERT_EQUAL(BootMode::Measure, chooseBootMode(softRestart, true));
  TEST_ASSERT_EQUAL(BootMode::Configure, chooseBootMode(watchdog, false));
}

void test_names() {
  TEST_ASSERT_EQUAL_STRING("Messbetrieb", bootModeName(BootMode::Measure));
  TEST_ASSERT_EQUAL_STRING("Konfigmodus", bootModeName(BootMode::Configure));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_timer_wakeup_measures_even_without_valid_config);
  RUN_TEST(test_reset_button_configures);
  RUN_TEST(test_cold_boot_depends_on_config);
  RUN_TEST(test_names);
  return UNITY_END();
}
