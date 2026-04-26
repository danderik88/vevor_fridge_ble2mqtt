# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 firmware (PlatformIO / Arduino framework) that bridges Alpicool-compatible fridges (including Vevor C50) from their proprietary BLE protocol to MQTT for Home Assistant integration. Tested on Vevor C50.

## Build / Flash / Monitor

PlatformIO project. Default env: `esp32dev` (`platformio.ini`).

```bash
pio run                      # build
pio run -t upload            # flash over USB (first time)
pio run -t upload --upload-port <ip>  # OTA once device is on the LAN
pio device monitor           # serial monitor @ 115200
pio run -t clean
```

Before first build, copy `src/config/config_example.h` → `src/config/config.h` and fill in WiFi / MQTT / `FRIDGE_BLE_ADDRESS` (lowercase MAC — casing matters, see README). `config.h` is gitignored.

There is no automated test suite (`test/` contains only the PlatformIO stub README). `test_mqtt.py` is a one-off broker credential check (`python3 test_mqtt.py`, paho-mqtt v2).

## Architecture

Single Arduino `setup()` / `loop()` in `src/main.cpp`, driving three long-lived objects on the main task:

- **`Blescanner`** (`src/devices/ble_scanner.cpp`) — passive NimBLE scan (6s cadence). When an advertisement matches `FRIDGE_BLE_ADDRESS`, it hands the `BLEAdvertisedDevice` to `Fridge` via `setServer()` + `setDoConnect(true)` and stops the scan. Scan only runs while the fridge is active and disconnected.
- **`Fridge`** (`src/devices/fridge.cpp`) — owns the `BLEClient`, service/char handles, and the framed protocol. Its `loop()` performs the deferred connect (scanner sets a flag, connect happens on the main loop, not the scan callback) and sends a ping frame every `DELAY_BETWEEN_MESSAGES` (2s). Notifications populate the global `last_status_report` and push values into `DataModel`.
- **`MqttHandler`** (`src/utils/mqtt.cpp`) — PubSubClient over `WiFiClient`. Subscribes to `COMMAND_TOPIC`, publishes `SENSOR_TOPIC` JSON every `DATA_PUBLISH_FREQ_MILLIS` (1s default), uses `AVAILABILITY_TOPIC` as LWT (retained `online`/`offline`). `MqttHandler::publish` is also used to mirror ESP logs to `DEBUG_TOPIC` via `esp_log_set_vprintf(&my_logging)`.

**Data flow:** BLE notify → `notifyCallback` parses `StatusReport` → writes `DataModel::getInstance().fridge_data` (singleton) → `MqttHandler::publishData` serializes it as `f_voltage`, `f_eco`, `f_on`, `f_actual_temperature`, `f_desired_temperature` to `SENSOR_TOPIC`. Commands arrive on `COMMAND_TOPIC` → `mqtt_callback` in `main.cpp` parses JSON (`fridge_on`, `fridge_eco`, `fridge_temperature`, `restart`) → calls `Fridge::setOnOff` / `setEcoMode` / `setTemperature`, which build a `SetStateCommand` or `SetTempCommand` from the last known `Settings` and write it over BLE.

**Protocol (defined in `src/devices/fridge.h`):** framed little-endian `__attribute__((packed))` structs with `0xfefe` preamble, 1-byte length, 1-byte command code, payload, and a 2-byte checksum that is the **byte-wise sum** of every field (booleans count as +1 each). The builders `NewSetTempCommand` / `NewSetStateCommand` deliberately swap the final two bytes after `memcpy` to fix checksum endianness on the wire — don't "clean that up", it's load-bearing. Control commands reuse the fridge's current `Settings` (mutated copy of `last_status_report.Settings`) so the device doesn't reset unrelated menu values.

**Wi-Fi/BLE coexistence:** `setup()` calls `esp_coex_preference_set(ESP_COEX_PREFER_BALANCE)` **once** — do not revert this to per-tick `PREFER_BT`: on a moderate-signal AP near a strong BLE peer, PREFER_BT starves Wi-Fi until the association dies. `main.cpp` re-calls `connect_wifi()` every 10s; `connect_wifi` is idempotent (returns early if connected) and escalates on failure: soft `WiFi.disconnect()` for the first 3 attempts, then `WIFI_OFF → WIFI_STA` full driver reset, then `ESP.restart()` once offline > 10 minutes. Counters live in static state inside `src/utils/wifi_util.cpp` and reset on successful connect. The scanner uses `setActiveScan(false)` and a 200/100 ms interval/window to stay out of the BLE stack's way — see the README "WDT Panic & Crash Fixes" notes before widening these.

**Main-loop task watchdog:** `setup_internal` arms `esp_task_wdt_init(30, true)` and subscribes the Arduino loop task. `loop_internal` must call `esp_task_wdt_reset()` as its first statement (nowhere else — the whole point is to catch blocks inside `blescanner->loop()` / `mqtt->loop()` / `fridge->loop()`). Budget of 30 s is deliberately ~2× the sum of worst-case legitimate blocking per tick (BLE scan 2 s + `PubSubClient::connect` 5 s + `WiFi.waitForConnectResult` 5 s + overhead). The WDT is complementary to the wifi_util escalation: wifi_util handles *"Wi-Fi down, loop ticking"*, the WDT handles *"loop not ticking"*. Widening scan duration, raising MQTT/Wi-Fi timeouts, or adding synchronous reads means revisiting this value. On boot, `setup()` logs `esp_reset_reason()` as `ESP_LOGW` — value `6` = task-WDT reboot (software freeze caught), `9` = brownout (power supply issue, not a firmware bug). Mirrored to `DEBUG_TOPIC` via `my_logging`.

**Bounded blocking calls:** three sites were deliberately timeout-bounded to keep the loop inside the 30 s WDT budget — do not widen them without a reason: `PubSubClient::setSocketTimeout(5)` + `WiFiClient::setTimeout(5)` + `setKeepAlive(15)` in `MqttHandler`'s constructor; `BLEClient::setConnectTimeout(10)` in `Fridge`'s constructor; `BLEClient::connect(server)` return value is now checked at `connectToServer`. Confirmed writes (`writeValue(..., true)`) are guarded by both the `connected` member and `client->isConnected()` — the latter because `connected` is set from the `onDisconnect` callback and can race with NimBLE's own link state. Don't change `true` → `false` on those writes: the retry path at the end of `Fridge::loop()` depends on the `writeValue` bool return.

**MQTT resilience:** `MqttHandler` uses `AVAILABILITY_TOPIC` as retained LWT (`online`/`offline`). When broker calls fail, rely on reconnect logic rather than tearing down the Wi-Fi stack — Wi-Fi recovery and MQTT recovery are independent concerns.

## Config / Topics

All compile-time: `src/config/config.h`. Defaults (from `config_example.h`): `tele/alpicool/{sensor,cmd,debug,availability}`, client id `alpicool-esp32`, publish 1 Hz. The Home Assistant YAML in `README.md` / `homeassistant/configuration.yaml` is hard-coded against those topic names and the `f_*` JSON keys — changing either requires updating both sides.

## Python helpers (`python/`)

`gatt.py` / `gattclient.py` are **reverse-engineering scratch scripts** (bleak-based) used to sniff the BLE protocol, not part of the firmware build. Keep them when exploring a new fridge model, but don't wire them into anything.


<!-- BEGIN BEADS INTEGRATION v:1 profile:minimal hash:ca08a54f -->
## Beads Issue Tracker

This project uses **bd (beads)** for issue tracking. Run `bd prime` to see full workflow context and commands.

### Quick Reference

```bash
bd ready              # Find available work
bd show <id>          # View issue details
bd update <id> --claim  # Claim work
bd close <id>         # Complete work
```

### Rules

- Use `bd` for ALL task tracking — do NOT use TodoWrite, TaskCreate, or markdown TODO lists
- Run `bd prime` for detailed command reference and session close protocol
- Use `bd remember` for persistent knowledge — do NOT use MEMORY.md files

## Session Completion

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   bd dolt push
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds
<!-- END BEADS INTEGRATION -->
