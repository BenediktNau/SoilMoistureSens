# Konfigurationsoberfläche und Messbetrieb mit Deep Sleep – Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Der ESP-12E-Bodenfeuchtesensor bekommt eine Web-Oberfläche zum Einstellen von WLAN, MQTT, Kalibrierung, Schwellen und Intervall sowie einen Messbetrieb, der per Timer aufwacht, misst, per MQTT sendet und wieder in Deep Sleep geht.

**Architecture:** Beim Booten entscheidet der Reset-Grund über den Modus (Timer-Wakeup = Messbetrieb, Reset-Taster = Konfigmodus). Alle Einstellungen liegen in `/config.json` im LittleFS. Reine Logik (Prozentrechnung, Bewertung, JSON-Konvertierung, Modusauswahl, MQTT-Payload) ist Arduino-frei in Headern und wird nativ auf dem PC getestet. Gerätecode (Webserver, LittleFS, MQTT, WLAN, Deep Sleep) wird per Build und seriellem Monitor am Gerät geprüft.

**Tech Stack:** PlatformIO, Arduino-Core ESP8266, ESP8266WebServer, LittleFS, ArduinoJson 7, PubSubClient 2.8, Unity (PlatformIO-Tests, Umgebung `native`).

**Spec:** `docs/superpowers/specs/2026-09-12-config-ui-and-sleep-mode-design.md`

## Global Constraints

- Board `esp12e`, Framework `arduino`, `board_build.f_cpu = 160000000L`, `monitor_speed = 115200`.
- Bibliotheken auf dem Gerät: `bblanchon/ArduinoJson@^7`, `knolleary/PubSubClient@^2.8`. WiFiManager wird am Ende entfernt (Task 7), vorher bleibt es, damit jeder Zwischenstand baut.
- Access Point: Name `SoilMoisture-Setup`, Passwort `bodenfeuchte`.
- Konfigdatei: `/config.json` im LittleFS. Standardwerte: `mqttPort` 1883, `topicPrefix` `soil`, `deviceName` `sensor1`, `dryRaw` 226, `wetRaw` 181, `dryBelowPct` 30, `wetAbovePct` 70, `intervalMin` 15. Alle Strings leer.
- `intervalMin` wird auf 1..180 begrenzt, `dryBelowPct`/`wetAbovePct` auf 0..100. `dryBelowPct < wetAbovePct` ist Pflicht, sonst 400.
- Passwörter werden über die API leer ausgegeben und nur bei nicht leerem Wert überschrieben. Zusätzlich liefert die API `wifiPasswordSet` und `mqttPasswordSet` als bool.
- Statische IP: Felder `staticIp`, `gateway`, `subnet` (Standard `255.255.255.0`), `dns`, alle IPv4-Strings. Leeres `staticIp` = DHCP, die anderen drei werden dann ignoriert. Ist `staticIp` gesetzt, müssen `staticIp`, `gateway`, `subnet` gültige IPv4-Adressen sein, sonst 400. Leeres `dns` = Gateway. Gilt in beiden Modi vor `WiFi.begin`; der Access Point bleibt 192.168.4.1.
- Konfiguration ist gültig, wenn `ssid` und `mqttHost` nicht leer sind.
- Zeiten: WLAN maximal 15 s, MQTT-Socket maximal 5 s, Konfigmodus 5 Minuten ohne Anfrage (Anfragen an `/api/status` zählen nicht).
- MQTT-Topic `<topicPrefix>/<deviceName>/state`, retained, Payload `{"raw":N,"percent":N,"level":"dry|ok|wet","rssi":N}`, Testnachricht zusätzlich `,"test":true`. Client-ID = `deviceName`.
- Farbtoken der Oberfläche: Hintergrund `#0d1116`, Karte `#141a21`, Rahmen `#242c36`, Eingabefeld `#0a0e13`, Text `#e6eaef`, gedämpft `#8a94a1`, Akzent/ok `#5eb1e8`, trocken `#e8a85c`, nass `#9b8cf5`.
- Serielle Meldungen und Kommentare auf Deutsch, Bezeichner auf Englisch (wie bisher in `src/main.cpp`).
- Keine externen Ressourcen in der HTML-Seite (kein CDN, kein Webfont).

---

## Dateistruktur

| Datei | Zuständigkeit | Task |
|---|---|---|
| `platformio.ini` | Umgebungen `esp12e` und `native`, Bibliotheken | 1, 7 |
| `src/moisture.h` | Prozent aus Rohwert, Bewertung, `Reading`; header-only, Arduino-frei | 1 |
| `src/boot_mode.h` | Modus aus Reset-Grund; header-only | 2 |
| `src/ipv4.h` | IPv4-String prüfen und in Bytes wandeln; header-only | 3 |
| `src/config.h` / `src/config.cpp` | `Config`, Standardwerte, Gültigkeit, JSON parse/serialize, LittleFS load/save | 3, 5 |
| `src/wifi_station.h` / `.cpp` | `beginStation(cfg)`: statische IP anwenden, `WiFi.begin` (Gerät) | 5 |
| `src/mqtt_payload.h` | Topic und Payload als Strings; header-only | 4 |
| `src/mqtt_sender.h` / `.cpp` | MQTT verbinden und veröffentlichen (Gerät) | 5 |
| `src/index_html.h` | HTML-Seite als PROGMEM-String | 6 |
| `src/web_ui.h` / `.cpp` | Webserver, Routen, Timeout, Sleep-Anforderung | 6 |
| `src/main.cpp` | Startablauf, Messzyklus, Konfigmodus, Deep Sleep | 7 |
| `test/test_moisture/test_main.cpp` | Tests Prozent und Bewertung | 1 |
| `test/test_boot_mode/test_main.cpp` | Tests Modusauswahl | 2 |
| `test/test_ipv4/test_main.cpp` | Tests IPv4-Parser | 3 |
| `test/test_config/test_main.cpp` | Tests JSON parse/serialize, Validierung | 3 |
| `test/test_mqtt_payload/test_main.cpp` | Tests Topic und Payload | 4 |
| `README.md` | Hardware, Flashen, Bedienung | 8 |

---

### Task 0: Git-Repository und Compiler für native Tests

**Files:**
- Create: `.git/` (per `git init`)
- Modify: `.gitignore`

**Interfaces:**
- Produces: ein Repository für die Commits der folgenden Tasks; `g++` für `pio test -e native`.

- [ ] **Step 1: gcc-c++ installieren (Fedora)**

Braucht Root. Der Nutzer tippt im Prompt:

```
! sudo dnf install -y gcc-c++
```

Prüfen:

```bash
g++ --version | head -1
```

Erwartet: eine Zeile mit `g++ (GCC) ...`.

- [ ] **Step 2: .gitignore ergänzen**

Aktueller Inhalt bleibt, dazu am Ende:

```
docs/design/bodenfeuchte-konfigseite.html
```

Grund: Das ist die zusammengesetzte Canvas-Datei mit eingebettetem Editor (mehrere MB). Die Quelldateien `Main.dc.html` und `canvas.json` bleiben versioniert.

- [ ] **Step 3: Repository anlegen und Ausgangsstand committen**

```bash
cd /home/bnau/workspace/SoilMoistureSens
git init -b main
git add .
git commit -m "chore: Ausgangsstand mit WiFiManager-Setup, Gehäuse und Design-Spec"
```

Erwartet: `git log --oneline` zeigt einen Commit. `git status` ist sauber.

---

### Task 1: Native Testumgebung und Feuchte-Logik

**Files:**
- Modify: `platformio.ini`
- Create: `src/moisture.h`
- Create: `test/test_moisture/test_main.cpp`

**Interfaces:**
- Produces:
  ```cpp
  enum class Level { Dry, Ok, Wet };
  int moisturePercent(int raw, int dryRaw, int wetRaw);            // 0..100
  Level classify(int percent, int dryBelowPct, int wetAbovePct);
  const char* levelName(Level level);                              // "dry" | "ok" | "wet"
  const char* levelNameDe(Level level);                            // "trocken" | "ok" | "nass"
  ```
  `Reading` und `evaluateReading` kommen in Task 3 dazu, sobald `Config` existiert.

- [ ] **Step 1: platformio.ini um native-Umgebung und Bibliotheken erweitern**

Datei komplett ersetzen:

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino
board_build.f_cpu = 160000000L
board_build.filesystem = littlefs
monitor_speed = 115200
lib_deps =
  tzapu/WiFiManager
  bblanchon/ArduinoJson@^7
  knolleary/PubSubClient@^2.8

[env:native]
platform = native
lib_deps = bblanchon/ArduinoJson@^7
build_flags = -std=c++17 -Isrc
test_build_src = yes
build_src_filter = -<*> +<config.cpp>
```

Hinweis: `build_src_filter` sorgt dafür, dass nativ nur `config.cpp` (ab Task 3) mitkompiliert wird, nicht der Gerätecode. Bis Task 3 existiert die Datei nicht, das ist für den Filter kein Fehler.

- [ ] **Step 2: Fehlschlagenden Test schreiben**

`test/test_moisture/test_main.cpp`:

```cpp
#include <unity.h>
#include "moisture.h"

void setUp() {}
void tearDown() {}

void test_percent_at_dry_is_zero() {
  TEST_ASSERT_EQUAL_INT(0, moisturePercent(226, 226, 181));
}

void test_percent_at_wet_is_hundred() {
  TEST_ASSERT_EQUAL_INT(100, moisturePercent(181, 226, 181));
}

void test_percent_in_between() {
  // Mitte zwischen 226 und 181 ist 203.5, Ganzzahl-Rundung liefert 51
  TEST_ASSERT_EQUAL_INT(51, moisturePercent(203, 226, 181));
}

void test_percent_is_clamped_below() {
  TEST_ASSERT_EQUAL_INT(0, moisturePercent(300, 226, 181));
}

void test_percent_is_clamped_above() {
  TEST_ASSERT_EQUAL_INT(100, moisturePercent(100, 226, 181));
}

void test_percent_with_swapped_calibration() {
  // Sensor, bei dem nass den hoeheren Rohwert hat
  TEST_ASSERT_EQUAL_INT(0, moisturePercent(100, 100, 900));
  TEST_ASSERT_EQUAL_INT(100, moisturePercent(900, 100, 900));
  TEST_ASSERT_EQUAL_INT(50, moisturePercent(500, 100, 900));
}

void test_percent_with_equal_calibration_is_zero() {
  TEST_ASSERT_EQUAL_INT(0, moisturePercent(200, 200, 200));
}

void test_classify_dry_ok_wet() {
  TEST_ASSERT_EQUAL(Level::Dry, classify(0, 30, 70));
  TEST_ASSERT_EQUAL(Level::Dry, classify(29, 30, 70));
  TEST_ASSERT_EQUAL(Level::Ok, classify(30, 30, 70));
  TEST_ASSERT_EQUAL(Level::Ok, classify(69, 30, 70));
  TEST_ASSERT_EQUAL(Level::Wet, classify(70, 30, 70));
  TEST_ASSERT_EQUAL(Level::Wet, classify(100, 30, 70));
}

void test_level_names() {
  TEST_ASSERT_EQUAL_STRING("dry", levelName(Level::Dry));
  TEST_ASSERT_EQUAL_STRING("ok", levelName(Level::Ok));
  TEST_ASSERT_EQUAL_STRING("wet", levelName(Level::Wet));
  TEST_ASSERT_EQUAL_STRING("trocken", levelNameDe(Level::Dry));
  TEST_ASSERT_EQUAL_STRING("nass", levelNameDe(Level::Wet));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_percent_at_dry_is_zero);
  RUN_TEST(test_percent_at_wet_is_hundred);
  RUN_TEST(test_percent_in_between);
  RUN_TEST(test_percent_is_clamped_below);
  RUN_TEST(test_percent_is_clamped_above);
  RUN_TEST(test_percent_with_swapped_calibration);
  RUN_TEST(test_percent_with_equal_calibration_is_zero);
  RUN_TEST(test_classify_dry_ok_wet);
  RUN_TEST(test_level_names);
  return UNITY_END();
}
```

- [ ] **Step 3: Test laufen lassen, Fehlschlag prüfen**

```bash
pio test -e native -f test_moisture
```

Erwartet: Build-Fehler `moisture.h: No such file or directory`.

- [ ] **Step 4: moisture.h schreiben**

`src/moisture.h`:

```cpp
#pragma once

// Reine Rechenlogik ohne Arduino-Abhaengigkeit, damit sie nativ testbar ist.

enum class Level { Dry, Ok, Wet };

// Prozent 0..100 aus einem Rohwert. dryRaw und wetRaw duerfen in beliebiger
// Reihenfolge liegen. Gleiche Kalibrierwerte liefern 0 statt Division durch Null.
inline int moisturePercent(int raw, int dryRaw, int wetRaw) {
  long span = (long)wetRaw - (long)dryRaw;
  if (span == 0) return 0;
  long pct = ((long)raw - (long)dryRaw) * 100L / span;
  if (pct < 0) return 0;
  if (pct > 100) return 100;
  return (int)pct;
}

inline Level classify(int percent, int dryBelowPct, int wetAbovePct) {
  if (percent < dryBelowPct) return Level::Dry;
  if (percent >= wetAbovePct) return Level::Wet;
  return Level::Ok;
}

inline const char* levelName(Level level) {
  switch (level) {
    case Level::Dry: return "dry";
    case Level::Wet: return "wet";
    default:         return "ok";
  }
}

inline const char* levelNameDe(Level level) {
  switch (level) {
    case Level::Dry: return "trocken";
    case Level::Wet: return "nass";
    default:         return "ok";
  }
}
```

- [ ] **Step 5: Test laufen lassen, Erfolg prüfen**

```bash
pio test -e native -f test_moisture
```

Erwartet: `9 Tests 0 Failures 0 Ignored` und `PASSED`.

Hinweis zur Rundung: `(203 - 226) * 100 / (181 - 226)` = `-2300 / -45` = 51 (Ganzzahl). Falls der Test 51 nicht trifft, stimmt die Formel nicht, nicht der Test.

- [ ] **Step 6: Gerätebuild prüfen (bestehendes main.cpp muss weiter bauen)**

```bash
pio run -e esp12e
```

Erwartet: `SUCCESS`. Die neuen Bibliotheken werden dabei geladen.

- [ ] **Step 7: Commit**

```bash
git add platformio.ini src/moisture.h test/test_moisture/test_main.cpp
git commit -m "feat: Feuchte-Logik als nativ testbarer Header, native Testumgebung"
```

---

### Task 2: Modusauswahl aus dem Reset-Grund

**Files:**
- Create: `src/boot_mode.h`
- Create: `test/test_boot_mode/test_main.cpp`

**Interfaces:**
- Produces:
  ```cpp
  enum class BootMode { Measure, Configure };
  // Werte entsprechen rst_reason aus user_interface.h des ESP8266-SDK
  constexpr uint32_t RST_REASON_DEEP_SLEEP_AWAKE = 5;
  constexpr uint32_t RST_REASON_EXT_SYS = 6;
  BootMode chooseBootMode(uint32_t resetReason, bool configValid);
  const char* bootModeName(BootMode m);   // "Messbetrieb" | "Konfigmodus"
  ```

- [ ] **Step 1: Fehlschlagenden Test schreiben**

`test/test_boot_mode/test_main.cpp`:

```cpp
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
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

```bash
pio test -e native -f test_boot_mode
```

Erwartet: Build-Fehler `boot_mode.h: No such file or directory`.

- [ ] **Step 3: boot_mode.h schreiben**

`src/boot_mode.h`:

```cpp
#pragma once
#include <stdint.h>

// Entscheidet anhand des Reset-Grunds, ob gemessen oder konfiguriert wird.
// Timer-Wakeup aus Deep Sleep: messen. Reset-Taster: Konfigmodus.
// Alles andere (Einschalten, Software-Reset, Watchdog): messen, wenn eine
// gueltige Konfiguration vorliegt, sonst Konfigmodus.

enum class BootMode { Measure, Configure };

// Werte aus rst_reason (user_interface.h des ESP8266 Non-OS SDK)
constexpr uint32_t RST_REASON_DEEP_SLEEP_AWAKE = 5;
constexpr uint32_t RST_REASON_EXT_SYS = 6;

inline BootMode chooseBootMode(uint32_t resetReason, bool configValid) {
  if (resetReason == RST_REASON_DEEP_SLEEP_AWAKE) return BootMode::Measure;
  if (resetReason == RST_REASON_EXT_SYS) return BootMode::Configure;
  return configValid ? BootMode::Measure : BootMode::Configure;
}

inline const char* bootModeName(BootMode m) {
  return m == BootMode::Measure ? "Messbetrieb" : "Konfigmodus";
}
```

- [ ] **Step 4: Test laufen lassen, Erfolg prüfen**

```bash
pio test -e native -f test_boot_mode
```

Erwartet: `4 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```bash
git add src/boot_mode.h test/test_boot_mode/test_main.cpp
git commit -m "feat: Modusauswahl aus Reset-Grund"
```

---

### Task 3: Konfigurationsstruktur, IPv4-Parser und JSON-Konvertierung

**Files:**
- Create: `src/ipv4.h`
- Create: `test/test_ipv4/test_main.cpp`
- Create: `src/config.h`
- Create: `src/config.cpp` (nur der Arduino-freie Teil; LittleFS kommt in Task 5)
- Modify: `src/moisture.h` (Reading und evaluateReading ergänzen)
- Create: `test/test_config/test_main.cpp`

**Interfaces:**
- Consumes: `Level`, `moisturePercent`, `classify` aus Task 1.
- Produces:
  ```cpp
  bool parseIpv4(const char* s, uint8_t out[4]);       // ipv4.h; out darf nullptr sein
  struct Config {
    char ssid[33]; char wifiPassword[65];
    char staticIp[16]; char gateway[16]; char subnet[16]; char dns[16];
    char mqttHost[65]; uint16_t mqttPort; char mqttUser[33]; char mqttPassword[65];
    char topicPrefix[33]; char deviceName[33];
    int dryRaw; int wetRaw; int dryBelowPct; int wetAbovePct; int intervalMin;
  };
  Config defaultConfig();
  bool configIsValid(const Config& c);                   // ssid und mqttHost nicht leer
  // Liest JSON ueber io. Fehlende Felder bleiben, wie sie in io stehen.
  // Leere Passwort-Strings ueberschreiben nicht. Zahlen werden begrenzt.
  // false bei unparsbarem JSON, io bleibt dann unveraendert.
  bool parseConfig(const char* json, Config& io);
  // nullptr wenn gueltig, sonst deutscher Fehlertext
  const char* validateConfig(const Config& c);
  // Schreibt JSON nach buf. maskSecrets: Passwoerter leer, dazu
  // wifiPasswordSet/mqttPasswordSet. Rueckgabe 0, wenn buf zu klein.
  size_t serializeConfig(const Config& c, char* buf, size_t n, bool maskSecrets);
  constexpr size_t CONFIG_JSON_SIZE = 1024;

  struct Reading { int raw; int percent; Level level; };
  Reading evaluateReading(int raw, const Config& c);
  ```

- [ ] **Step 0a: Fehlschlagenden Test für den IPv4-Parser schreiben**

`test/test_ipv4/test_main.cpp`:

```cpp
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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_addresses);
  RUN_TEST(test_invalid_addresses);
  return UNITY_END();
}
```

- [ ] **Step 0b: Test laufen lassen, Fehlschlag prüfen**

```bash
pio test -e native -f test_ipv4
```

Erwartet: Build-Fehler `ipv4.h: No such file or directory`.

- [ ] **Step 0c: ipv4.h schreiben**

`src/ipv4.h`:

```cpp
#pragma once
#include <stdint.h>

// Prueft einen IPv4-String der Form "a.b.c.d" (nur Ziffern und Punkte,
// jede Gruppe 1..3 Ziffern, Wert 0..255). out darf nullptr sein.
inline bool parseIpv4(const char* s, uint8_t out[4]) {
  if (s == nullptr || *s == '\0') return false;
  int part = 0;
  int value = 0;
  int digits = 0;
  uint8_t tmp[4] = {0, 0, 0, 0};
  for (const char* p = s;; p++) {
    char c = *p;
    if (c >= '0' && c <= '9') {
      if (++digits > 3) return false;
      value = value * 10 + (c - '0');
      if (value > 255) return false;
    } else if (c == '.' || c == '\0') {
      if (digits == 0 || part > 3) return false;
      tmp[part++] = (uint8_t)value;
      value = 0;
      digits = 0;
      if (c == '\0') break;
    } else {
      return false;
    }
  }
  if (part != 4) return false;
  if (out != nullptr) {
    for (int i = 0; i < 4; i++) out[i] = tmp[i];
  }
  return true;
}
```

- [ ] **Step 0d: Test laufen lassen, Erfolg prüfen**

```bash
pio test -e native -f test_ipv4
```

Erwartet: `2 Tests 0 Failures`.

- [ ] **Step 1: Fehlschlagenden Test schreiben**

`test/test_config/test_main.cpp`:

```cpp
#include <unity.h>
#include <string.h>
#include "config.h"
#include "moisture.h"

void setUp() {}
void tearDown() {}

void test_defaults() {
  Config c = defaultConfig();
  TEST_ASSERT_EQUAL_STRING("", c.ssid);
  TEST_ASSERT_EQUAL_STRING("", c.mqttHost);
  TEST_ASSERT_EQUAL_UINT16(1883, c.mqttPort);
  TEST_ASSERT_EQUAL_STRING("soil", c.topicPrefix);
  TEST_ASSERT_EQUAL_STRING("sensor1", c.deviceName);
  TEST_ASSERT_EQUAL_INT(226, c.dryRaw);
  TEST_ASSERT_EQUAL_INT(181, c.wetRaw);
  TEST_ASSERT_EQUAL_INT(30, c.dryBelowPct);
  TEST_ASSERT_EQUAL_INT(70, c.wetAbovePct);
  TEST_ASSERT_EQUAL_INT(15, c.intervalMin);
  TEST_ASSERT_EQUAL_STRING("", c.staticIp);
  TEST_ASSERT_EQUAL_STRING("", c.gateway);
  TEST_ASSERT_EQUAL_STRING("255.255.255.0", c.subnet);
  TEST_ASSERT_EQUAL_STRING("", c.dns);
  TEST_ASSERT_FALSE(configIsValid(c));
}

void test_valid_needs_ssid_and_host() {
  Config c = defaultConfig();
  strcpy(c.ssid, "Garten");
  TEST_ASSERT_FALSE(configIsValid(c));
  strcpy(c.mqttHost, "192.168.1.10");
  TEST_ASSERT_TRUE(configIsValid(c));
}

void test_parse_full_json() {
  Config c = defaultConfig();
  const char* json =
    "{\"ssid\":\"Garten\",\"wifiPassword\":\"geheim\",\"mqttHost\":\"broker\","
    "\"mqttPort\":8883,\"mqttUser\":\"u\",\"mqttPassword\":\"p\",\"topicPrefix\":\"haus\","
    "\"deviceName\":\"beet2\",\"dryRaw\":300,\"wetRaw\":150,\"dryBelowPct\":20,"
    "\"wetAbovePct\":80,\"intervalMin\":30}";
  TEST_ASSERT_TRUE(parseConfig(json, c));
  TEST_ASSERT_EQUAL_STRING("Garten", c.ssid);
  TEST_ASSERT_EQUAL_STRING("geheim", c.wifiPassword);
  TEST_ASSERT_EQUAL_STRING("broker", c.mqttHost);
  TEST_ASSERT_EQUAL_UINT16(8883, c.mqttPort);
  TEST_ASSERT_EQUAL_STRING("u", c.mqttUser);
  TEST_ASSERT_EQUAL_STRING("p", c.mqttPassword);
  TEST_ASSERT_EQUAL_STRING("haus", c.topicPrefix);
  TEST_ASSERT_EQUAL_STRING("beet2", c.deviceName);
  TEST_ASSERT_EQUAL_INT(300, c.dryRaw);
  TEST_ASSERT_EQUAL_INT(150, c.wetRaw);
  TEST_ASSERT_EQUAL_INT(20, c.dryBelowPct);
  TEST_ASSERT_EQUAL_INT(80, c.wetAbovePct);
  TEST_ASSERT_EQUAL_INT(30, c.intervalMin);
}

void test_parse_partial_json_keeps_rest() {
  Config c = defaultConfig();
  strcpy(c.ssid, "Alt");
  TEST_ASSERT_TRUE(parseConfig("{\"intervalMin\":5}", c));
  TEST_ASSERT_EQUAL_INT(5, c.intervalMin);
  TEST_ASSERT_EQUAL_STRING("Alt", c.ssid);
  TEST_ASSERT_EQUAL_INT(226, c.dryRaw);
}

void test_parse_empty_object_and_garbage() {
  Config c = defaultConfig();
  TEST_ASSERT_TRUE(parseConfig("{}", c));
  TEST_ASSERT_EQUAL_INT(15, c.intervalMin);
  strcpy(c.ssid, "Bleibt");
  TEST_ASSERT_FALSE(parseConfig("kein json", c));
  TEST_ASSERT_EQUAL_STRING("Bleibt", c.ssid);
  TEST_ASSERT_FALSE(parseConfig("", c));
}

void test_parse_empty_password_keeps_old() {
  Config c = defaultConfig();
  strcpy(c.wifiPassword, "alt1");
  strcpy(c.mqttPassword, "alt2");
  TEST_ASSERT_TRUE(parseConfig("{\"wifiPassword\":\"\",\"mqttPassword\":\"\",\"ssid\":\"\"}", c));
  TEST_ASSERT_EQUAL_STRING("alt1", c.wifiPassword);
  TEST_ASSERT_EQUAL_STRING("alt2", c.mqttPassword);
  TEST_ASSERT_EQUAL_STRING("", c.ssid);   // nur Passwoerter sind geschuetzt
}

void test_parse_clamps_numbers() {
  Config c = defaultConfig();
  TEST_ASSERT_TRUE(parseConfig("{\"intervalMin\":0,\"dryBelowPct\":-5,\"wetAbovePct\":150}", c));
  TEST_ASSERT_EQUAL_INT(1, c.intervalMin);
  TEST_ASSERT_EQUAL_INT(0, c.dryBelowPct);
  TEST_ASSERT_EQUAL_INT(100, c.wetAbovePct);
  TEST_ASSERT_TRUE(parseConfig("{\"intervalMin\":999}", c));
  TEST_ASSERT_EQUAL_INT(180, c.intervalMin);
}

void test_parse_truncates_long_strings() {
  Config c = defaultConfig();
  char json[200];
  char longSsid[50];
  memset(longSsid, 'a', 49); longSsid[49] = '\0';
  snprintf(json, sizeof json, "{\"ssid\":\"%s\"}", longSsid);
  TEST_ASSERT_TRUE(parseConfig(json, c));
  TEST_ASSERT_EQUAL_INT(32, (int)strlen(c.ssid));
}

void test_validate_thresholds() {
  Config c = defaultConfig();
  TEST_ASSERT_NULL(validateConfig(c));
  c.dryBelowPct = 70; c.wetAbovePct = 70;
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  c.dryBelowPct = 80;
  TEST_ASSERT_NOT_NULL(validateConfig(c));
}

void test_parse_static_ip_fields() {
  Config c = defaultConfig();
  TEST_ASSERT_TRUE(parseConfig(
    "{\"staticIp\":\"192.168.1.50\",\"gateway\":\"192.168.1.1\",\"subnet\":\"255.255.0.0\",\"dns\":\"1.1.1.1\"}", c));
  TEST_ASSERT_EQUAL_STRING("192.168.1.50", c.staticIp);
  TEST_ASSERT_EQUAL_STRING("192.168.1.1", c.gateway);
  TEST_ASSERT_EQUAL_STRING("255.255.0.0", c.subnet);
  TEST_ASSERT_EQUAL_STRING("1.1.1.1", c.dns);
}

void test_validate_static_ip() {
  Config c = defaultConfig();
  // DHCP: Gateway darf beliebig sein
  strcpy(c.gateway, "unsinn");
  TEST_ASSERT_NULL(validateConfig(c));
  // statische IP ohne Gateway ist ein Fehler
  strcpy(c.staticIp, "192.168.1.50");
  strcpy(c.gateway, "");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  // ungueltige IP
  strcpy(c.gateway, "192.168.1.1");
  strcpy(c.staticIp, "192.168.1.300");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  // ungueltige Maske
  strcpy(c.staticIp, "192.168.1.50");
  strcpy(c.subnet, "255.255.255");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
  // alles gueltig, DNS leer ist erlaubt
  strcpy(c.subnet, "255.255.255.0");
  TEST_ASSERT_NULL(validateConfig(c));
  // DNS gesetzt, aber ungueltig
  strcpy(c.dns, "dns.local");
  TEST_ASSERT_NOT_NULL(validateConfig(c));
}

void test_serialize_roundtrip() {
  Config c = defaultConfig();
  strcpy(c.ssid, "Garten"); strcpy(c.wifiPassword, "pw");
  strcpy(c.mqttHost, "broker"); strcpy(c.mqttPassword, "mp");
  strcpy(c.staticIp, "10.0.0.5"); strcpy(c.gateway, "10.0.0.1"); strcpy(c.dns, "10.0.0.1");
  c.intervalMin = 42;
  char buf[CONFIG_JSON_SIZE];
  size_t n = serializeConfig(c, buf, sizeof buf, false);
  TEST_ASSERT_TRUE(n > 0);
  Config back = defaultConfig();
  TEST_ASSERT_TRUE(parseConfig(buf, back));
  TEST_ASSERT_EQUAL_STRING("Garten", back.ssid);
  TEST_ASSERT_EQUAL_STRING("pw", back.wifiPassword);
  TEST_ASSERT_EQUAL_STRING("mp", back.mqttPassword);
  TEST_ASSERT_EQUAL_STRING("10.0.0.5", back.staticIp);
  TEST_ASSERT_EQUAL_STRING("10.0.0.1", back.gateway);
  TEST_ASSERT_EQUAL_STRING("255.255.255.0", back.subnet);
  TEST_ASSERT_EQUAL_STRING("10.0.0.1", back.dns);
  TEST_ASSERT_EQUAL_INT(42, back.intervalMin);
}

void test_serialize_masks_secrets() {
  Config c = defaultConfig();
  strcpy(c.wifiPassword, "pw");
  char buf[CONFIG_JSON_SIZE];
  TEST_ASSERT_TRUE(serializeConfig(c, buf, sizeof buf, true) > 0);
  TEST_ASSERT_NULL(strstr(buf, "\"pw\""));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"wifiPassword\":\"\""));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"wifiPasswordSet\":true"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"mqttPasswordSet\":false"));
}

void test_serialize_too_small_buffer_returns_zero() {
  Config c = defaultConfig();
  char buf[16];
  TEST_ASSERT_EQUAL_UINT(0, serializeConfig(c, buf, sizeof buf, false));
}

void test_evaluate_reading() {
  Config c = defaultConfig();
  Reading r = evaluateReading(226, c);
  TEST_ASSERT_EQUAL_INT(226, r.raw);
  TEST_ASSERT_EQUAL_INT(0, r.percent);
  TEST_ASSERT_EQUAL(Level::Dry, r.level);
  r = evaluateReading(181, c);
  TEST_ASSERT_EQUAL_INT(100, r.percent);
  TEST_ASSERT_EQUAL(Level::Wet, r.level);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_valid_needs_ssid_and_host);
  RUN_TEST(test_parse_full_json);
  RUN_TEST(test_parse_partial_json_keeps_rest);
  RUN_TEST(test_parse_empty_object_and_garbage);
  RUN_TEST(test_parse_empty_password_keeps_old);
  RUN_TEST(test_parse_clamps_numbers);
  RUN_TEST(test_parse_truncates_long_strings);
  RUN_TEST(test_validate_thresholds);
  RUN_TEST(test_parse_static_ip_fields);
  RUN_TEST(test_validate_static_ip);
  RUN_TEST(test_serialize_roundtrip);
  RUN_TEST(test_serialize_masks_secrets);
  RUN_TEST(test_serialize_too_small_buffer_returns_zero);
  RUN_TEST(test_evaluate_reading);
  return UNITY_END();
}
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

```bash
pio test -e native -f test_config
```

Erwartet: Build-Fehler `config.h: No such file or directory`.

- [ ] **Step 3: config.h schreiben**

`src/config.h`:

```cpp
#pragma once
#include <stddef.h>
#include <stdint.h>

// Alle Einstellungen des Sensors. Quelle ist /config.json im LittleFS.
// Dieser Header ist Arduino-frei; nur loadConfig/saveConfig brauchen das Geraet.

struct Config {
  char ssid[33];
  char wifiPassword[65];
  char staticIp[16];     // leer = DHCP
  char gateway[16];
  char subnet[16];
  char dns[16];          // leer = Gateway
  char mqttHost[65];
  uint16_t mqttPort;
  char mqttUser[33];
  char mqttPassword[65];
  char topicPrefix[33];
  char deviceName[33];
  int dryRaw;
  int wetRaw;
  int dryBelowPct;
  int wetAbovePct;
  int intervalMin;
};

// Groesse eines Puffers, der serializeConfig sicher aufnimmt
constexpr size_t CONFIG_JSON_SIZE = 1024;
constexpr int INTERVAL_MIN_MINUTES = 1;
constexpr int INTERVAL_MAX_MINUTES = 180;

Config defaultConfig();

// Gueltig, wenn SSID und MQTT-Host gesetzt sind
bool configIsValid(const Config& c);

// Liest JSON ueber io. Fehlende Felder bleiben, wie sie in io stehen.
// Leere Passwort-Strings ueberschreiben nicht. Zahlen werden begrenzt.
// Liefert false bei unparsbarem JSON; io bleibt dann unveraendert.
bool parseConfig(const char* json, Config& io);

// nullptr, wenn gueltig, sonst ein deutscher Fehlertext fuer die API
const char* validateConfig(const Config& c);

// Schreibt die Konfiguration als JSON nach buf. Bei maskSecrets werden die
// Passwoerter leer ausgegeben und wifiPasswordSet/mqttPasswordSet ergaenzt.
// Rueckgabe: Laenge, oder 0, wenn buf zu klein ist.
size_t serializeConfig(const Config& c, char* buf, size_t n, bool maskSecrets);

// Nur auf dem Geraet (LittleFS). Implementierung in config.cpp unter #ifdef ARDUINO.
bool loadConfig(Config& io);          // false, wenn Datei fehlt oder unlesbar; io behaelt Standardwerte
bool saveConfig(const Config& c);     // false bei Schreibfehler
```

- [ ] **Step 4: config.cpp schreiben (Arduino-freier Teil)**

`src/config.cpp`:

```cpp
#include "config.h"
#include "ipv4.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

static void setStr(char* dst, size_t n, const char* src) {
  snprintf(dst, n, "%s", src ? src : "");
}

static int clampInt(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

Config defaultConfig() {
  Config c;
  memset(&c, 0, sizeof c);
  c.mqttPort = 1883;
  setStr(c.subnet, sizeof c.subnet, "255.255.255.0");
  setStr(c.topicPrefix, sizeof c.topicPrefix, "soil");
  setStr(c.deviceName, sizeof c.deviceName, "sensor1");
  c.dryRaw = 226;
  c.wetRaw = 181;
  c.dryBelowPct = 30;
  c.wetAbovePct = 70;
  c.intervalMin = 15;
  return c;
}

bool configIsValid(const Config& c) {
  return c.ssid[0] != '\0' && c.mqttHost[0] != '\0';
}

// Uebernimmt einen JSON-String in dst, wenn er vorhanden ist.
// keepIfEmpty: leerer String laesst dst unveraendert (fuer Passwoerter).
static void readStr(JsonVariantConst v, char* dst, size_t n, bool keepIfEmpty) {
  if (!v.is<const char*>()) return;
  const char* s = v.as<const char*>();
  if (s == nullptr) return;
  if (keepIfEmpty && s[0] == '\0') return;
  setStr(dst, n, s);
}

static void readInt(JsonVariantConst v, int& dst) {
  if (v.is<int>()) dst = v.as<int>();
}

bool parseConfig(const char* json, Config& io) {
  if (json == nullptr) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err || !doc.is<JsonObjectConst>()) return false;
  JsonObjectConst o = doc.as<JsonObjectConst>();

  readStr(o["ssid"],         io.ssid,         sizeof io.ssid,         false);
  readStr(o["wifiPassword"], io.wifiPassword, sizeof io.wifiPassword, true);
  readStr(o["staticIp"],     io.staticIp,     sizeof io.staticIp,     false);
  readStr(o["gateway"],      io.gateway,      sizeof io.gateway,      false);
  readStr(o["subnet"],       io.subnet,       sizeof io.subnet,       false);
  readStr(o["dns"],          io.dns,          sizeof io.dns,          false);
  readStr(o["mqttHost"],     io.mqttHost,     sizeof io.mqttHost,     false);
  readStr(o["mqttUser"],     io.mqttUser,     sizeof io.mqttUser,     false);
  readStr(o["mqttPassword"], io.mqttPassword, sizeof io.mqttPassword, true);
  readStr(o["topicPrefix"],  io.topicPrefix,  sizeof io.topicPrefix,  false);
  readStr(o["deviceName"],   io.deviceName,   sizeof io.deviceName,   false);

  int port = io.mqttPort;
  readInt(o["mqttPort"], port);
  io.mqttPort = (uint16_t)clampInt(port, 1, 65535);

  readInt(o["dryRaw"], io.dryRaw);
  readInt(o["wetRaw"], io.wetRaw);
  readInt(o["dryBelowPct"], io.dryBelowPct);
  readInt(o["wetAbovePct"], io.wetAbovePct);
  readInt(o["intervalMin"], io.intervalMin);

  io.dryBelowPct = clampInt(io.dryBelowPct, 0, 100);
  io.wetAbovePct = clampInt(io.wetAbovePct, 0, 100);
  io.intervalMin = clampInt(io.intervalMin, INTERVAL_MIN_MINUTES, INTERVAL_MAX_MINUTES);
  return true;
}

const char* validateConfig(const Config& c) {
  if (c.dryBelowPct >= c.wetAbovePct) return "Schwelle 'trocken unter' muss kleiner sein als 'nass ab'";
  if (c.deviceName[0] == '\0') return "Geraetename darf nicht leer sein";
  if (c.topicPrefix[0] == '\0') return "Topic-Praefix darf nicht leer sein";
  if (c.staticIp[0] != '\0') {
    if (!parseIpv4(c.staticIp, nullptr)) return "Statische IP ist keine gueltige IPv4-Adresse";
    if (!parseIpv4(c.gateway, nullptr)) return "Gateway fehlt oder ist keine gueltige IPv4-Adresse";
    if (!parseIpv4(c.subnet, nullptr)) return "Subnetzmaske ist keine gueltige IPv4-Adresse";
    if (c.dns[0] != '\0' && !parseIpv4(c.dns, nullptr)) return "DNS ist keine gueltige IPv4-Adresse";
  }
  return nullptr;
}

size_t serializeConfig(const Config& c, char* buf, size_t n, bool maskSecrets) {
  JsonDocument doc;
  doc["ssid"] = c.ssid;
  doc["wifiPassword"] = maskSecrets ? "" : c.wifiPassword;
  doc["staticIp"] = c.staticIp;
  doc["gateway"] = c.gateway;
  doc["subnet"] = c.subnet;
  doc["dns"] = c.dns;
  doc["mqttHost"] = c.mqttHost;
  doc["mqttPort"] = c.mqttPort;
  doc["mqttUser"] = c.mqttUser;
  doc["mqttPassword"] = maskSecrets ? "" : c.mqttPassword;
  doc["topicPrefix"] = c.topicPrefix;
  doc["deviceName"] = c.deviceName;
  doc["dryRaw"] = c.dryRaw;
  doc["wetRaw"] = c.wetRaw;
  doc["dryBelowPct"] = c.dryBelowPct;
  doc["wetAbovePct"] = c.wetAbovePct;
  doc["intervalMin"] = c.intervalMin;
  if (maskSecrets) {
    doc["wifiPasswordSet"] = c.wifiPassword[0] != '\0';
    doc["mqttPasswordSet"] = c.mqttPassword[0] != '\0';
  }
  size_t need = measureJson(doc);
  if (need + 1 > n) return 0;
  return serializeJson(doc, buf, n);
}

#ifdef ARDUINO
// LittleFS-Teil folgt in Task 5
#endif
```

- [ ] **Step 5: Reading und evaluateReading in moisture.h ergänzen**

Am Ende von `src/moisture.h` anfügen:

```cpp
#include "config.h"

struct Reading {
  int raw;
  int percent;
  Level level;
};

inline Reading evaluateReading(int raw, const Config& c) {
  Reading r;
  r.raw = raw;
  r.percent = moisturePercent(raw, c.dryRaw, c.wetRaw);
  r.level = classify(r.percent, c.dryBelowPct, c.wetAbovePct);
  return r;
}
```

- [ ] **Step 6: Tests laufen lassen, Erfolg prüfen**

```bash
pio test -e native
```

Erwartet: alle vier Testgruppen `PASSED`, test_config mit `15 Tests 0 Failures`.

Bekannte Stolperstelle: Wenn `v.is<const char*>()` bei ArduinoJson 7 nicht kompiliert, stattdessen `v.is<JsonString>()` und `v.as<const char*>()` verwenden.

- [ ] **Step 7: Commit**

```bash
git add src/ipv4.h test/test_ipv4/test_main.cpp src/config.h src/config.cpp src/moisture.h test/test_config/test_main.cpp
git commit -m "feat: Konfigurationsstruktur mit JSON-Konvertierung, Validierung und IPv4-Parser"
```

---

### Task 4: MQTT-Topic und Payload

**Files:**
- Create: `src/mqtt_payload.h`
- Create: `test/test_mqtt_payload/test_main.cpp`

**Interfaces:**
- Consumes: `Config`, `Reading`, `levelName` aus Task 1 und 3.
- Produces:
  ```cpp
  size_t buildStateTopic(const Config& c, char* buf, size_t n);                       // "<prefix>/<name>/state"
  size_t buildStatePayload(const Reading& r, int rssi, bool test, char* buf, size_t n);
  constexpr size_t MQTT_TOPIC_SIZE = 80;
  constexpr size_t MQTT_PAYLOAD_SIZE = 128;
  ```

- [ ] **Step 1: Fehlschlagenden Test schreiben**

`test/test_mqtt_payload/test_main.cpp`:

```cpp
#include <unity.h>
#include <string.h>
#include "mqtt_payload.h"

void setUp() {}
void tearDown() {}

void test_topic() {
  Config c = defaultConfig();
  char buf[MQTT_TOPIC_SIZE];
  TEST_ASSERT_TRUE(buildStateTopic(c, buf, sizeof buf) > 0);
  TEST_ASSERT_EQUAL_STRING("soil/sensor1/state", buf);
  strcpy(c.topicPrefix, "haus/garten");
  strcpy(c.deviceName, "beet2");
  buildStateTopic(c, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("haus/garten/beet2/state", buf);
}

void test_payload_without_test_flag() {
  Reading r = { 202, 54, Level::Ok };
  char buf[MQTT_PAYLOAD_SIZE];
  TEST_ASSERT_TRUE(buildStatePayload(r, -61, false, buf, sizeof buf) > 0);
  TEST_ASSERT_EQUAL_STRING("{\"raw\":202,\"percent\":54,\"level\":\"ok\",\"rssi\":-61}", buf);
}

void test_payload_with_test_flag() {
  Reading r = { 218, 18, Level::Dry };
  char buf[MQTT_PAYLOAD_SIZE];
  buildStatePayload(r, -70, true, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("{\"raw\":218,\"percent\":18,\"level\":\"dry\",\"rssi\":-70,\"test\":true}", buf);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_topic);
  RUN_TEST(test_payload_without_test_flag);
  RUN_TEST(test_payload_with_test_flag);
  return UNITY_END();
}
```

- [ ] **Step 2: Test laufen lassen, Fehlschlag prüfen**

```bash
pio test -e native -f test_mqtt_payload
```

Erwartet: Build-Fehler `mqtt_payload.h: No such file or directory`.

- [ ] **Step 3: mqtt_payload.h schreiben**

`src/mqtt_payload.h`:

```cpp
#pragma once
#include <stdio.h>
#include "config.h"
#include "moisture.h"

constexpr size_t MQTT_TOPIC_SIZE = 80;
constexpr size_t MQTT_PAYLOAD_SIZE = 128;

// "<topicPrefix>/<deviceName>/state"
inline size_t buildStateTopic(const Config& c, char* buf, size_t n) {
  int len = snprintf(buf, n, "%s/%s/state", c.topicPrefix, c.deviceName);
  return len < 0 ? 0 : (size_t)len;
}

// {"raw":N,"percent":N,"level":"dry|ok|wet","rssi":N[,"test":true]}
inline size_t buildStatePayload(const Reading& r, int rssi, bool test, char* buf, size_t n) {
  int len = snprintf(buf, n, "{\"raw\":%d,\"percent\":%d,\"level\":\"%s\",\"rssi\":%d%s}",
                     r.raw, r.percent, levelName(r.level), rssi, test ? ",\"test\":true" : "");
  return len < 0 ? 0 : (size_t)len;
}
```

- [ ] **Step 4: Test laufen lassen, Erfolg prüfen**

```bash
pio test -e native -f test_mqtt_payload
```

Erwartet: `3 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```bash
git add src/mqtt_payload.h test/test_mqtt_payload/test_main.cpp
git commit -m "feat: MQTT-Topic und Payload als testbare Funktionen"
```

---

### Task 5: LittleFS-Speicherung, WLAN-Station und MQTT-Sender (Gerätecode)

**Files:**
- Modify: `src/config.cpp` (Block `#ifdef ARDUINO`)
- Create: `src/wifi_station.h`
- Create: `src/wifi_station.cpp`
- Create: `src/mqtt_sender.h`
- Create: `src/mqtt_sender.cpp`

**Interfaces:**
- Consumes: `Config`, `serializeConfig`, `parseConfig`, `buildStateTopic`, `buildStatePayload`, `Reading`.
- Produces:
  ```cpp
  bool loadConfig(Config& io);        // aus config.h, LittleFS muss gemountet sein
  bool saveConfig(const Config& c);
  // Wendet die statische IP aus cfg an (falls gesetzt) und startet WiFi.begin.
  // Nicht blockierend. false, wenn keine SSID gesetzt ist.
  bool beginStation(const Config& cfg);
  // Verbindet mit dem Broker aus cfg, veroeffentlicht retained, trennt wieder.
  // Bei Fehler: false und deutscher Text in err.
  bool mqttPublishReading(const Config& cfg, const Reading& r, int rssi, bool test, char* err, size_t errLen);
  ```

Diese Funktionen laufen nur am Gerät; der Test in diesem Task ist ein sauberer Build. Funktionstest folgt in Task 7 am Gerät.

- [ ] **Step 1: LittleFS-Teil in config.cpp ergänzen**

Den Block `#ifdef ARDUINO ... #endif` am Ende von `src/config.cpp` ersetzen durch:

```cpp
#ifdef ARDUINO
#include <Arduino.h>
#include <LittleFS.h>

static const char* CONFIG_PATH = "/config.json";

bool loadConfig(Config& io) {
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) {
    Serial.println("Keine config.json, Standardwerte");
    return false;
  }
  char buf[CONFIG_JSON_SIZE];
  size_t n = f.readBytes(buf, sizeof buf - 1);
  buf[n] = '\0';
  f.close();
  if (!parseConfig(buf, io)) {
    Serial.println("config.json unlesbar, Standardwerte");
    return false;
  }
  return true;
}

bool saveConfig(const Config& c) {
  char buf[CONFIG_JSON_SIZE];
  size_t n = serializeConfig(c, buf, sizeof buf, false);
  if (n == 0) return false;
  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) {
    Serial.println("config.json kann nicht geschrieben werden");
    return false;
  }
  size_t written = f.write((const uint8_t*)buf, n);
  f.close();
  return written == n;
}
#endif
```

- [ ] **Step 1b: wifi_station.h schreiben**

`src/wifi_station.h`:

```cpp
#pragma once
#include "config.h"

// Startet die Verbindung zum Heimnetz aus cfg. Ist staticIp gesetzt, wird
// vorher WiFi.config() mit IP, Gateway, Subnetz und DNS (leer = Gateway)
// aufgerufen; sonst DHCP. Nicht blockierend: Aufrufer prueft WiFi.status().
// Rueckgabe false, wenn keine SSID gesetzt ist.
bool beginStation(const Config& cfg);
```

- [ ] **Step 1c: wifi_station.cpp schreiben**

`src/wifi_station.cpp`:

```cpp
#include "wifi_station.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "ipv4.h"

static IPAddress toIp(const char* s) {
  uint8_t b[4] = {0, 0, 0, 0};
  parseIpv4(s, b);
  return IPAddress(b[0], b[1], b[2], b[3]);
}

bool beginStation(const Config& cfg) {
  if (cfg.ssid[0] == '\0') return false;
  if (cfg.staticIp[0] != '\0' && parseIpv4(cfg.staticIp, nullptr) &&
      parseIpv4(cfg.gateway, nullptr) && parseIpv4(cfg.subnet, nullptr)) {
    IPAddress dns = cfg.dns[0] != '\0' && parseIpv4(cfg.dns, nullptr) ? toIp(cfg.dns) : toIp(cfg.gateway);
    WiFi.config(toIp(cfg.staticIp), toIp(cfg.gateway), toIp(cfg.subnet), dns);
    Serial.printf("Statische IP %s\n", cfg.staticIp);
  } else {
    // Zurueck auf DHCP, falls vorher eine statische Adresse gesetzt war
    WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
  }
  WiFi.begin(cfg.ssid, cfg.wifiPassword);
  return true;
}
```

- [ ] **Step 2: mqtt_sender.h schreiben**

`src/mqtt_sender.h`:

```cpp
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
```

- [ ] **Step 3: mqtt_sender.cpp schreiben**

`src/mqtt_sender.cpp`:

```cpp
#include "mqtt_sender.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "mqtt_payload.h"

static const uint16_t MQTT_SOCKET_TIMEOUT_S = 5;

bool mqttPublishReading(const Config& cfg, const Reading& r, int rssi, bool test,
                        char* err, size_t errLen) {
  WiFiClient net;
  net.setTimeout(MQTT_SOCKET_TIMEOUT_S * 1000);
  PubSubClient mqtt(net);
  mqtt.setServer(cfg.mqttHost, cfg.mqttPort);
  mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);

  const char* user = cfg.mqttUser[0] ? cfg.mqttUser : nullptr;
  const char* pass = cfg.mqttPassword[0] ? cfg.mqttPassword : nullptr;
  if (!mqtt.connect(cfg.deviceName, user, pass)) {
    snprintf(err, errLen, "MQTT-Verbindung zu %s:%u fehlgeschlagen (Code %d)",
             cfg.mqttHost, cfg.mqttPort, mqtt.state());
    return false;
  }

  char topic[MQTT_TOPIC_SIZE];
  char payload[MQTT_PAYLOAD_SIZE];
  buildStateTopic(cfg, topic, sizeof topic);
  buildStatePayload(r, rssi, test, payload, sizeof payload);

  bool ok = mqtt.publish(topic, payload, true);
  mqtt.loop();
  mqtt.disconnect();
  if (!ok) {
    snprintf(err, errLen, "Veroeffentlichen auf %s fehlgeschlagen", topic);
    return false;
  }
  Serial.printf("MQTT %s <- %s\n", topic, payload);
  return true;
}
```

- [ ] **Step 4: Build am Gerät prüfen**

```bash
pio run -e esp12e
```

Erwartet: `SUCCESS`. Falls `LittleFS.h` nicht gefunden wird, fehlt `board_build.filesystem = littlefs` in `platformio.ini` (Task 1).

- [ ] **Step 5: Native Tests laufen weiterhin**

```bash
pio test -e native
```

Erwartet: alle `PASSED` (der `#ifdef ARDUINO`-Block wird nativ nicht kompiliert).

- [ ] **Step 6: Commit**

```bash
git add src/config.cpp src/wifi_station.h src/wifi_station.cpp src/mqtt_sender.h src/mqtt_sender.cpp
git commit -m "feat: Konfiguration im LittleFS speichern, WLAN-Station mit statischer IP, MQTT-Sender"
```

---

### Task 6: HTML-Seite und Webserver

**Files:**
- Create: `src/index_html.h`
- Create: `src/web_ui.h`
- Create: `src/web_ui.cpp`

**Interfaces:**
- Consumes: `Config`, `parseConfig`, `validateConfig`, `serializeConfig`, `saveConfig`, `mqttPublishReading`, `evaluateReading`.
- Produces:
  ```cpp
  class WebUi {
   public:
    WebUi(Config& cfg, unsigned long idleTimeoutMs);
    void begin();                       // Routen registrieren, Server starten
    void handle();                      // in der Schleife aufrufen
    void setRaw(int raw);               // aktueller Rohwert fuer /api/status
    bool shouldExit() const;            // Timeout abgelaufen oder /api/sleep
    int secondsLeft() const;
  };
  extern const char INDEX_HTML[] PROGMEM;
  ```

- [ ] **Step 1: index_html.h schreiben**

`src/index_html.h` (die Seite entspricht dem abgenommenen Mockup `docs/design/Main.dc.html`):

```cpp
#pragma once
#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Bodenfeuchte</title>
<style>
:root{--bg:#0d1116;--card:#141a21;--line:#242c36;--field:#0a0e13;--tx:#e6eaef;--mut:#8a94a1;--dim:#5b6572;--acc:#5eb1e8;--dry:#e8a85c;--wet:#9b8cf5}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.4 system-ui,-apple-system,"Segoe UI",sans-serif}
.wrap{max-width:480px;margin:0 auto;padding:20px 16px 28px;display:flex;flex-direction:column;gap:14px}
.mono{font-family:ui-monospace,"SF Mono",Menlo,Consolas,monospace;font-variant-numeric:tabular-nums}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:18px 16px;display:flex;flex-direction:column;gap:14px}
h2{font-size:12px;font-weight:600;letter-spacing:.09em;text-transform:uppercase;color:var(--mut);margin:0}
label{display:block;font-size:13px;color:#b7c0ca;margin-bottom:6px}
input{width:100%;height:44px;background:var(--field);border:1px solid var(--line);border-radius:8px;padding:0 12px;font:inherit;color:var(--tx)}
input.mono{font-family:ui-monospace,"SF Mono",Menlo,Consolas,monospace}
input::placeholder{color:var(--dim)}
input:focus{outline:none;border-color:var(--acc)}
button{height:44px;border-radius:8px;padding:0 14px;font:inherit;font-size:14px;font-weight:600;white-space:nowrap;cursor:pointer;background:transparent;border:1px solid #2d3742;color:#cdd5de}
button.pri{background:var(--acc);border-color:var(--acc);color:#061018;height:48px;font-size:15px}
button.sm{height:36px;font-size:13px}
button:disabled{opacity:.5;cursor:default}
.row{display:flex;gap:8px}.row>*:first-child{flex:1;min-width:0}
.g2{display:grid;grid-template-columns:1fr 1fr;gap:10px}.g3{display:grid;grid-template-columns:2fr 1fr;gap:10px}
.hint{font-size:12px;color:var(--mut);margin:0;line-height:1.45}
.head{display:flex;justify-content:space-between;align-items:flex-start;gap:12px;padding:0 2px 6px}
.pill{display:flex;align-items:center;gap:8px;background:var(--card);border:1px solid var(--line);border-radius:999px;padding:6px 10px 6px 8px;font-size:12px;color:#b7c0ca;white-space:nowrap}
.dot{width:8px;height:8px;border-radius:50%;background:var(--acc);box-shadow:0 0 8px var(--acc)}
.big{font-size:64px;font-weight:600;line-height:1;letter-spacing:-.03em}
.badge{font-size:13px;font-weight:600;padding:5px 10px;border-radius:999px;border:1px solid var(--line)}
.bar{position:relative;height:10px;display:flex;gap:2px}.bar div{height:10px}
.z1{background:#3a2a17;border-radius:999px 0 0 999px}.z2{background:#172d3a}.z3{background:#221d3a;border-radius:0 999px 999px 0}
.mark{position:absolute;top:-4px;width:4px;height:18px;margin-left:-2px;border-radius:2px;left:0}
.scale{display:flex;justify-content:space-between;font-size:11px;color:var(--dim);margin-top:8px}
.msg{min-height:18px;font-size:13px;text-align:center;color:var(--mut)}
.msg.err{color:var(--dry)}.msg.ok{color:var(--acc)}
.unit{position:relative}.unit span{position:absolute;right:12px;top:12px;color:var(--dim);font-size:14px}
.ok{color:var(--acc)}
.tbox{display:flex;align-items:center;justify-content:space-between;gap:10px;background:var(--field);border:1px solid var(--line);border-radius:8px;padding:10px 12px}
.actions{display:flex;flex-direction:column;gap:10px;padding-top:6px}
</style></head><body><div class="wrap">

<div class="head">
 <div><div style="font-size:18px;font-weight:700">Bodenfeuchte</div><div class="mono" style="font-size:12px;color:var(--mut)" id="ids">&ndash;</div></div>
 <div class="pill"><span class="dot"></span>Konfigmodus <span class="mono" id="left" style="color:var(--tx)">&ndash;:&ndash;&ndash;</span></div>
</div>

<div class="card">
 <div style="display:flex;justify-content:space-between;align-items:flex-end;gap:12px">
  <div><h2>Aktuelle Feuchte</h2><div style="display:flex;align-items:baseline;gap:6px"><span class="mono big" id="pct">&ndash;</span><span class="mono" style="font-size:22px;color:var(--mut)">%</span></div></div>
  <div style="display:flex;flex-direction:column;align-items:flex-end;gap:8px;padding-bottom:4px"><span class="badge" id="lvl">&ndash;</span><span class="mono" style="font-size:12px;color:var(--mut)">Roh <span id="raw">&ndash;</span></span></div>
 </div>
 <div>
  <div class="bar"><div class="z1" id="z1"></div><div class="z2" id="z2"></div><div class="z3" id="z3"></div><div class="mark" id="mark"></div></div>
  <div class="mono scale"><span>0</span><span id="t1" style="color:var(--mut)">30 trocken</span><span id="t2" style="color:var(--mut)">70 nass</span><span>100</span></div>
 </div>
 <p class="hint">Aktualisiert alle 2 Sekunden. Prozent rechnet mit den unten eingetragenen Kalibrierwerten.</p>
</div>

<div class="card">
 <h2>Kalibrierung</h2>
 <div><label for="dryRaw">Rohwert trocken (Sensor an der Luft)</label><div class="row"><input class="mono" id="dryRaw" type="number" inputmode="numeric"><button type="button" onclick="cal('dryRaw')">Aktuellen Wert &uuml;bernehmen</button></div></div>
 <div><label for="wetRaw">Rohwert nass (Sensor im Wasser)</label><div class="row"><input class="mono" id="wetRaw" type="number" inputmode="numeric"><button type="button" onclick="cal('wetRaw')">Aktuellen Wert &uuml;bernehmen</button></div></div>
 <p class="hint">Sensor in Luft halten, Wert &uuml;bernehmen. Dann in ein Wasserglas, Wert &uuml;bernehmen.</p>
</div>

<div class="card">
 <h2>Bewertung und Intervall</h2>
 <div class="g2">
  <div><label for="dryBelowPct">Trocken unter</label><div class="unit"><input class="mono" id="dryBelowPct" type="number" min="0" max="100"><span>%</span></div></div>
  <div><label for="wetAbovePct">Nass ab</label><div class="unit"><input class="mono" id="wetAbovePct" type="number" min="0" max="100"><span>%</span></div></div>
 </div>
 <div><label for="intervalMin">Sendeintervall</label><div class="unit"><input class="mono" id="intervalMin" type="number" min="1" max="180"><span>Minuten</span></div></div>
 <p class="hint">Zwischen zwei Messungen schl&auml;ft der Sensor. K&uuml;rzere Intervalle kosten Akku.</p>
</div>

<div class="card">
 <div style="display:flex;justify-content:space-between;align-items:center"><h2>WLAN</h2><span class="mono" id="wifiState" style="font-size:12px">&ndash;</span></div>
 <div><label for="ssid">Netzwerk</label><div class="row"><input id="ssid" list="nets" autocomplete="off"><button type="button" id="scanBtn" onclick="scan()">Suchen</button></div><datalist id="nets"></datalist></div>
 <div><label for="wifiPassword">Passwort</label><input class="mono" id="wifiPassword" type="password" autocomplete="off"></div>
 <div class="g2">
  <div><label for="staticIp">Statische IP</label><input class="mono" id="staticIp" placeholder="leer = DHCP" inputmode="decimal"></div>
  <div><label for="gateway">Gateway</label><input class="mono" id="gateway" placeholder="192.168.1.1" inputmode="decimal"></div>
 </div>
 <div class="g2">
  <div><label for="subnet">Subnetzmaske</label><input class="mono" id="subnet" inputmode="decimal"></div>
  <div><label for="dns">DNS</label><input class="mono" id="dns" placeholder="leer = Gateway" inputmode="decimal"></div>
 </div>
 <p class="hint">Eine feste IP spart beim Aufwachen die DHCP-Zeit und damit Akku. Gateway und Maske sind dann Pflicht.</p>
</div>

<div class="card">
 <h2>MQTT</h2>
 <div class="g3">
  <div><label for="mqttHost">Broker</label><input class="mono" id="mqttHost"></div>
  <div><label for="mqttPort">Port</label><input class="mono" id="mqttPort" type="number" min="1" max="65535"></div>
 </div>
 <div class="g2">
  <div><label for="mqttUser">Benutzer</label><input id="mqttUser" placeholder="optional" autocomplete="off"></div>
  <div><label for="mqttPassword">Passwort</label><input class="mono" id="mqttPassword" type="password" placeholder="optional" autocomplete="off"></div>
 </div>
 <div class="g2">
  <div><label for="topicPrefix">Topic-Pr&auml;fix</label><input class="mono" id="topicPrefix"></div>
  <div><label for="deviceName">Ger&auml;tename</label><input class="mono" id="deviceName"></div>
 </div>
 <div class="tbox"><div><div style="font-size:12px;color:var(--mut)">Sendet nach</div><div class="mono" id="topic" style="font-size:13px">&ndash;</div></div><button type="button" class="sm" onclick="mqttTest()">Testnachricht</button></div>
</div>

<div class="actions">
 <div class="msg" id="msg"></div>
 <button type="button" onclick="save()">Speichern</button>
 <button type="button" class="pri" onclick="saveSleep()">Speichern und Messbetrieb starten</button>
 <p class="hint" style="text-align:center">Danach misst der Sensor, sendet und schl&auml;ft bis zum n&auml;chsten Intervall. Zur&uuml;ck ins Men&uuml;: Taster am Geh&auml;use dr&uuml;cken.</p>
</div>

</div>
<script>
const $=id=>document.getElementById(id);
const F=['ssid','wifiPassword','staticIp','gateway','subnet','dns','mqttHost','mqttPort','mqttUser','mqttPassword','topicPrefix','deviceName','dryRaw','wetRaw','dryBelowPct','wetAbovePct','intervalMin'];
const NUM=['mqttPort','dryRaw','wetRaw','dryBelowPct','wetAbovePct','intervalMin'];
const NAMES={dry:'trocken',ok:'ok',wet:'nass'},COL={dry:'#e8a85c',ok:'#5eb1e8',wet:'#9b8cf5'};
let lastRaw=null,timer=null,ended=false;
function num(id){const v=parseInt($(id).value,10);return isNaN(v)?0:v}
function pctOf(raw){const d=num('dryRaw'),w=num('wetRaw');if(d===w)return 0;const p=Math.trunc((raw-d)*100/(w-d));return Math.max(0,Math.min(100,p))}
function levelOf(p){return p<num('dryBelowPct')?'dry':p>=num('wetAbovePct')?'wet':'ok'}
function render(){
 const d=num('dryBelowPct'),w=num('wetAbovePct');
 $('z1').style.flexBasis=d+'%';$('z2').style.flexBasis=Math.max(0,w-d)+'%';$('z3').style.flexBasis=Math.max(0,100-w)+'%';
 $('t1').textContent=d+' trocken';$('t2').textContent=w+' nass';
 $('topic').textContent=$('topicPrefix').value+'/'+$('deviceName').value+'/state';
 if(lastRaw===null)return;
 const p=pctOf(lastRaw),l=levelOf(p),c=COL[l];
 $('raw').textContent=lastRaw;$('pct').textContent=p;$('pct').style.color=c;$('pct').style.textShadow='0 0 24px '+c+'73';
 const b=$('lvl');b.textContent=NAMES[l];b.style.color=c;b.style.borderColor=c+'59';b.style.background=c+'1f';
 const m=$('mark');m.style.left=p+'%';m.style.background=c;m.style.boxShadow='0 0 10px '+c+'73';
}
function fmt(s){s=Math.max(0,s);return Math.floor(s/60)+':'+String(s%60).padStart(2,'0')}
async function poll(){
 if(ended)return;
 try{
  const r=await fetch('/api/status');const s=await r.json();
  lastRaw=s.raw;$('left').textContent=fmt(s.secondsLeft);
  $('ids').textContent=$('deviceName').value+' · '+s.apIp;
  const w=$('wifiState');w.textContent=s.staIp?'verbunden · '+s.staIp:'nicht verbunden';w.className='mono'+(s.staIp?' ok':'');
  render();
 }catch(e){say('Keine Verbindung zum Sensor','err')}
}
function collect(){const o={};for(const k of F)o[k]=NUM.includes(k)?num(k):$(k).value;return o}
async function loadCfg(){
 const r=await fetch('/api/config');const c=await r.json();
 for(const k of F)if(k in c)$(k).value=c[k];
 $('wifiPassword').placeholder=c.wifiPasswordSet?'•••••••• (unverändert)':'';
 $('mqttPassword').placeholder=c.mqttPasswordSet?'•••••••• (unverändert)':'optional';
 render();
}
function say(t,cls){const m=$('msg');m.textContent=t;m.className='msg '+(cls||'')}
async function post(url,body){
 const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body||{})});
 const t=await r.text();if(!r.ok)throw new Error(t||('HTTP '+r.status));return t;
}
async function save(){try{await post('/api/config',collect());$('wifiPassword').value='';$('mqttPassword').value='';await loadCfg();say('Gespeichert','ok')}catch(e){say(e.message,'err')}}
async function saveSleep(){
 try{await post('/api/config',collect());await post('/api/sleep');ended=true;clearInterval(timer);
  document.querySelectorAll('button').forEach(b=>b.disabled=true);
  say('Sensor misst, sendet und schläft. Zurück ins Menü mit dem Taster.','ok')}catch(e){say(e.message,'err')}
}
async function mqttTest(){try{await post('/api/config',collect());const t=await post('/api/mqtt-test');say(t||'Testnachricht gesendet','ok')}catch(e){say(e.message,'err')}}
async function scan(){
 const b=$('scanBtn');b.disabled=true;b.textContent='Suche…';
 try{const r=await fetch('/api/scan');const nets=await r.json();const dl=$('nets');dl.innerHTML='';
  for(const n of nets){const o=document.createElement('option');o.value=n.ssid;o.label=n.rssi+' dBm'+(n.secure?', gesichert':'');dl.appendChild(o)}
  say(nets.length+' Netzwerke gefunden, Feld Netzwerk antippen')}
 catch(e){say('Suche fehlgeschlagen','err')}
 b.disabled=false;b.textContent='Suchen';
}
function cal(id){if(lastRaw!==null){$(id).value=lastRaw;render()}}
document.querySelectorAll('input').forEach(i=>i.addEventListener('input',render));
loadCfg().then(poll);timer=setInterval(poll,2000);
</script></body></html>)rawliteral";
```

- [ ] **Step 2: web_ui.h schreiben**

`src/web_ui.h`:

```cpp
#pragma once
#include <ESP8266WebServer.h>
#include "config.h"

// Webserver des Konfigmodus. Bedient die Seite und die JSON-Schnittstelle.
// Jede Anfrage ausser /api/status setzt den Leerlauf-Timer zurueck.
class WebUi {
 public:
  WebUi(Config& cfg, unsigned long idleTimeoutMs);
  void begin();
  void handle();
  void setRaw(int raw);
  bool shouldExit() const;
  int secondsLeft() const;

 private:
  void touch();
  void handleRoot();
  void handleStatus();
  void handleGetConfig();
  void handlePostConfig();
  void handleScan();
  void handleMqttTest();
  void handleSleep();

  Config& cfg_;
  ESP8266WebServer server_;
  unsigned long idleTimeoutMs_;
  unsigned long lastRequestMs_;
  int raw_;
  bool sleepRequested_;
};
```

- [ ] **Step 3: web_ui.cpp schreiben**

`src/web_ui.cpp`:

```cpp
#include "web_ui.h"
#include <ESP8266WiFi.h>
#include "index_html.h"
#include "moisture.h"
#include "mqtt_payload.h"
#include "mqtt_sender.h"
#include "wifi_station.h"

static const char* JSON_TYPE = "application/json";
static const char* TEXT_TYPE = "text/plain; charset=utf-8";

WebUi::WebUi(Config& cfg, unsigned long idleTimeoutMs)
  : cfg_(cfg), server_(80), idleTimeoutMs_(idleTimeoutMs),
    lastRequestMs_(0), raw_(0), sleepRequested_(false) {}

void WebUi::begin() {
  lastRequestMs_ = millis();
  server_.on("/", HTTP_GET, [this] { handleRoot(); });
  server_.on("/api/status", HTTP_GET, [this] { handleStatus(); });
  server_.on("/api/config", HTTP_GET, [this] { handleGetConfig(); });
  server_.on("/api/config", HTTP_POST, [this] { handlePostConfig(); });
  server_.on("/api/scan", HTTP_GET, [this] { handleScan(); });
  server_.on("/api/mqtt-test", HTTP_POST, [this] { handleMqttTest(); });
  server_.on("/api/sleep", HTTP_POST, [this] { handleSleep(); });
  server_.onNotFound([this] { server_.send(404, TEXT_TYPE, "Nicht gefunden"); });
  server_.begin();
  Serial.println("Webserver auf Port 80 gestartet");
}

void WebUi::handle() { server_.handleClient(); }

void WebUi::setRaw(int raw) { raw_ = raw; }

bool WebUi::shouldExit() const {
  return sleepRequested_ || (millis() - lastRequestMs_) >= idleTimeoutMs_;
}

int WebUi::secondsLeft() const {
  unsigned long elapsed = millis() - lastRequestMs_;
  if (elapsed >= idleTimeoutMs_) return 0;
  return (int)((idleTimeoutMs_ - elapsed) / 1000UL);
}

void WebUi::touch() { lastRequestMs_ = millis(); }

void WebUi::handleRoot() {
  touch();
  server_.send_P(200, PSTR("text/html; charset=utf-8"), INDEX_HTML);
}

void WebUi::handleStatus() {
  // Absichtlich kein touch(): das Polling der Seite haelt den Konfigmodus nicht wach.
  String staIp = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String();
  char buf[192];
  snprintf(buf, sizeof buf,
           "{\"raw\":%d,\"rssi\":%d,\"staIp\":\"%s\",\"apIp\":\"%s\",\"secondsLeft\":%d}",
           raw_, WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0,
           staIp.c_str(), WiFi.softAPIP().toString().c_str(), secondsLeft());
  server_.send(200, JSON_TYPE, buf);
}

void WebUi::handleGetConfig() {
  touch();
  char buf[CONFIG_JSON_SIZE];
  if (serializeConfig(cfg_, buf, sizeof buf, true) == 0) {
    server_.send(500, TEXT_TYPE, "Konfiguration zu gross");
    return;
  }
  server_.send(200, JSON_TYPE, buf);
}

void WebUi::handlePostConfig() {
  touch();
  Config next = cfg_;
  if (!parseConfig(server_.arg("plain").c_str(), next)) {
    server_.send(400, TEXT_TYPE, "Ungueltiges JSON");
    return;
  }
  const char* problem = validateConfig(next);
  if (problem != nullptr) {
    server_.send(400, TEXT_TYPE, problem);
    return;
  }
  if (!saveConfig(next)) {
    server_.send(500, TEXT_TYPE, "Speichern fehlgeschlagen");
    return;
  }
  bool wifiChanged = strcmp(cfg_.ssid, next.ssid) != 0 ||
                     strcmp(cfg_.wifiPassword, next.wifiPassword) != 0 ||
                     strcmp(cfg_.staticIp, next.staticIp) != 0 ||
                     strcmp(cfg_.gateway, next.gateway) != 0 ||
                     strcmp(cfg_.subnet, next.subnet) != 0 ||
                     strcmp(cfg_.dns, next.dns) != 0;
  cfg_ = next;
  if (wifiChanged) {
    // Neue Zugangsdaten oder Adresse sofort probieren, nicht blockierend
    WiFi.disconnect();
    beginStation(cfg_);
  }
  Serial.println("Konfiguration gespeichert");
  server_.send(200, TEXT_TYPE, "ok");
}

void WebUi::handleScan() {
  touch();
  int n = WiFi.scanNetworks();
  if (n < 0) n = 0;
  // Nach Signalstaerke sortieren (kleine Liste, einfache Auswahl reicht)
  int idx[32];
  int count = n < 32 ? n : 32;
  for (int i = 0; i < count; i++) idx[i] = i;
  for (int i = 1; i < count; i++) {
    int k = idx[i], j = i - 1;
    while (j >= 0 && WiFi.RSSI(idx[j]) < WiFi.RSSI(k)) { idx[j + 1] = idx[j]; j--; }
    idx[j + 1] = k;
  }
  String out = "[";
  for (int i = 0; i < count; i++) {
    int id = idx[i];
    String ssid = WiFi.SSID(id);
    if (ssid.length() == 0) continue;
    ssid.replace("\\", "\\\\");
    ssid.replace("\"", "\\\"");
    if (out.length() > 1) out += ",";
    out += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(id)) +
           ",\"secure\":" + (WiFi.encryptionType(id) == ENC_TYPE_NONE ? "false" : "true") + "}";
  }
  out += "]";
  WiFi.scanDelete();
  server_.send(200, JSON_TYPE, out);
}

void WebUi::handleMqttTest() {
  touch();
  if (WiFi.status() != WL_CONNECTED) {
    server_.send(502, TEXT_TYPE, "Kein WLAN, Testnachricht nicht moeglich");
    return;
  }
  if (cfg_.mqttHost[0] == '\0') {
    server_.send(400, TEXT_TYPE, "Kein MQTT-Broker eingetragen");
    return;
  }
  Reading r = evaluateReading(raw_, cfg_);
  char err[96];
  if (!mqttPublishReading(cfg_, r, WiFi.RSSI(), true, err, sizeof err)) {
    server_.send(502, TEXT_TYPE, err);
    return;
  }
  char topic[MQTT_TOPIC_SIZE];
  snprintf(topic, sizeof topic, "Testnachricht gesendet an %s/%s/state", cfg_.topicPrefix, cfg_.deviceName);
  server_.send(200, TEXT_TYPE, topic);
}

void WebUi::handleSleep() {
  touch();
  server_.send(200, TEXT_TYPE, "ok");
  sleepRequested_ = true;
}
```

- [ ] **Step 4: Build prüfen**

```bash
pio run -e esp12e
```

Erwartet: `SUCCESS`. Der Raw-String-Literal ist etwa 9 KB; falls der Compiler über Länge klagt, die Seite in zwei Literale aufteilen, die per Präprozessor aneinandergehängt werden (`"..." "..."`).

Bekannte Stolperstellen:
- `ENC_TYPE_NONE` braucht `#include <ESP8266WiFi.h>` (ist oben eingebunden).
- `strcmp` braucht `<string.h>` (kommt über Arduino.h).

- [ ] **Step 5: Commit**

```bash
git add src/index_html.h src/web_ui.h src/web_ui.cpp
git commit -m "feat: Web-Oberflaeche und JSON-Schnittstelle des Konfigmodus"
```

---

### Task 7: Startablauf, Messzyklus und Deep Sleep

**Files:**
- Modify: `src/main.cpp` (komplett ersetzen)
- Modify: `platformio.ini` (WiFiManager entfernen)

**Interfaces:**
- Consumes: alles aus Task 1 bis 6.
- Produces: lauffähige Firmware.

- [ ] **Step 1: WiFiManager aus platformio.ini entfernen**

In `[env:esp12e]` die Zeile `  tzapu/WiFiManager` löschen. Ergebnis:

```ini
lib_deps =
  bblanchon/ArduinoJson@^7
  knolleary/PubSubClient@^2.8
```

- [ ] **Step 2: main.cpp ersetzen**

`src/main.cpp`:

```cpp
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
extern "C" {
#include <user_interface.h>
}
#include "config.h"
#include "moisture.h"
#include "boot_mode.h"
#include "mqtt_sender.h"
#include "wifi_station.h"
#include "web_ui.h"

static const char* AP_NAME     = "SoilMoisture-Setup";
static const char* AP_PASSWORD = "bodenfeuchte";

static const unsigned long WIFI_TIMEOUT_MS   = 15000;
static const unsigned long CONFIG_TIMEOUT_MS = 5UL * 60UL * 1000UL;
static const unsigned long MEASURE_PERIOD_MS = 2000;
static const int SAMPLES = 10;   // Anzahl Messungen, die gemittelt werden

static Config cfg;

int readMoistureRaw() {
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(A0);
    delay(10);
  }
  return sum / SAMPLES;
}

// Verbindet mit dem gespeicherten WLAN, blockiert hoechstens timeoutMs.
static bool connectStation(unsigned long timeoutMs) {
  if (!beginStation(cfg)) return false;
  Serial.printf("Verbinde mit '%s'", cfg.ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(100);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("Verbunden, IP %s, RSSI %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

// Kehrt nicht zurueck. Wakeup ueber GPIO16 -> RST.
static void goToSleep() {
  uint64_t us = (uint64_t)cfg.intervalMin * 60ULL * 1000000ULL;
  Serial.printf("Deep Sleep fuer %d Minuten (wach seit %lu ms)\n", cfg.intervalMin, millis());
  Serial.flush();
  WiFi.mode(WIFI_OFF);
  ESP.deepSleep(us);
  delay(500);   // deepSleep braucht einen Moment
}

// Messen, senden, schlafen. Jeder Fehler fuehrt trotzdem zum Schlafen.
static void measureCycle() {
  Reading r = evaluateReading(readMoistureRaw(), cfg);
  Serial.printf("Roh: %4d  Feuchte: %3d %%  %s\n", r.raw, r.percent, levelNameDe(r.level));

  if (!configIsValid(cfg)) {
    Serial.println("Keine gueltige Konfiguration, nichts zu senden");
    goToSleep();
  }
  WiFi.mode(WIFI_STA);
  if (!connectStation(WIFI_TIMEOUT_MS)) {
    Serial.println("WLAN nicht erreichbar");
    goToSleep();
  }
  char err[96];
  if (!mqttPublishReading(cfg, r, WiFi.RSSI(), false, err, sizeof err)) {
    Serial.println(err);
  } else {
    Serial.println("Gesendet");
  }
  goToSleep();
}

// Access Point plus Heimnetz, Webserver, bis Timeout oder /api/sleep.
static void runConfigMode() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_NAME, AP_PASSWORD);
  beginStation(cfg);   // nicht blockierend, Erfolg egal
  Serial.printf("Konfigmodus: AP '%s', Passwort '%s', http://%s/\n",
                AP_NAME, AP_PASSWORD, WiFi.softAPIP().toString().c_str());

  WebUi ui(cfg, CONFIG_TIMEOUT_MS);
  ui.begin();

  unsigned long lastMeasure = 0;
  bool reportedSta = false;
  while (!ui.shouldExit()) {
    ui.handle();
    if (millis() - lastMeasure >= MEASURE_PERIOD_MS) {
      lastMeasure = millis();
      ui.setRaw(readMoistureRaw());
    }
    if (!reportedSta && WiFi.status() == WL_CONNECTED) {
      reportedSta = true;
      Serial.printf("Auch im Heimnetz erreichbar: http://%s/\n", WiFi.localIP().toString().c_str());
    }
    delay(2);
  }
  Serial.println("Konfigmodus beendet, wechsle in den Messbetrieb");
  WiFi.softAPdisconnect(true);
  measureCycle();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  WiFi.persistent(false);   // Zugangsdaten nur aus config.json, kein Flash-Schreiben durch den Core

  if (!LittleFS.begin()) {
    Serial.println("LittleFS konnte nicht gemountet werden");
  }
  cfg = defaultConfig();
  loadConfig(cfg);

  uint32_t reason = ESP.getResetInfoPtr()->reason;
  BootMode mode = chooseBootMode(reason, configIsValid(cfg));
  Serial.printf("Reset-Grund %u, Konfiguration %s, Modus %s\n",
                reason, configIsValid(cfg) ? "gueltig" : "unvollstaendig", bootModeName(mode));

  if (mode == BootMode::Configure) {
    runConfigMode();
  } else {
    measureCycle();
  }
}

void loop() {
  // Alles passiert in setup(); measureCycle() endet immer im Deep Sleep.
}
```

- [ ] **Step 3: Build prüfen**

```bash
pio run -e esp12e
```

Erwartet: `SUCCESS`. Auf RAM/Flash-Ausgabe achten: RAM sollte unter 60 % liegen.

- [ ] **Step 4: Native Tests laufen weiterhin**

```bash
pio test -e native
```

Erwartet: alle vier Gruppen `PASSED`.

- [ ] **Step 5: Flashen und Gerätetest**

Vor dem Flashen die Brücke GPIO16 zu RST trennen, falls der Upload nicht anläuft.

```bash
pio run -e esp12e -t upload
pio device monitor
```

Testablauf am seriellen Monitor (jeden Punkt abhaken):

1. Erster Start ohne `config.json`: Ausgabe `Keine config.json, Standardwerte`, `Modus Konfigmodus`, `AP 'SoilMoisture-Setup'`.
2. Mit dem Handy ins WLAN `SoilMoisture-Setup` (Passwort `bodenfeuchte`), `http://192.168.4.1/` öffnen. Seite lädt, Prozent aktualisiert sich, Restzeit zählt herunter.
3. "Suchen" listet Netzwerke. Heimnetz und Passwort eintragen, Broker eintragen, "Speichern". Meldung `Gespeichert`. Serieller Monitor: `Konfiguration gespeichert`, danach `Auch im Heimnetz erreichbar: http://...`.
4. Seite über die Heimnetz-IP öffnen. WLAN-Karte zeigt `verbunden · IP`.
5. Sensor in Luft, "Aktuellen Wert übernehmen" bei trocken. Sensor ins Wasserglas, bei nass übernehmen. Prozentanzeige springt entsprechend.
6. "Testnachricht": am Broker mit `mosquitto_sub -h <broker> -t 'soil/#' -v` erscheint `soil/sensor1/state {"raw":..,"percent":..,"level":"..","rssi":..,"test":true}`.
7. "Speichern und Messbetrieb starten": Monitor zeigt Messung, `Gesendet`, `Deep Sleep fuer 15 Minuten`. Am Broker kommt die Nachricht ohne `test` an.
8. Brücke GPIO16 an RST setzen. Intervall zum Testen auf 1 Minute stellen (über Konfigmodus, Taster). Nach einer Minute: Monitor zeigt `Reset-Grund 5`, `Modus Messbetrieb`, Nachricht am Broker, wieder Deep Sleep. Wachzeit laut Ausgabe `wach seit` unter 10000 ms.
9. Taster am RST drücken: `Reset-Grund 6`, `Modus Konfigmodus`.
10. Seite schließen, 5 Minuten warten: Monitor zeigt `Konfigmodus beendet`, Messzyklus, Deep Sleep.
11. Falschen Broker eintragen, Messbetrieb starten: Monitor zeigt `MQTT-Verbindung ... fehlgeschlagen`, danach trotzdem Deep Sleep.
12. Statische IP: freie Adresse im Heimnetz, Gateway und Maske eintragen, "Speichern". Monitor zeigt `Statische IP ...`, WLAN-Karte zeigt nach kurzer Zeit `verbunden · <diese IP>`. Seite über die neue IP öffnen. Messbetrieb starten: Wachzeit laut `wach seit` ist kürzer als mit DHCP.
13. Statische IP ohne Gateway speichern: Meldung `Gateway fehlt oder ist keine gueltige IPv4-Adresse`, nichts gespeichert. Feld leeren, speichern: wieder DHCP.

Probleme dokumentieren und beheben, bevor der Commit passiert. Wenn ein Punkt nicht geht, ist das ein Fehler in diesem Task und nicht "später".

- [ ] **Step 6: Commit**

```bash
git add platformio.ini src/main.cpp
git commit -m "feat: Startablauf mit Reset-Grund, Messzyklus mit Deep Sleep, Konfigmodus"
```

---

### Task 8: README

**Files:**
- Create: `README.md`

- [ ] **Step 1: README schreiben**

`README.md`:

```markdown
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
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: README mit Hardware, Flashen und Bedienung"
```
