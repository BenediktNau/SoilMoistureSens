# SoilMoistureSens

Batteriebetriebener Bodenfeuchtesensor auf ESP-12E (ESP8266) mit Solarladung.
Misst alle 15 Minuten, sendet per MQTT und schläft dazwischen im Deep Sleep.
Einstellungen laufen über eine Web-Oberfläche auf dem Chip.

## Hardware

- ESP-12E, analoger Feuchtesensor an A0
- 18650-Akku mit HW-775-Lademodul, 14,5 cm Solarpanel (Gehäuse unter `case/`)
- **Brücke GPIO16 an RST**: ohne sie wacht der Chip nicht aus dem Deep Sleep auf
- **Taster zwischen RST und GND** am Gehäuse: öffnet den Konfigmodus

## Flashen

```bash
pio run -e esp12e -t upload
pio device monitor
```

Startet der Upload nicht, die Brücke GPIO16 zu RST für den Upload trennen.

## Bedienung

**Konfigmodus** startet beim ersten Einschalten (ohne Konfiguration) oder nach
Druck auf den Taster.

1. Mit WLAN `SoilMoisture-Setup` verbinden, Passwort `bodenfeuchte`.
2. `http://192.168.4.1/` öffnen.
3. WLAN, MQTT-Broker, Kalibrierung und Intervall eintragen.
4. "Speichern und Messbetrieb starten".

Sobald das Heimnetz eingetragen ist, ist die Seite auch über die dort
vergebene IP erreichbar (steht im seriellen Monitor und in der WLAN-Karte).

Kalibrieren: Sensor an der Luft halten und bei "trocken" den aktuellen Wert
übernehmen, dann ins Wasserglas und bei "nass" übernehmen.

Statische IP: Wer eine feste Adresse einträgt, spart beim Aufwachen die
DHCP-Zeit. Gateway und Subnetzmaske sind dann Pflicht, DNS ist optional
(leer = Gateway). Feld leer lassen bedeutet DHCP.

Der Konfigmodus endet nach 5 Minuten ohne Bedienung von selbst.

**Messbetrieb**: aufwachen, messen, senden, schlafen. Ziel unter 10 Sekunden
wach. Ohne WLAN oder Broker schläft der Sensor trotzdem bis zum nächsten
Intervall, damit der Akku geschont wird.

## MQTT

Topic `<Präfix>/<Gerätename>/state`, retained:

```json
{"raw":202,"percent":54,"level":"ok","rssi":-61}
```

`level` ist `dry`, `ok` oder `wet` nach den eingestellten Schwellen.

## Entwicklung

```bash
pio test -e native      # Logik-Tests auf dem PC (braucht g++)
pio run -e esp12e       # Firmware bauen
```

Konfiguration liegt in `/config.json` im LittleFS des Chips. Zum Zurücksetzen
`pio run -e esp12e -t erase` und neu flashen.

Design-Spec: `docs/superpowers/specs/2026-09-12-config-ui-and-sleep-mode-design.md`
Mockup der Oberfläche: `docs/design/Main.dc.html`
