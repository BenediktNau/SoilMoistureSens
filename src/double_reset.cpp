#include "double_reset.h"
#include <Arduino.h>

// Offset in 4-Byte-Bloecken im RTC-Nutzerspeicher (0..127). Nach dem
// Einschalten steht dort Zufall, daher ein Magic-Wert statt eines Bits.
static const uint32_t RTC_SLOT  = 0;
static const uint32_t RTC_MAGIC = 0xD0B1E5E7;

static uint32_t readSlot() {
  uint32_t v = 0;
  ESP.rtcUserMemoryRead(RTC_SLOT, &v, sizeof v);
  return v;
}

static void writeSlot(uint32_t v) {
  ESP.rtcUserMemoryWrite(RTC_SLOT, &v, sizeof v);
}

bool doubleResetPending() {
  bool pending = readSlot() == RTC_MAGIC;
  if (pending) writeSlot(0);
  return pending;
}

void doubleResetArm()    { writeSlot(RTC_MAGIC); }
void doubleResetDisarm() { writeSlot(0); }
