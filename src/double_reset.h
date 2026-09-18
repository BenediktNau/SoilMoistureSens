#pragma once

// Erkennt einen doppelten Druck auf den Reset-Taster ueber den RTC-Speicher
// des ESP8266. Der ueberlebt einen Reset, aber kein Stromlos-Machen.
//
// Ablauf beim Boot nach Reset-Taster:
//   doubleResetPending()  -> true: zweiter Druck, Konfigmodus
//                         -> false: doubleResetArm(), DOUBLE_RESET_WINDOW_MS
//                            warten, doubleResetDisarm(), normal weiter.

static const unsigned long DOUBLE_RESET_WINDOW_MS = 3000;

// Liest und loescht die Markierung. true, wenn sie gesetzt war.
bool doubleResetPending();
void doubleResetArm();
void doubleResetDisarm();
