#include <unity.h>
#include "ipv4.h"

void setUp() {}
void tearDown() {}

void test_valid_addresses() {
  uint8_t b[4];
  TEST_ASSERT_TRUE(parseIpv4("192.168.1.42", b));
  TEST_ASSERT_EQUAL_UINT8(192, b[0]);
  TEST_ASSERT_EQUAL_UINT8(168, b[1]);
  TEST_ASSERT_EQUAL_UINT8(1, b[2]);
  TEST_ASSERT_EQUAL_UINT8(42, b[3]);
  TEST_ASSERT_TRUE(parseIpv4("0.0.0.0", b));
  TEST_ASSERT_TRUE(parseIpv4("255.255.255.0", b));
  TEST_ASSERT_TRUE(parseIpv4("10.0.0.1", nullptr));
}

void test_invalid_addresses() {
  TEST_ASSERT_FALSE(parseIpv4("", nullptr));
  TEST_ASSERT_FALSE(parseIpv4(nullptr, nullptr));
  TEST_ASSERT_FALSE(parseIpv4("192.168.1", nullptr));
  TEST_ASSERT_FALSE(parseIpv4("192.168.1.256", nullptr));
  TEST_ASSERT_FALSE(parseIpv4("192.168.1.1.1", nullptr));
  TEST_ASSERT_FALSE(parseIpv4("192.168..1", nullptr));
  TEST_ASSERT_FALSE(parseIpv4("abc.def.ghi.jkl", nullptr));
  TEST_ASSERT_FALSE(parseIpv4("192.168.1.42 ", nullptr));
  TEST_ASSERT_FALSE(parseIpv4("1234.1.1.1", nullptr));
}

static bool netmaskOk(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  uint8_t b[4] = {b0, b1, b2, b3};
  return isValidNetmask(b);
}

void test_valid_netmask() {
  TEST_ASSERT_TRUE(netmaskOk(255, 255, 255, 0));
  TEST_ASSERT_TRUE(netmaskOk(255, 255, 0, 0));
  TEST_ASSERT_TRUE(netmaskOk(255, 255, 255, 255));
  TEST_ASSERT_TRUE(netmaskOk(255, 255, 255, 128));
  TEST_ASSERT_FALSE(netmaskOk(192, 168, 1, 0));
  TEST_ASSERT_FALSE(netmaskOk(255, 0, 255, 0));
  TEST_ASSERT_FALSE(netmaskOk(255, 255, 255, 1));
  TEST_ASSERT_FALSE(netmaskOk(0, 0, 0, 0));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_addresses);
  RUN_TEST(test_invalid_addresses);
  RUN_TEST(test_valid_netmask);
  return UNITY_END();
}
