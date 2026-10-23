# Ketcar: Host-UART Gas / Bremse (MM32SPIN05)

Arduino-/ESP32-Sketch, der periodisch Steuerframes an die Firmware
[MM32SPIN05_Hoberboard_hack](https://github.com/clauspanitz-hub/MM32SPIN05_Hoberboard_hack)
sendet (EFeru-kompatibles Protokoll).

## Sketch

Pfad: [`Arduino/ketcar_hoverserial/ketcar_hoverserial.ino`](../Arduino/ketcar_hoverserial/ketcar_hoverserial.ino)

- Primärziel: **ESP32** (`Serial2`, Pins 16/17)
- Classic **Uno/AVR**: SoftwareSerial RX=2 / TX=3, ADC A0/A1 (im Sketch kommentiert)
- `steer` = 0 (ein Motor pro Board)
- Sendeintervall **25 ms** (Firmware-Timeout ~0,8 s)
- Optionaler **Deadman** (`PIN_ENABLE`, Pull-up): LOW → `speed = 0`
- Harte Bremse → `speed = 0`; Frames laufen weiter (kein Silence-Timeout)

## Protokoll

| Feld     | Wert                                      |
|----------|-------------------------------------------|
| Baud     | 115200 8N1                                |
| Start    | `0xABCD`                                  |
| Payload  | `int16 steer`, `int16 speed`              |
| Checksum | `start XOR steer XOR speed` (uint16)      |
| Speed    | typ. −1000 … +1000 (hier Gas → positiv)   |

## Verdrahtung (One-Liner)

**Host TX → Board PB4 (Rx), Host RX ← Board PB6 (Tx), GND gemeinsam; Logik 3,3 V — bei 5-V-Host Pegelwandler.**

| Signal        | Hoverboard (MM32) | ESP32 (Default) | Uno (Hinweis)      |
|---------------|-------------------|-----------------|--------------------|
| UART Host→HB  | **PB4** Rx        | GPIO **17** TX  | SoftSerial TX **3** |
| UART HB→Host  | **PB6** Tx        | GPIO **16** RX  | SoftSerial RX **2** |
| Masse         | GND               | GND             | GND                |
| Gas           | —                 | GPIO **34** ADC | **A0**             |
| Bremse        | —                 | GPIO **35** ADC | **A1**             |
| Enable/Deadman| —                 | GPIO **25**     | Pin **4**          |

Am UART-Stecker liegt oft auch **+12 V** — **nicht** an den Host-MCU-VCC legen. Nur Signal + GND (und ggf. getrennte Versorgung).

**Pegel:** MM32-UART ist **3,3 V**. ESP32 passt direkt. Uno (5 V): mindestens Spannungsteiler oder Level-Shifter auf der Leitung Host-TX → PB4.

## Flashen & Test

1. Arduino IDE oder PlatformIO: Board **ESP32 Dev Module** (o. ä.), Sketch öffnen, flashen.
2. Hoverboard-Firmware bereits per SWD geflasht (`pyocd flash -t mm32spin05pf firmware.hex`).
3. Verdrahten (siehe oben), gemeinsamen GND prüfen.
4. Serial Monitor **115200**: Zeilen `TX speed=… thr=… brk=… en=…`; bei gültigem Feedback `FB cmd=… n=… Vbat=…`.
5. Rad **angehoben**, Enable geschlossen, kleines Gas — Motor sollte mit kleinem `|speed|` drehen. Enable öffnen → `speed=0`.
6. Pedal-Kalibrierung: `THROTTLE_MIN/MAX`, `BRAKE_MIN/MAX` im Sketch an gemessene ADC-Werte anpassen.

## Safety (Host + Fahrzeug)

- Firmware: Timeout ~0,8 s, Enable erst bei `|cmd| < 50`, Strom-Chopping — **kein** Ersatz für Not-Aus.
- Host: Deadman + Bremse → 0; bei Pedalfehler Enable öffnen.
- Empfohlen: mechanischer Not-Aus / Schütz in der Batterieleitung.

## Dual-Motor (später)

Pro Rad ein Board + eigener UART (z. B. `Serial1` / `Serial2`). Gleicher Frame, `steer` weiter 0; Vorzeichen je Radspiegelung am Host.
