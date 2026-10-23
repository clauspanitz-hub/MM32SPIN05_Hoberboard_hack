# MM32SPIN05 based Hoberboard hack

This project is EFeru Hoverboard hack clone for MM32SPIN05 (https://github.com/EFeru/hoverboard-firmware-hack-FOC)
<img src="hardware/IMG_6922.JPG">

## Input modes (Ketcar fork)

**Default: potentiometer (ADC) on PA3** — see [docs/ketcar-poti.md](docs/ketcar-poti.md).

- Throttle pot → speed **0…1000** (forward). Optional brake switch on **PB10** (CHARG ↔ GND).
- UART host remains a **compile-time fallback** (`CONTROL_SERIAL` in `src/config.h`). Protocol notes: [EFeru hoverserial](https://github.com/EFeru/hoverboard-firmware-hack-FOC/tree/main/Arduino/hoverserial).

Upstream README said “Only UART control is functional”: EFeru `VARIANT_ADC` was never wired on this MM32 port; pot scaling helpers already existed in `util.c`. This fork enables ADC CH3 (PA3).

<img src="hardware/IMG_6918.JPG">

TRQ_MODE and SPD_MODE are not functional because of hardware limitation - looks like originally board was designed for MCU with integrated opamp.

Visual Studio code with Platformio plugin can be used for code design, but it generates not functional binary.
Please use "make" to compile sources.
<img src="hardware/bild.png">

Use "pyocd flash -t mm32spin05pf firmware.hex" to upload binary.
<img src="hardware/upload.png">

GCC compilation for MM32SPIN05 adapted by me, based on Mindmotion packet for Keil uVision.
If any mistakes - feel free to fix yourself.

Board view:
<img src="hardware/IMG_6858.JPG">
Pinout:
<img src="hardware/pinout.JPG">
Schematic:
<img src="hardware/MM32SPIN05_CTRL.jpg">

I am sorry about mistakes in the reversed schematic, if any :)

Disclaimer:
no guarantee, no support.

