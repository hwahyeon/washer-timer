# Washer Timer

Timer firmware for a pneumatic cylinder washer.
It repeatedly actuates a pneumatic cylinder for the duration set on the 4-digit display, then indicates completion using the buzzer and display.

## Hardware

Custom ATmega2560-based board (16 MHz).

| Part | Type | Notes |
|---|---|---|
| MCU | ATmega2560 | 16 MHz crystal |
| Power | 24 V DC input | Soft power latch; 5 V supplied by an on-board DC/DC converter |
| Buttons | 5 tactile switches | Power, Start, Stop, UP, DOWN |
| LEDs | 2 | Start / Stop indicators |
| Display | TM1637 + 4-digit 7-segment display | No colon; decimal point separates minutes and seconds |
| Buzzer | Piezo | |
| Pneumatic cylinder | Solenoid valve (model TBD) | Uses one of three 24 V output channels |

## Pin Map

| Function | MCU Port | Arduino Pin | Polarity |
|---|---|---|---|
| Power button | PE4 | 2 | LOW when pressed |
| Power hold | PA5 | 27 | HIGH = keep power on |
| Start button | PA0 | 22 | LOW when pressed |
| Stop button | PA1 | 23 | LOW when pressed |
| UP button | PA3 | 25 | LOW when pressed |
| DOWN button | PA4 | 26 | LOW when pressed |
| Start LED (LED1) | PK1 | A9 | LOW = on |
| Stop LED (LED2) | PK2 | A10 | LOW = on |
| Display CLK | PD0 | 21 | |
| Display DIO | PD1 | 20 | |
| Buzzer | PB4 | 10 | HIGH = on |
| Cylinder 1 (P11) | PC0 | 37 | HIGH = on |
| Cylinder 2 (P12) | PC1 | 36 | HIGH = on |
| Cylinder 3 (P13) | PC2 | 35 | HIGH = on |

## Operation

**Power**
- Press and hold the Power button to turn the device on; press and hold it again to turn the device off.
- The board remains powered while the button is held. Once a long press is detected, the firmware sets the power hold pin HIGH.
- To turn off, the firmware sets the power hold pin LOW. Power is cut when the button is released.
- The device always starts in the stopped state.

**Stopped**
- The Stop LED is on, and the cylinder output remains on.
- Press UP or DOWN to adjust the time in 1-minute increments. Hold either button to repeat the adjustment continuously.
- The time can be set from 1 to 30 minutes; the default is 5 minutes.

**Running** (Start button)
- The display counts down once per second.
- The cylinder output toggles on and off every second.
- The Start LED blinks.
- Pressing Stop immediately halts operation and resets the timer to the default (5 minutes).

**Done** (timer reaches 0 seconds)
- The display shows `donE` and the buzzer sounds, blinking three times at 1-second intervals.
- The device then returns to the stopped state with the default time (5 minutes).

## Settings

Adjust the constants at the top of `WasherTimer/WasherTimer.ino`.
The following settings should be verified on the actual hardware.

| Constant | Default | When to change |
|---|---|---|
| `DIGITS_REVERSED` | `false` | Set to `true` if the digits appear in reverse order (`1234` → `4321`) |
| `PIN_START_LED` / `PIN_STOP_LED` | `A9` / `A10` | Swap the values if the Start and Stop LEDs are reversed |
| `PIN_CYLINDER` | `37` (P11) | Use `36` for P12 or `35` for P13 |

## Build and Upload

### Setup

1. Install the following libraries using the Arduino IDE Library Manager:
   - **OneButton** (Matthias Hertel)
   - **TM1637Display** (Avishay Orpaz), version 1.2.0 or later
2. Open `WasherTimer/WasherTimer.ino`.
3. Select **Arduino Mega or Mega 2560** as the board.

### Upload

The ATmega2560 must have the Arduino Mega bootloader installed.
Connect a 5 V USB-to-serial adapter to the board's `CON1` connector.

| Adapter | CON1 |
|---|---|
| 5 V | 1 (+5 V) |
| TX | 2 (RX) |
| RX | 3 (TX) |
| GND | 5 (GND) |

For uploading, it is recommended to disconnect the 24 V power supply and power the board from the USB-to-serial adapter's 5 V output.
