#include <Arduino.h>
#include "OneButton.h"
#include <TM1637Display.h>

const uint8_t PIN_POWER      = 2;
const uint8_t PIN_POWER_HOLD = 27;  // PA5, HIGH keeps the board powered after the power button is released
const uint8_t PIN_POWER_LED  = 13;
const uint8_t PIN_READVOLT   = 53;
const uint8_t PIN_CLK        = 21;  // PD0
const uint8_t PIN_DIO        = 20;  // PD1
const uint8_t PIN_BUZZER     = 10;  // PB4
const uint8_t PIN_UP         = 25;  // PA3
const uint8_t PIN_DOWN       = 26;  // PA4
const uint8_t PIN_STOP       = 23;  // PA1
const uint8_t PIN_START      = 22;  // PA0
const uint8_t PIN_CYLINDER   = 37;  // PC0 (CYLINDER1, P11). P12 = 36 (PC1), P13 = 35 (PC2)
const uint8_t PIN_START_LED  = A9;   // PK1 (LED1)
const uint8_t PIN_STOP_LED   = A10;  // PK2 (LED2)
const uint8_t LED_ON         = LOW;  // LEDs are active-low

const int DEFAULT_SEC = 300;
const int MIN_SEC     = 60;
const int MAX_SEC     = 1800;
const int STEP_SEC    = 60;

const unsigned long DEBOUNCE_MS        = 30;
const unsigned long REPEAT_DELAY_MS    = 600;
const unsigned long REPEAT_INTERVAL_MS = 150;
const unsigned long CYLINDER_PERIOD_MS = 1000;
const unsigned long START_BLINK_MS     = 500;
const unsigned long DONE_PHASE_MS      = 1000;
const uint8_t       DONE_REPEAT        = 3;
const unsigned int  BUZZER_HZ          = 2500;
const uint8_t       FND_BRIGHTNESS     = 2;
const bool          DIGITS_REVERSED    = false;  // set true if "1234" shows up as "4321"

const uint8_t SEG_DONE[] = {
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
  SEG_C | SEG_E | SEG_G,                           // n
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
};

struct Button {
  uint8_t pin;
  bool pressed;
  bool lastRaw;
  unsigned long changedAt;
  unsigned long nextRepeat;
};

void buttonInit(Button &b) {
  pinMode(b.pin, INPUT_PULLUP);
  b.pressed = false;
  b.lastRaw = false;
  b.changedAt = 0;
  b.nextRepeat = 0;
}

bool buttonPressed(Button &b, unsigned long now) {
  bool raw = (digitalRead(b.pin) == LOW);
  if (raw != b.lastRaw) {
    b.lastRaw = raw;
    b.changedAt = now;
  }
  if (raw != b.pressed && now - b.changedAt >= DEBOUNCE_MS) {
    b.pressed = raw;
    if (raw) {
      b.nextRepeat = now + REPEAT_DELAY_MS;
      return true;
    }
  }
  return false;
}

bool buttonPressedOrRepeat(Button &b, unsigned long now) {
  if (buttonPressed(b, now)) return true;
  if (b.pressed && (long)(now - b.nextRepeat) >= 0) {
    b.nextRepeat += REPEAT_INTERVAL_MS;
    return true;
  }
  return false;
}

enum State { STATE_OFF, STATE_IDLE, STATE_RUNNING, STATE_DONE };

OneButton powerButton(PIN_POWER, true);
TM1637Display display(PIN_CLK, PIN_DIO);

Button btnUp    = { PIN_UP };
Button btnDown  = { PIN_DOWN };
Button btnStop  = { PIN_STOP };
Button btnStart = { PIN_START };

State state = STATE_OFF;
int count = DEFAULT_SEC;
int shownCount = -1;

unsigned long secondTimer   = 0;
unsigned long cylinderTimer = 0;
unsigned long blinkTimer    = 0;
unsigned long doneTimer     = 0;
uint8_t donePhase = 0;
bool cylinderOn = true;
bool startLedOn = false;

void setCylinder(bool on) {
  cylinderOn = on;
  digitalWrite(PIN_CYLINDER, on ? HIGH : LOW);
}

void setLeds(bool startLed, bool stopLed) {
  startLedOn = startLed;
  digitalWrite(PIN_START_LED, startLed ? LED_ON : !LED_ON);
  digitalWrite(PIN_STOP_LED, stopLed ? LED_ON : !LED_ON);
}

// Writes 4 digits left to right, flipping the order if the FND is wired reversed
void writeDigits(const uint8_t segs[4]) {
  uint8_t out[4];
  for (uint8_t i = 0; i < 4; i++) {
    out[i] = segs[DIGITS_REVERSED ? 3 - i : i];
  }
  display.setSegments(out);
}

// TM1637 writes are slow, so only redraw when the value changes
void showTime(int sec) {
  if (sec == shownCount) return;
  shownCount = sec;
  int mm = sec / 60;
  int ss = sec % 60;
  uint8_t segs[4] = {
    display.encodeDigit(mm / 10),
    (uint8_t)(display.encodeDigit(mm % 10) | 0x80),  // DP separates MM and SS
    display.encodeDigit(ss / 10),
    display.encodeDigit(ss % 10),
  };
  writeDigits(segs);
}

void enterOff() {
  state = STATE_OFF;
  count = DEFAULT_SEC;
  noTone(PIN_BUZZER);
  setCylinder(true);
  setLeds(false, false);
  digitalWrite(PIN_POWER_LED, LOW);
  digitalWrite(PIN_POWER_HOLD, LOW);  // power is cut when the button is released
  display.setBrightness(FND_BRIGHTNESS, false);
  display.clear();
  shownCount = -1;
}

void enterIdle() {
  state = STATE_IDLE;
  count = DEFAULT_SEC;
  noTone(PIN_BUZZER);
  setCylinder(true);
  setLeds(false, true);
  digitalWrite(PIN_POWER_LED, HIGH);
  digitalWrite(PIN_POWER_HOLD, HIGH);
  display.setBrightness(FND_BRIGHTNESS, true);
  shownCount = -1;
  showTime(count);
}

void enterRunning(unsigned long now) {
  state = STATE_RUNNING;
  secondTimer = now;
  cylinderTimer = now;
  blinkTimer = now;
  setCylinder(false);
  setLeds(true, false);
}

void doneStep() {
  if (donePhase % 2 == 0) {
    writeDigits(SEG_DONE);
    tone(PIN_BUZZER, BUZZER_HZ, DONE_PHASE_MS);
  } else {
    display.clear();
    noTone(PIN_BUZZER);
  }
  shownCount = -1;
}

void enterDone(unsigned long now) {
  state = STATE_DONE;
  setCylinder(true);
  setLeds(false, true);
  donePhase = 0;
  doneTimer = now;
  doneStep();
}

void powerOnOff() {
  if (digitalRead(PIN_READVOLT) != HIGH) return;
  if (state == STATE_OFF) enterIdle();
  else enterOff();
}

void setup() {
  pinMode(PIN_POWER_HOLD, OUTPUT);
  pinMode(PIN_POWER_LED, OUTPUT);
  pinMode(PIN_READVOLT, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_CYLINDER, OUTPUT);
  pinMode(PIN_START_LED, OUTPUT);
  pinMode(PIN_STOP_LED, OUTPUT);

  buttonInit(btnUp);
  buttonInit(btnDown);
  buttonInit(btnStop);
  buttonInit(btnStart);

  powerButton.attachLongPressStart(powerOnOff);

  enterOff();
}

void loop() {
  powerButton.tick();

  unsigned long now = millis();

  // Read every loop regardless of state to keep debounce state current
  bool upHit    = buttonPressedOrRepeat(btnUp, now);
  bool downHit  = buttonPressedOrRepeat(btnDown, now);
  bool stopHit  = buttonPressed(btnStop, now);
  bool startHit = buttonPressed(btnStart, now);

  switch (state) {
    case STATE_OFF:
      break;

    case STATE_IDLE:
      if (upHit)   count = min(count + STEP_SEC, MAX_SEC);
      if (downHit) count = max(count - STEP_SEC, MIN_SEC);
      if (stopHit) count = DEFAULT_SEC;
      showTime(count);
      if (startHit) enterRunning(now);
      break;

    case STATE_RUNNING:
      if (stopHit) {
        enterIdle();
        break;
      }
      if (now - secondTimer >= 1000) {
        secondTimer += 1000;
        count--;
        if (count <= 0) {
          count = 0;
          showTime(0);
          enterDone(now);
          break;
        }
      }
      if (now - cylinderTimer >= CYLINDER_PERIOD_MS) {
        cylinderTimer += CYLINDER_PERIOD_MS;
        setCylinder(!cylinderOn);
      }
      if (now - blinkTimer >= START_BLINK_MS) {
        blinkTimer += START_BLINK_MS;
        setLeds(!startLedOn, false);
      }
      showTime(count);
      break;

    case STATE_DONE:
      if (stopHit) {
        enterIdle();
        break;
      }
      if (now - doneTimer >= DONE_PHASE_MS) {
        doneTimer += DONE_PHASE_MS;
        donePhase++;
        if (donePhase >= DONE_REPEAT * 2) enterIdle();
        else doneStep();
      }
      break;
  }
}
