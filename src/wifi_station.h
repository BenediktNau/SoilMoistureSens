#pragma once
#include "config.h"

// Startet die Verbindung zum Heimnetz aus cfg. Ist staticIp gesetzt, wird
// vorher WiFi.config() mit IP, Gateway, Subnetz und DNS (leer = Gateway)
// aufgerufen; sonst DHCP. Nicht blockierend: Aufrufer prueft WiFi.status().
// Rueckgabe false, wenn keine SSID gesetzt ist.
bool beginStation(const Config& cfg);
