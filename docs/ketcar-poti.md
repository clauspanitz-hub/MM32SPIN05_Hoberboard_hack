# Ketcar: Poti-/ADC-Gas (statt UART-Host)

Kurz: Gaspedal-Potentiometer direkt am MM32SPIN05-Board → Geschwindigkeit **0…1000** (vorwärts). UART-Host (PR #1) ist fürs Fahren nicht mehr nötig; UART bleibt als **Compile-Zeit-Fallback** und optional für Telemetrie.

## Was war vorher?

| Quelle | Aussage |
|--------|---------|
| `README.md` (Upstream) | „Only UART control is functional.“ |
| `src/util.c` | Poti-Hilfen `calcInputCmd` / `checkInputType` / `PRI_INPUT` aus dem EFeru-Port vorhanden, aber `readCommand()` las nur `commandR.speed` (UART). |
| EFeru `VARIANT_ADC` | Zwei Potis an Sensor-Kabel PA2/PA3 — **nicht** auf dieses MM32-Board portiert. |

## Pin (belegt, nicht erfunden)

| Signal | Pin | Beleg |
|--------|-----|--------|
| Throttle-Wiper | **PA3** | Pad in `hardware/pinout.JPG`; MindMotion DS: **ADC1_VIN[3]**; bisher **nicht** im ADC-Scan (`setup.c` hatte CH0/1/4/5/7/9) |
| Poti-GND | GND | z. B. HALLS- oder UART-Stecker |
| Poti-Versorgung | MCU-**VDDA**/Logikrail | typ. **3,3 V** (LDO) — **nicht** die rote +12 V am UART; +5 V nur wenn VDDA wirklich 5 V ist |
| Bremse (optional) | **PB10** ↔ GND | CHARG-Stecker laut `pinout.JPG`; Active-Low, Pull-up in Firmware |

**Nicht verwendet:** PA0/PA1 (Phase-B), PA4/PA5 (Phase-C), PA7 (DC-Strom), PB1 (Batterie). PA2/PA6 liegen in den Sense-Clustern auf dem Pinout-Foto und wurden bewusst gemieden.

## Verdrahtung

1. Linear-Poti (z. B. 10 kΩ): ein Ende **GND**, anderes Ende **VDDA (3,3 V)**, Schleifer → **PA3**-Pad.
2. Empfohlen: **Pulldown ~47–100 kΩ** Schleifer→GND (offener Schleifer → ADC niedrig → `cmd=0`).
3. Optional Bremse: Taster/Schalter zwischen **PB10** und **GND** (CHARG).

## Mapping & Fail-Safe

- Default `PRI_INPUT`: **TYPE 1**, MIN/MID≈400, MAX≈3700 → **0…+1000** vorwärts.
- Rohwerte außerhalb `POT_OPEN_LOW`…`POT_OPEN_HIGH` → **0**.
- TYPE-1 Idle-Zone `POT_IDLE_ZONE` oberhalb MIN → **0**.
- Bremse geschlossen → **0**.
- Bidirektional (−1000…1000): in `config.h` z. B. `PRI_INPUT  2, 400, 2048, 3700, 100` (TYPE 2, Mittelrast).

Kalibrierung: UART-Feedback bleibt aktiv; in ADC-Mode ist `Feedback.cmd1` = **roher Poti-ADC**, `Feedback.cmd` = Soll. MIN/MAX in `config.h` anpassen, neu bauen.

## Bauen / Flashen

```bash
# Default = CONTROL_ADC
make
pyocd flash -t mm32spin05pf firmware.hex
```

UART-Host wieder aktiv (Fallback):

```c
// config.h
// #define CONTROL_ADC 1
#define CONTROL_SERIAL 1
```

Dann erneut `make` und flashen. Sketch aus PR #1 (`Arduino/ketcar_hoverserial`) gilt wieder.

## Relation zu PR #1 (UART-Host)

- **PR #1** = Host-Sketch Gas/Bremse → UART-Frames.
- **Dieser Stand** = Poti ersetzt den Host **zum Fahren**; Sketch optional/obsolet für Antrieb.
- Kein Merge-Konflikt erwartet (PR #1 vor allem `Arduino/` + Doku); Branch basiert auf `main`.

## Safety

- Rad angehoben testen; Enable erst bei `|cmd| < 50`.
- Mechanischer Not-Aus in der Batterieleitung bleibt Pflicht.
- Reverse-Schematic `hardware/MM32SPIN05_CTRL.jpg` kann Fehler haben — Pinwahl folgt **Pinout-Foto + Datasheet + freiem ADC-Kanal**.
