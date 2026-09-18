#pragma once

// Bodenfeuchtesensor am ADS1115 (I2C, Adresse 0x48, Kanal A0).
// Die Sensorversorgung wird ueber GPIO14 -> Q1 geschaltet, damit der Sensor
// im Deep Sleep keinen Strom zieht. Ohne Q1 laeuft der Code genauso, GPIO14
// schaltet dann ins Leere.

// Pin und I2C einrichten. true, wenn der ADS1115 antwortet.
bool moistureSensorBegin();

// Sensor ein- oder ausschalten. Beim Einschalten wird die Einschwingzeit
// abgewartet, damit der naechste Messwert gueltig ist.
void moistureSensorPower(bool on);

// Mittelwert aus mehreren Wandlungen. RAW_INVALID, wenn der ADS1115 fehlt,
// nicht antwortet oder der Sensor nicht eingeschaltet ist.
int moistureSensorRead();
