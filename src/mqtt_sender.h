#pragma once
#include <stddef.h>
#include "config.h"
#include "moisture.h"

// Verbindet mit dem Broker aus cfg, veroeffentlicht die Messung retained auf
// <topicPrefix>/<deviceName>/state und trennt wieder. Blockiert hoechstens
// etwa 5 s pro Schritt (Socket-Timeout).
// Rueckgabe false bei Fehler, err enthaelt dann einen deutschen Text.
bool mqttPublishReading(const Config& cfg, const Reading& r, int rssi, bool test,
                        char* err, size_t errLen);
