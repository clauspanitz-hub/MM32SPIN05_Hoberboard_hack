# MM32SPIN05 based Hoberboard hack

This project is EFeru Hoverboard hack clone for MM32SPIN05 (https://github.com/EFeru/hoverboard-firmware-hack-FOC)
<img src="hardware/IMG_6922.JPG">

Only UART control is functional.
Please check here https://github.com/EFeru/hoverboard-firmware-hack-FOC/tree/main/Arduino/hoverserial for details.
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

---

## Ketcar: Host-UART (Gas / Bremse)

Sketch und Verdrahtung (Deutsch): [`docs/ketcar-uart-host.md`](docs/ketcar-uart-host.md)  
Code: [`Arduino/ketcar_hoverserial/`](Arduino/ketcar_hoverserial/)

**Verdrahtung kurz:** Host TX → Board **PB4** (Rx), Host RX ← Board **PB6** (Tx), GND; 3,3‑V-Logik (bei 5‑V-Host Pegelwandler). Baud **115200**, Startframe **`0xABCD`**, Checksum = start ⊕ steer ⊕ speed. Frames alle ~25 ms (Firmware-Timeout ~0,8 s).

