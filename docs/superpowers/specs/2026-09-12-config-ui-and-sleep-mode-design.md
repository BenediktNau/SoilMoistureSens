# Konfigurationsoberfläche und Messbetrieb mit Deep Sleep

Datum: 2026-09-12
Projekt: SoilMoistureSens (ESP-12E / ESP8266, PlatformIO, Arduino-Framework)

## Ziel

Der Sensor bekommt zwei Betriebsmodi:

- **Konfigmodus**: Der ESP hostet eine Web-Oberfläche, in der WLAN, MQTT,
  Kalibrierung, Bewertungsschwellen und Sendeintervall eingestellt werden
  und die aktuelle Bodenfeuchte live angezeigt wird.
- **Messbetrieb**: Der ESP wacht per Timer auf, misst, sendet per MQTT und
  geht sofort wieder in Deep Sleep. Standardintervall 15 Minuten.

WiFiManager wird entfernt. Alle Einstellungen liegen in einer eigenen
JSON-Datei im LittleFS, die WLAN-Zugangsdaten eingeschlossen.

## Nicht Teil dieses Vorhabens

- Zigbee (bräuchte andere Hardware, eigenes Projekt).
- Home-Assistant-Autodiscovery über MQTT.
- Schnellverbindung über gespeicherte BSSID/Kanal (mögliche spätere
  Akku-Optimierung). Eine statische IP ist dagegen enthalten, sie spart
  bereits die DHCP-Zeit.
- OTA-Updates.

## Hardware-Voraussetzungen

- Brücke **GPIO16 an RST** für den Timer-Wakeup aus Deep Sleep.
- **Reset-Taster** (auf dem NodeMCU vorhanden, kein zusätzlicher Taster).
  Zweimal drücken innerhalb von 3 s ist der einzige Weg in den Konfigmodus
  im laufenden Betrieb.
- Beim Flashen über USB kann die GPIO16-Brücke stören. Das steht in der
  README.
- Feuchte wird über einen **ADS1115** (I2C 0x48, Kanal A0) gemessen, nicht
  über den internen ADC. Die Sensorversorgung schaltet **GPIO14** über einen
  MOSFET; Pull-down 10 kΩ am Gate hält den Sensor im Deep Sleep aus.
- Der Sensor (v1.2 mit NE555) läuft an **5 V** aus einem MT3608-Step-up
  direkt am Akku. Der MOSFET an GPIO14 schaltet die Masse von Step-up und
  Sensor gemeinsam. Der ESP selbst hängt über einen HT7333 am Akku und wird
  am 3V3-Pin gespeist. Schaltplan Rev D unter `docs/hardware/`.

## Betriebsmodi und Startablauf

Beim Booten wird `ESP.getResetInfoPtr()->reason` ausgewertet:

| Reset-Grund | Bedeutung | Modus |
|---|---|---|
| `REASON_DEEP_SLEEP_AWAKE` | Timer-Wakeup | Messbetrieb |
| `REASON_EXT_SYS_RST`, Doppel-Reset-Markierung gesetzt | zweiter Druck auf RST innerhalb von 3 s | Konfigmodus |
| `REASON_EXT_SYS_RST`, keine Markierung | erster Druck auf RST | Markierung setzen, 3 s warten, Markierung löschen, dann wie Kaltstart |
| alle anderen (Einschalten, Software-Reset, Watchdog) | Kaltstart | Messbetrieb, falls Konfiguration gültig, sonst Konfigmodus |

Eine Konfiguration gilt als gültig, wenn SSID und MQTT-Host nicht leer sind.

Die Doppel-Reset-Markierung ist ein Magic-Wert im RTC-Nutzerspeicher
(`ESP.rtcUserMemoryRead/Write`, Block 0). Er überlebt einen Reset, nicht
aber das Stromlos-Machen; nach dem Einschalten steht dort Zufall, daher ein
32-Bit-Magic statt eines Bits. Timer-Wakeup und Kaltstart lesen die
Markierung nicht.

### Messbetrieb (Ziel: unter 10 s wach)

1. Konfiguration laden.
2. Sensor einschalten, 200 ms warten, Feuchte messen (10 Wandlungen des
   ADS1115, gemittelt), Sensor ausschalten. Antwortet der ADS1115 nicht,
   wird nichts gesendet.
3. WLAN verbinden, maximal 15 s.
4. MQTT verbinden, maximal 5 s, Nachricht veröffentlichen.
5. `ESP.deepSleep(intervalMin * 60e6)`.

Jeder Fehler (kein WLAN, kein Broker) führt trotzdem zu Schritt 5. Der Akku
wird nie durch Wiederholungsversuche belastet. Fehlerursachen gehen auf die
serielle Schnittstelle.

Während des Messbetriebs läuft kein Webserver.

### Konfigmodus

1. Konfiguration laden (Standardwerte, falls Datei fehlt).
2. Access Point `SoilMoisture-Setup` mit Passwort `bodenfeuchte` öffnen,
   IP 192.168.4.1.
3. Zusätzlich Verbindung zum gespeicherten WLAN versuchen (Modus AP+STA),
   damit die Seite auch über das Heimnetz erreichbar ist. Ein Fehlschlag
   ist kein Fehler.
4. Webserver auf Port 80 starten.
5. Schleife: HTTP bedienen, Feuchte alle 2 s messen und zwischenspeichern.

Der Konfigmodus endet

- nach **5 Minuten ohne HTTP-Anfrage** (jede Anfrage außer dem Polling von
  `/api/status` setzt den Zähler zurück, so zählt die Anzeige sichtbar
  herunter, solange niemand bedient), oder
- sofort über `POST /api/sleep` ("Speichern und Messbetrieb starten").

In beiden Fällen folgt ein regulärer Messzyklus (messen, senden, schlafen).
Die verbleibende Zeit steht im Statusendpunkt und wird oben in der Seite
angezeigt.

## Konfiguration

Datei `/config.json` im LittleFS. Struktur `Config`:

| Bereich | Feld | Typ | Standard |
|---|---|---|---|
| WLAN | `ssid` | String | leer |
| WLAN | `wifiPassword` | String | leer |
| WLAN | `staticIp` | String (IPv4) | leer = DHCP |
| WLAN | `gateway` | String (IPv4) | leer |
| WLAN | `subnet` | String (IPv4) | `255.255.255.0` |
| WLAN | `dns` | String (IPv4) | leer = Gateway |
| MQTT | `mqttHost` | String | leer |
| MQTT | `mqttPort` | uint16 | 1883 |
| MQTT | `mqttUser` | String | leer |
| MQTT | `mqttPassword` | String | leer |
| MQTT | `topicPrefix` | String | `soil` |
| MQTT | `deviceName` | String | `sensor1` |
| Kalibrierung | `dryRaw` | int | 226 |
| Kalibrierung | `wetRaw` | int | 181 |
| Schwellen | `dryBelowPct` | int | 30 |
| Schwellen | `wetAbovePct` | int | 70 |
| Intervall | `intervalMin` | int | 15 |

Regeln:

- Fehlende oder unlesbare Felder bekommen den Standardwert. Eine kaputte
  Datei führt zu Standardwerten, nicht zum Absturz.
- `intervalMin` wird auf 1 bis 180 begrenzt (ESP8266 schläft maximal etwa
  3 Stunden zuverlässig).
- `dryBelowPct` muss kleiner als `wetAbovePct` sein, sonst wird der
  POST mit 400 abgelehnt.
- Ist `staticIp` gesetzt, müssen `staticIp`, `gateway` und `subnet` gültige
  IPv4-Adressen sein, sonst 400. Zusätzlich muss `subnet` eine echte
  Netzmaske sein (erstes Oktett 255, Einsen zusammenhängend) und `staticIp`
  und `gateway` müssen im selben Subnetz liegen, sonst ebenfalls 400. Grund:
  Der ESP8266-Core erkennt die Argumentreihenfolge von `WiFi.config()` an
  der Maske und lehnt IP und Gateway in verschiedenen Subnetzen still ab.
  Leeres `dns` bedeutet Gateway als DNS. Leeres `staticIp` bedeutet DHCP;
  Gateway, Subnetz und DNS werden dann ignoriert.
- Schlägt `WiFi.config()` trotz Validierung fehl, meldet das Gerät seriell
  "Statische IP abgelehnt, weiter mit DHCP" und verbindet per DHCP.
- Die statische IP gilt in beiden Modi: im Messbetrieb vor `WiFi.begin`
  (spart die DHCP-Zeit), im Konfigmodus für die Heimnetz-Verbindung. Der
  Access Point behält immer 192.168.4.1.
- `WiFi.persistent(false)`: Der Arduino-Core schreibt keine eigenen
  Zugangsdaten mehr in den Flash. Einzige Quelle ist `config.json`.
- Passwörter werden beim Lesen über die API als leerer String ausgegeben und
  nur überschrieben, wenn der POST einen nicht leeren Wert liefert. Damit die
  Oberfläche anzeigen kann, ob ein Passwort hinterlegt ist, liefert die API
  zusätzlich `wifiPasswordSet` und `mqttPasswordSet` als bool.

## Web-Oberfläche

Eine einzige HTML-Seite als PROGMEM-String, kein externes CSS oder JS,
Systemschrift, Monospace für Messwerte. Layout und Farben stammen aus dem
abgenommenen Mockup (`docs/design/Main.dc.html`,
Artifact https://claude.ai/code/artifact/62b0389d-a7ac-40ed-9dd1-1d85963e8132).

Farbtoken:

| Token | Wert |
|---|---|
| Hintergrund | `#0d1116` |
| Karte | `#141a21` |
| Rahmen | `#242c36` |
| Eingabefeld | `#0a0e13` |
| Text | `#e6eaef` |
| Text gedämpft | `#8a94a1` |
| Akzent / ok | `#5eb1e8` |
| trocken | `#e8a85c` |
| nass | `#9b8cf5` |

Aufbau von oben nach unten:

1. **Kopfzeile**: Gerätename und AP-IP, rechts Pille "Konfigmodus" mit
   Restzeit mm:ss.
2. **Live-Anzeige**: Prozent groß, Bewertung als Pille, Rohwert, Balken mit
   drei Zonen und Marker. Aktualisierung alle 2 s über `/api/status`. Die
   Prozentrechnung passiert im Browser mit den aktuell eingetippten
   Kalibrier- und Schwellenwerten, so ist die Wirkung vor dem Speichern
   sichtbar.
3. **Kalibrierung**: zwei Felder mit je einem Knopf "Aktuellen Wert
   übernehmen", der den zuletzt gelesenen Rohwert einträgt.
4. **Bewertung und Intervall**: trocken unter, nass ab, Sendeintervall.
5. **WLAN**: Netzwerk als Eingabefeld mit Vorschlagsliste, Knopf "Suchen"
   ruft `/api/scan`, Passwortfeld, Anzeige der Heimnetz-IP falls verbunden.
   Darunter "Statische IP (leer = DHCP)", Gateway, Subnetzmaske und DNS.
6. **MQTT**: Broker, Port, Benutzer, Passwort, Topic-Präfix, Gerätename,
   Vorschau des Topics, Knopf "Testnachricht".
7. **Aktionen**: "Speichern" und "Speichern und Messbetrieb starten".

### HTTP-Schnittstelle

| Methode | Pfad | Zweck |
|---|---|---|
| GET | `/` | HTML-Seite |
| GET | `/api/status` | `{raw, rssi, staIp, apIp, secondsLeft}` |
| GET | `/api/config` | aktuelle Konfiguration, Passwörter leer |
| POST | `/api/config` | JSON-Body, validieren, speichern; 200 oder 400 mit Fehlertext |
| GET | `/api/scan` | `[{ssid, rssi, secure}]`, sortiert nach RSSI |
| POST | `/api/mqtt-test` | verbindet mit den **gespeicherten** MQTT-Daten und sendet eine Testnachricht; 200 oder 502 mit Fehlertext |
| POST | `/api/sleep` | antwortet 200 und beendet den Konfigmodus |

Der Knopf "Speichern und Messbetrieb starten" ruft im Browser erst
`POST /api/config`, dann `POST /api/sleep`. Ändert ein POST die
WLAN-Einstellungen, antwortet das Gerät zuerst mit 200 und verbindet erst
etwa eine halbe Sekunde später neu, damit die Antwort über die bestehende
Verbindung noch ankommt. Leere Zahlenfelder schickt der Browser nicht mit,
der gespeicherte Wert bleibt dann erhalten. Der Knopf "Testnachricht"
speichert vorher ebenfalls, damit die Testnachricht mit den sichtbaren
Werten geht.

## MQTT-Nachricht

- Topic: `<topicPrefix>/<deviceName>/state`, retained.
- Payload: `{"raw":202,"percent":54,"level":"ok","rssi":-61}`.
- `level` ist `dry`, `ok` oder `wet` nach den Schwellen.
- Client-ID: `<deviceName>`.
- Testnachricht: gleiches Topic, gleiche Struktur, zusätzlich `"test":true`.

## Code-Struktur

```
src/
  main.cpp            Reset-Grund auswerten, Modus wählen, Abläufe
  double_reset.h/.cpp Doppel-Reset-Markierung im RTC-Speicher
  config.h/.cpp       struct Config, Standardwerte, load()/save() über LittleFS + ArduinoJson
  moisture.h          reine Rechenlogik, ohne Arduino-Abhängigkeit
  web_ui.h/.cpp       ESP8266WebServer, Routen, PROGMEM-HTML
  mqtt_sender.h/.cpp  verbinden, publizieren, Timeout
  mqtt_payload.h      Topic und Payload als Strings, reine Logik
  boot_mode.h         Modus aus Reset-Grund, reine Logik
  ipv4.h              IPv4-Parser, reine Logik
  wifi_station.h/.cpp beginStation(cfg): statische IP anwenden, WiFi.begin
  index_html.h        die HTML-Seite als Raw-String-Literal in PROGMEM
test/
  test_moisture/      Unit-Tests der Rechenlogik (native)
  test_config/        Unit-Tests der JSON-Konvertierung (native)
```

### moisture.h (reine Logik)

```cpp
int moisturePercent(int raw, int dryRaw, int wetRaw);     // 0..100, dry/wet beliebig geordnet
enum class Level { Dry, Ok, Wet };
Level classify(int percent, int dryBelow, int wetAbove);
const char* levelName(Level);                             // "dry" / "ok" / "wet"
```

Ohne `Arduino.h`, damit die Tests auf dem PC laufen. Sonderfall
`dryRaw == wetRaw` liefert 0 statt Division durch Null.

### config.h

```cpp
struct Config { ... Felder wie oben ... };
Config defaultConfig();
bool configIsValid(const Config&);
bool parseConfig(const char* json, Config& io);    // fehlende Felder bleiben wie in io
const char* validateConfig(const Config&);         // nullptr oder Fehlertext
size_t serializeConfig(const Config&, char* buf, size_t n, bool maskSecrets);
bool loadConfig(Config&);                          // LittleFS, nur auf Gerät
bool saveConfig(const Config&);

// ipv4.h, reine Logik
bool parseIpv4(const char* s, uint8_t out[4]);     // "192.168.1.5" -> Bytes
```

`parseConfig` und `serializeConfig` hängen nur von ArduinoJson ab und sind
native testbar. `loadConfig`/`saveConfig` sind dünne Hüllen um LittleFS.

### Bibliotheken

`platformio.ini`:

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino
board_build.f_cpu = 160000000L
board_build.filesystem = littlefs
monitor_speed = 115200
lib_deps =
  bblanchon/ArduinoJson@^7
  knolleary/PubSubClient@^2.8

[env:native]
platform = native
lib_deps = bblanchon/ArduinoJson@^7
build_flags = -std=c++17
test_build_src = no
```

WiFiManager entfällt.

## Fehlerbehandlung

| Situation | Verhalten |
|---|---|
| `config.json` fehlt oder ist kaputt | Standardwerte, Konfigmodus |
| WLAN im Messbetrieb nicht erreichbar | nach 15 s schlafen, seriell melden |
| Broker nicht erreichbar | nach 5 s schlafen, seriell melden |
| Ungültiger POST (Schwellen, ungültige IP) | 400 mit Klartext-Fehler, nichts gespeichert |
| LittleFS-Schreibfehler | 500, alte Datei bleibt (Schreiben über `/config.json.tmp` und `rename`) |
| Reset-Taster einmal während Messbetrieb | Reset, 3 s Wartefenster, dann normaler Messzyklus |
| Reset-Taster zweimal innerhalb von 3 s | Konfigmodus |

## Tests

- **Native Unit-Tests** (`pio test -e native`): `moisturePercent`,
  `classify`, Grenzfälle (Klemmen auf 0..100, vertauschte Kalibrierwerte,
  gleiche Kalibrierwerte), `parseConfig` mit vollständigem, leerem und
  teilweisem JSON, `serializeConfig` mit und ohne Maskierung, Round-Trip,
  `validateConfig` für Schwellen und statische IP, `parseIpv4`,
  `chooseBootMode`, MQTT-Topic und -Payload.
- **Gerätetests von Hand** mit seriellem Monitor: Reset-Grund-Erkennung
  (einfacher Reset, Doppel-Reset, Timer, Einschalten), Konfigmodus-Timeout, Seite über AP und
  Heimnetz, Scan, Speichern, Testnachricht am Broker sichtbar,
  Messzyklus-Dauer, Deep Sleep und Wakeup.

## Offene Punkte

Keine. Spätere Erweiterungen (Autodiscovery, Schnellverbindung, OTA) sind
oben unter "Nicht Teil dieses Vorhabens" festgehalten.
