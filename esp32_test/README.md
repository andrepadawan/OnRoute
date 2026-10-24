# esp32_test — OnRoute hardware test firmware

**Test** firmware: it answers a single question — *does the hardware work end-to-end?*
GPS NEO-6M → ESP32 → phone hotspot → `POST /update-coordinates` on Railway, with no Mac attached.
Status is shown on an LED and on a 128x64 OLED display.

This is not the final firmware: study it, then write your own version in `../esp32/`.

> On-device strings (display, serial monitor, captive portal) are in Italian. They are quoted
> verbatim below, with their meaning alongside.

---

## Structure

```
esp32_test/
├── platformio.ini        board, libraries, hook to load_env.py
├── load_env.py           .env  ->  #define ENV_<KEY> "value"  (at compile time)
├── .env.example          .env template (the real .env is gitignored)
└── src/
    ├── main.cpp          setup()/loop(): orchestration, no delay() in the loop
    ├── config.h          pins, timings, .env defaults — everything you may want to tweak lives here
    ├── Coordinates.h     struct = mirror of PayloadReceived (Server/api.py) + toJson()
    ├── GpsReader.*       UART2 + TinyGPS++, non-blocking reads, diagnostics
    ├── Settings.*        WiFi startup (known networks -> captive portal) + NVS overrides; token and line as a pair
    ├── KnownNetworks.*   list of up to 6 WiFi networks in NVS: survives firmware flashing
    ├── WifiKeeper.*      reconnection when WiFi drops: async scan of known networks, restart after 30 min
    ├── Uploader.*        HTTPS POST with Bearer token, explains error codes
    ├── StatusLed.*       LED codes for when you can't see the serial monitor
    ├── LocalClock.*      clock in Italian local time: from GPS, or from NTP when online
    ├── NetProbe.*        "is there internet?" — separate FreeRTOS task, never blocks the loop
    ├── NetState.h        Online/Offline/Unknown enum shared by NetProbe and Screens
    ├── Display.*         OLED: I2C bus discovery, init, frame transfer
    └── Screens.*         WHAT gets drawn (boot, portal, status) + UiState
```

`Screens` only draws: it knows nothing about I2C, WiFi or GPS. `main.cpp` fills a `UiState`
(a snapshot of the data), `Display` hands it to `Screens` and sends the frame. Changing the
layout means touching a single file.

## Wiring

| GPS GY-NEO6MV2 | ESP32 DevKit        | Notes                                        |
|----------------|---------------------|----------------------------------------------|
| VCC            | 3V3                 | the module has its own regulator             |
| GND            | GND                 | **common GND**, nothing works without it     |
| TX             | GPIO16 (RX2)        | **crossed**: GPS TX into ESP32 RX            |
| RX             | GPIO17 (TX2)        | optional (only needed to configure the GPS)  |

| OLED 128x64 I2C | ESP32 DevKit       | Notes                                        |
|-----------------|--------------------|----------------------------------------------|
| VCC             | 3V3                |                                              |
| GND             | GND                |                                              |
| SDA             | GPIO21 (D21)       | standard ESP32 I2C pin                       |
| SCL             | GPIO22 (D22)       |                                              |

At boot the firmware looks for the display at `0x3C`/`0x3D` and prints `[OLED] Schermo trovato`
("display found") on serial. If it can't find it, it scans the bus and **carries on without a
display**: GPS and uploads still work.
A column of garbage pixels or an image shifted by 2 pixels → it's an SH1106 (typical of 1.3"
displays): in `platformio.ini` remove the `;` in front of `build_flags = -D OLED_SH1106`.

Battery/boost power: add a 220–1000 µF bulk capacitor close to the ESP32 power pins. If serial
shows `Brownout detector was triggered`, that's the cause.

## First boot

1. `cp .env.example .env` and fill in `URL_WEBSITE` (**full** URL, `https://…/update-coordinates`),
   `DEVICE_TOKEN` (identical to the one on Railway) and `LINE_NUMBER` (the line for that token).
2. Flash — from inside `esp32_test/`:
   ```bash
   pio run -t upload
   pio device monitor
   ```
   In VS Code: *PlatformIO → Open Project → `esp32_test`* (the repo contains two PlatformIO
   projects, open this one as a standalone project).
   During the build you should see `[load_env] ENV_URL_WEBSITE = …` — if it doesn't show up,
   the `.env` wasn't read.
3. Phone hotspot **on 2.4 GHz** (iPhone: *Personal Hotspot → Maximize Compatibility*;
   Android: 2.4 GHz band). The ESP32 can't see 5 GHz.
4. The LED stays **solid on** and the display shows `CONFIGURAZIONE` ("setup"): connect your
   phone to the `OnRoute-Setup` network, the portal opens (otherwise go to `192.168.4.1`), pick
   the hotspot, enter its password, save. The URL / token / line number fields are only needed
   to override the `.env` without reflashing: **if left empty, they stay unchanged**.
   The chosen network is added to the known-networks list (see below): next time, if that
   network is in range, the portal won't open.
   **Token and line are a pair**: they are always saved together and, if the `.env` changes,
   they are reset together. If you change the token from the portal, check that the prefilled
   line is still the right one (serial prints a warning if you changed the token but not the line).
5. Take everything **outdoors**. NEO-6M first fix: 1 to 10+ minutes; later fixes are much faster.
   The LED on the GPS module blinks once per second only when it has a fix.

## Known WiFi networks

The ESP32 remembers up to **6 networks** (`KNOWN_NETWORKS_MAX` in `config.h`), not just the last one.
They live in NVS, a partition separate from the firmware: `pio run -t upload` **does not erase them**.
Only *Erase Flash* (`pio run -t erase`) and the BOOT-button reset erase them.

- **At boot**: scan, then try the known network with the strongest signal, then the others.
  Up to 2 scans (the hotspot might still be starting up). No known network in range →
  captive portal for 3 minutes; when the portal times out, restart and start over.
- **New network from the portal**: added to the list. List full → it replaces the one saved
  longest ago. A known network with a new password gets updated.
- **When WiFi drops**: for 15 s the driver retries on its own, then every 15 s an
  **asynchronous** scan (the loop never blocks) and an attempt with the known networks in range;
  every 4 attempts the radio is switched off and on before scanning. After **30 minutes**
  without WiFi: restart → if no known network is in range, the portal reopens.
- **First boot after upgrading** from an older firmware: the network saved by WiFiManager is
  imported into the list, so a device in the field doesn't reopen the portal.

At boot, serial lists the known networks from newest to oldest (the last one is the next to be
replaced). Passwords are never printed.

## Display

```
┌────────────────────────────┐
│ LINEA 2          14:03:21  │  header: line (from portal/.env) + Italian local time
│ 04/10/2026        ora GPS  │  date + time source: GPS, rete (NTP) or "attesa ora" (waiting)
│ Trasmesso  SI 3s fa        │  "Sent": SI (yes) if the last accepted POST is under 15 s old
│ Internet   SI              │  SI / NO / verifico... (checking)
│ GPS        3D 8 sat        │  fix type and satellites
└────────────────────────────┘
```

Problem values are shown **inverted** (white box, black text), so they're visible from a metre
away. When `Trasmesso` is `NO`, it shows the first broken link in the chain:

| Reason      | Meaning                                                          |
|-------------|------------------------------------------------------------------|
| `GPS muto`  | no bytes from the GPS: wiring                                    |
| `no fix`    | GPS alive but no position: clear sky and patience                |
| `no WiFi`   | hotspot off, too far, or on 5 GHz                                |
| `no rete`   | WiFi ok but no internet (hotspot without mobile data)            |
| `HTTP 401`  | (or any other code) the server replied with an error: see table below |
| `err rete`  | the POST failed at network level (DNS, TLS, timeout)             |
| `URL?`      | empty or malformed URL                                           |
| `attesa`    | all set, the next upload goes out within 5 s                     |

**Internet ≠ WiFi.** A hotspot without mobile data gives full WiFi and zero internet. `NetProbe`
checks by opening a TCP connection to the server host every 10 s (in a separate task, so an
unresponsive DNS doesn't freeze the display and GPS). Any HTTP response from the server already
counts as proof, and in that case the check uses no data. With no internet the firmware
**doesn't attempt the POST**: it retries once the check succeeds again.

**Time:** the GPS provides UTC time even before a fix (one satellite is enough); without GPS it
comes from NTP over the internet. Conversion to Italian local time, including the 25 October
DST change, is handled by libc using the rule `CET-1CEST,M3.5.0,M10.5.0/3` in `config.h`.

## LED codes (on-board blue LED, GPIO2)

| LED                      | Meaning                                  | What to do                                  |
|--------------------------|------------------------------------------|---------------------------------------------|
| solid on                 | portal open                              | connect to `OnRoute-Setup`                  |
| fast blink (5 Hz)        | the GPS sends no bytes                   | wiring: TX→GPIO16, GND, power               |
| slow blink (1 Hz)        | GPS alive but no fix / first upload      | outdoors, clear sky, wait                   |
| 3 flashes every 2 s      | WiFi lost or no internet                 | hotspot on? 2.4 GHz? mobile data enabled?   |
| 2 flashes every 2 s      | the server replies with an error         | serial monitor or Railway logs (see below)  |
| 1 flash every 2 s        | **all good**, last POST accepted         | nothing: the hardware works                 |

**Check without a Mac:** on your phone open `https://<your-service>.up.railway.app/coordinates`
and reload: `timestamp` should advance every ~5 s. (Works outside shift hours too: only `is_active` changes.)

**Reset:** with the firmware running, hold **BOOT** for 5 s → erases **all** saved networks and
overrides, restarts, reopens the portal. (Don't hold it while powering on: that's flash mode.)

## Reading the serial monitor

```
[GPS] chars=48213 ok=402 fail=0 sats=8 hdop=1.1 fix=3 lat=45.070812 lng=7.686234 age=412ms
[HTTP] POST {"longitude":7.686234,…} -> 200 (640 ms)
```

| Symptom                     | Likely cause                                                     |
|-----------------------------|------------------------------------------------------------------|
| `chars=0`                   | TX/RX swapped, missing GND, GPS not powered                      |
| `fail` increasing           | wrong baud rate or noise on the wire                             |
| `chars` ok, `sats=0`        | the GPS talks but can't see the sky                              |
| `-> 401`                    | token differs from the one on Railway                            |
| `-> 404`                    | the URL doesn't end with `/update-coordinates`                   |
| `-> 422`                    | JSON doesn't match `PayloadReceived` (read the `detail`)         |
| `-> 301/308`                | URL with `http://` instead of `https://`                         |
| `-> -1` / network error     | hotspot without mobile data, DNS, server down                    |

## Testing against the server on your Mac (no Railway)

Mac and ESP32 on the same hotspot, then from `Server/` run `uvicorn api:app --host 0.0.0.0 --port 8000`
(`0.0.0.0`, not the default `127.0.0.1`, otherwise the ESP32 can't reach you) and in the portal
set the URL to `http://<Mac-IP>:8000/update-coordinates`. The uploader picks HTTP or HTTPS on its
own based on the URL.

## Why the firmware in `../esp32/` didn't work

Worth keeping in mind when you rewrite it:

1. **`getenv()` on ESP32 always returns empty** — there's no operating system. Here the `.env`
   is read at compile time by `load_env.py`.
2. **`WiFiClient` on an `https://` URL** — Railway is HTTPS-only; you need `WiFiClientSecure`.
3. **`delay(2000)` in the loop** — the default UART buffer is 256 bytes, the NEO-6M sends ~500/s:
   most NMEA sentences were lost. Here: non-blocking loop + 4 KB buffer.
4. **Date format `2026-09-26 14h:03m:21s`** — Pydantic expects ISO 8601, the server replied 422.
   Here: `2026-09-26T14:03:21Z` (verified against a copy of `PayloadReceived`).
5. **`fix_status` and `track` never assigned** — random memory in the JSON. Here every field has a default.
6. **`speed` in km/h** — the server works in m/s (0.3 threshold). Here `gps.speed.mps()`.
7. **`json["api_token"] = api_token`** — reversed direction (it wrote instead of reading), and
   `SPIFFS.begin()` without `true` fails on never-formatted flash. Here: NVS with `Preferences`.

## Known technical debt (fine for a test, not for the event)

- `setInsecure()`: TLS is encrypted but the server certificate isn't verified → in the final
  version use `setCACert()` with the root CA.
- Token and URL are inside the binary: anyone with physical access to the device can extract
  them. The same goes for known-network passwords: NVS is stored in plain text (NVS encryption
  exists, but it would be overkill here).
- CPU at 160 MHz instead of 240 (`CPU_FREQ_MHZ` in `config.h`) to reduce power draw. 80 MHz would
  draw even less but triples the TLS handshake time, which blocks the loop: only try it after
  measuring the `[HTTP] POST ... (xxx ms)` times.
- The POST is blocking (up to 8 s timeout): the 4 KB buffer absorbs it, but display and LED
  freeze meanwhile. The worst case (dead DNS) is already avoided by `NetProbe`. For the final
  version consider a dedicated FreeRTOS upload task with a 1-element queue — `NetProbe` is already
  a working example of the "separate task + atomic variables" pattern.
- The line is entered by hand: it may not match the token. With the silo model (one Railway
  instance per line) the server already knows its line: an authenticated endpoint such as
  `GET /device-info` returning it would remove the error at the root.
