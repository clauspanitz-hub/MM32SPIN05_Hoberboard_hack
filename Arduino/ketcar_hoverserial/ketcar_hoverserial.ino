/*
 * Ketcar Host-UART: Gas / Bremse → Hoverboard MM32SPIN05
 *
 * Protokoll (Firmware MM32SPIN05_Hoberboard_hack / EFeru-kompatibel):
 *   Frame: start=0xABCD, steer (int16), speed (int16), checksum = start XOR steer XOR speed
 *   Baud:  115200 8N1
 *   Timeout Firmware: ~0,8 s ohne gültigen Frame → Motor OPEN_MODE / cmd 0
 *   Sendeintervall hier: 25 ms (deutlich unter Timeout)
 *
 * Primärziel: ESP32 (Hardware-UART). Classic Uno/AVR: siehe Abschnitt PINNING unten.
 *
 * Hoverboard-Stecker (Firmware setup.c / pinout):
 *   Board PB6 = Tx  →  Host RX
 *   Board PB4 = Rx  →  Host TX
 *   GND gemeinsam
 *   MCU-Logik 3,3 V — bei 5-V-Host (Uno) Pegelwandler oder zumindest Spannungsteiler Host-TX→Board-RX
 *
 * Sicherheit:
 *   - Optionaler Deadman/Enable (INPUT_ENABLE): HIGH = freigeben, LOW/offen = speed 0
 *   - Harte Bremse (Pedal über Schwelle) → speed 0
 *   - Kein Enable / Lose Enable → speed 0 (Frames laufen weiter, Timeout vermeiden)
 *
 * steer bleibt 0 (ein Motor pro Board; Dual-Board später separat).
 */

// ########################## BOARD / PINS ##########################
/*
 * ESP32 (Standard — Ketcar):
 *   PIN_HOVER_RX  16  ← Board Tx (PB6)
 *   PIN_HOVER_TX  17  → Board Rx (PB4)
 *   PIN_THROTTLE  34  Analog Gaspedal (ADC1, nur Input)
 *   PIN_BRAKE     35  Analog Bremse  (ADC1, nur Input)
 *   PIN_ENABLE    25  Optional Deadman: HIGH = fahren, LOW = Stop (interner Pull-up)
 *   Debug: USB Serial (Serial) @ 115200
 *
 * Arduino Uno / AVR (kurz):
 *   SoftwareSerial oder AltSoftSerial: RX=2, TX=3 (oder andere freie Pins)
 *   PIN_THROTTLE  A0
 *   PIN_BRAKE     A1
 *   PIN_ENABLE    4   (INPUT_PULLUP)
 *   Debug: Hardware Serial @ 115200 — Hover-UART dann nur SoftwareSerial
 *   ADC 10 Bit (0…1023); Mapping unten nutzt ADC_MAX
 */

#if defined(ESP32)
  #define PIN_HOVER_RX   16
  #define PIN_HOVER_TX   17
  #define PIN_THROTTLE   34
  #define PIN_BRAKE      35
  #define PIN_ENABLE     25
  #define HOVER_UART     Serial2
  #define ADC_MAX        4095
#else
  // Classic AVR / Uno — SoftwareSerial
  #include <SoftwareSerial.h>
  #define PIN_HOVER_RX   2
  #define PIN_HOVER_TX   3
  #define PIN_THROTTLE   A0
  #define PIN_BRAKE      A1
  #define PIN_ENABLE     4
  SoftwareSerial HoverSerial(PIN_HOVER_RX, PIN_HOVER_TX);
  #define HOVER_UART     HoverSerial
  #define ADC_MAX        1023
#endif

// ########################## CONFIG ##########################
#define HOVER_SERIAL_BAUD   115200
#define DEBUG_SERIAL_BAUD   115200
#define START_FRAME         0xABCD
#define TIME_SEND_MS        25      // 20–50 ms empfohlen; Firmware-Timeout ~800 ms

#define SPEED_MAX           1000    // Firmware-Limits typ. −1000…+1000
#define THROTTLE_DEADBAND   40      // ADC-Rohwert totzone um Leerlauf
#define BRAKE_HARD_THRESH   ((ADC_MAX * 30) / 100)  // ~30 % Pedalweg = harte Bremse → speed 0

// Pedal-Kalibrierung (Roh-ADC). Anpassen: Leerlauf und Vollgas messen (Serial-Debug).
#define THROTTLE_MIN        200
#define THROTTLE_MAX        (ADC_MAX - 100)
#define BRAKE_MIN           200
#define BRAKE_MAX           (ADC_MAX - 100)

// Optional: Enable-Pin nutzen. 0 = immer freigegeben (nur für Bench ohne Deadman).
#define USE_ENABLE_PIN      1

// ########################## TYPES ##########################
typedef struct {
  uint16_t start;
  int16_t  steer;
  int16_t  speed;
  uint16_t checksum;
} SerialCommand;

typedef struct {
  uint16_t start;
  int16_t  cmd;
  int16_t  cmd1;         // MM32-Port: Phasenstrom B (nicht EFeru-cmd2)
  int16_t  speed_meas;
  int16_t  speedR_meas;  // MM32-Port: Phasenstrom C
  int16_t  batVoltage;
  int16_t  boardTemp;
  uint16_t cmdLed;       // MM32-Port: DC-Strom
  uint16_t checksum;
} SerialFeedback;

SerialCommand  Command;
SerialFeedback Feedback;
SerialFeedback NewFeedback;

uint8_t  rxIdx = 0;
uint16_t bufStartFrame = 0;
byte    *rxPtr;
byte     incomingByte = 0;
byte     incomingBytePrev = 0;

unsigned long iTimeSend = 0;

// ########################## HELPERS ##########################
static int16_t mapConstrain(long x, long in_min, long in_max, long out_min, long out_max)
{
  if (in_max <= in_min) return (int16_t)out_min;
  if (x < in_min) x = in_min;
  if (x > in_max) x = in_max;
  long y = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  if (y < out_min) y = out_min;
  if (y > out_max) y = out_max;
  return (int16_t)y;
}

static bool enableOk(void)
{
#if USE_ENABLE_PIN
  // INPUT_PULLUP: Schalter nach GND öffnet → LOW = Stop
  return digitalRead(PIN_ENABLE) == HIGH;
#else
  return true;
#endif
}

static int16_t computeSpeed(int throttleRaw, int brakeRaw)
{
  // Harte Bremse hat Vorrang
  int brakeTravel = brakeRaw - BRAKE_MIN;
  if (brakeTravel < 0) brakeTravel = 0;
  if (brakeTravel >= (int)BRAKE_HARD_THRESH) {
    return 0;
  }

  if (!enableOk()) {
    return 0;
  }

  // Leichte Bremse: Gas drosseln (linear auf 0 bei Schwelle)
  int16_t speed = 0;
  int t = throttleRaw;
  if (t < (int)(THROTTLE_MIN + THROTTLE_DEADBAND)) {
    speed = 0;
  } else {
    speed = mapConstrain(t, THROTTLE_MIN + THROTTLE_DEADBAND, THROTTLE_MAX, 0, SPEED_MAX);
  }

  if (brakeTravel > 0) {
    // Je mehr Bremse, desto stärker Richtung 0 (noch unter harter Schwelle)
    long scale = (long)(BRAKE_HARD_THRESH - brakeTravel) * 1000L / BRAKE_HARD_THRESH;
    speed = (int16_t)((long)speed * scale / 1000L);
  }

  return speed;
}

// ########################## SEND / RECEIVE ##########################
void Send(int16_t uSteer, int16_t uSpeed)
{
  Command.start    = (uint16_t)START_FRAME;
  Command.steer    = uSteer;
  Command.speed    = uSpeed;
  Command.checksum = (uint16_t)(Command.start ^ Command.steer ^ Command.speed);
  HOVER_UART.write((uint8_t *)&Command, sizeof(Command));
}

void Receive(void)
{
  if (!HOVER_UART.available()) return;

  incomingByte   = HOVER_UART.read();
  bufStartFrame  = ((uint16_t)incomingByte << 8) | incomingBytePrev;

  if (bufStartFrame == START_FRAME) {
    rxPtr = (byte *)&NewFeedback;
    *rxPtr++ = incomingBytePrev;
    *rxPtr++ = incomingByte;
    rxIdx = 2;
  } else if (rxIdx >= 2 && rxIdx < sizeof(SerialFeedback)) {
    *rxPtr++ = incomingByte;
    rxIdx++;
  }

  if (rxIdx == sizeof(SerialFeedback)) {
    uint16_t checksum = (uint16_t)(
        NewFeedback.start ^ NewFeedback.cmd ^ NewFeedback.cmd1
        ^ NewFeedback.speed_meas ^ NewFeedback.speedR_meas
        ^ NewFeedback.batVoltage ^ NewFeedback.boardTemp ^ NewFeedback.cmdLed);

    if (NewFeedback.start == START_FRAME && checksum == NewFeedback.checksum) {
      memcpy(&Feedback, &NewFeedback, sizeof(SerialFeedback));
      Serial.print(F("FB cmd="));
      Serial.print(Feedback.cmd);
      Serial.print(F(" n="));
      Serial.print(Feedback.speed_meas);
      Serial.print(F(" Vbat="));
      Serial.print(Feedback.batVoltage);
      Serial.print(F(" T="));
      Serial.println(Feedback.boardTemp);
    } else {
      Serial.println(F("FB checksum skip"));
    }
    rxIdx = 0;
  }

  incomingBytePrev = incomingByte;
}

// ########################## SETUP / LOOP ##########################
void setup()
{
  Serial.begin(DEBUG_SERIAL_BAUD);
  delay(200);
  Serial.println(F("Ketcar hoverserial — Gas/Bremse → UART MM32SPIN05"));
  Serial.println(F("Wiring: Host TX→PB4, Host RX←PB6, GND; 3.3V logic / level shift if 5V host"));

#if defined(ESP32)
  HOVER_UART.begin(HOVER_SERIAL_BAUD, SERIAL_8N1, PIN_HOVER_RX, PIN_HOVER_TX);
#else
  HOVER_UART.begin(HOVER_SERIAL_BAUD);
#endif

  pinMode(PIN_ENABLE, INPUT_PULLUP);
#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
#endif
  // ESP32 ADC-Pins brauchen kein pinMode(INPUT); AVR Analog ok ohne

  Serial.print(F("Send every "));
  Serial.print(TIME_SEND_MS);
  Serial.println(F(" ms, steer=0"));
}

void loop()
{
  unsigned long now = millis();

  Receive();

  if ((long)(iTimeSend - now) > 0) return;
  iTimeSend = now + TIME_SEND_MS;

  int throttleRaw = analogRead(PIN_THROTTLE);
  int brakeRaw    = analogRead(PIN_BRAKE);
  int16_t speed   = computeSpeed(throttleRaw, brakeRaw);
  const int16_t steer = 0;

  Send(steer, speed);

  Serial.print(F("TX speed="));
  Serial.print(speed);
  Serial.print(F(" thr="));
  Serial.print(throttleRaw);
  Serial.print(F(" brk="));
  Serial.print(brakeRaw);
  Serial.print(F(" en="));
  Serial.println(enableOk() ? 1 : 0);

#if defined(LED_BUILTIN)
  digitalWrite(LED_BUILTIN, (now % 1000) < 50);
#endif
}
