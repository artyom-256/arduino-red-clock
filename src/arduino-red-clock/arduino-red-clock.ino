/**
 * Copyright (c) 2021 Artem Hlumov <artyom.altair@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <DS3231.h>

// Our 7-segment display has the following pins:
//
//    D4  A  F  D3 D2 B
//     |  |  |  |  |  |
//  ----------------------
// |  __   __   __   __   |
// | |__| |__| |__| |__|  |
// | |__|.|__|.|__|.|__|. |
// |                      |
//  ----------------------
//     |  |  |  |  |  |
//     E  D  DP C  G  D1
//
// Pins A, B, C, D, E, F, G are anods (+) and correspond to the particular segments of one digit:
//
//   _A_
// F|   |
//  |_G_|B
// E|   |
//  |___|C
//    D
// Pin DP represents a digit dot.
// Four pins D1, D2, D3, D4 are catodes (-) and correspond to four digits we have on the display.
// So in order to display a digit we should close the circuit and apply potential difference between segments we want to light up
// and the catode that corresponds to the digit we want to show.
// For example, if we apply +5v to A, B, C, D1, D3, D4 and connect D2 to the ground we will create potential difference between
// the segments and the second digit catode, but not D1, D3, D4. This means only the second digit will light up and show "7".
// If we connect all D1, D2, D3, D4 to the ground, the "7" will appear in all positions.
// Therefore having 12 pins overall we can display any digit in one or several positions, but we cannot display
// different digits at the same time.
// To work it around we can display digits one by one with enough big frequency so that the human eye won't recognize any flickering.

// ====================================================
//                         PINS
// ====================================================

const int DISPLAY_PIN_A = 12;
const int DISPLAY_PIN_B = 8;
const int DISPLAY_PIN_C = 5;
const int DISPLAY_PIN_D = 3;
const int DISPLAY_PIN_E = 2;
const int DISPLAY_PIN_F = 11;
const int DISPLAY_PIN_G = 6;
const int DISPLAY_PIN_DP = 4;
const int DISPLAY_PIN_D1 = 13;
const int DISPLAY_PIN_D2 = 10;
const int DISPLAY_PIN_D3 = 9;
const int DISPLAY_PIN_D4 = 7;

// ====================================================
//                    CONFIGURATION
// ====================================================

/**
 * Now much time the digit should be displayed before switching to another position.
 * Big number may lead to noticeable blinking of digits.
 * The value is in milliseconds.
 */
const int DIGIT_DISPLAY_DELAY = 1;
/**
 * How often do we read the real time clock to get the new value.
 * The value is in milliseconds.
 */
const int READ_TIME_PERIOD = 1000;
/**
 * How often the middle dot blinks.
 * The value defines duration in milliseconds of the blink period that consists of switch on and off phases.
 */
const int DOT_BLINK_PERIOD = 1000;

// ====================================================
//                    DISPLAY CLASS
// ====================================================

/**
 * Provide an API to display digits on the 7-segment display.
 */
class Display {
public:
  /**
   * Constructor.
   * Initialize the object with pins connected to the display.
   * See above diagram for the particular meaning of each pin.
   */
  Display(int pin_a, int pin_b, int pin_c, int pin_d,
          int pin_e, int pin_f, int pin_g, int pin_dp,
          int pin_d1, int pin_d2, int pin_d3, int pin_d4)
  {
    // Save pins to the array.
    m_pins[Pins::A] = pin_a;
    m_pins[Pins::B] = pin_b;
    m_pins[Pins::C] = pin_c;
    m_pins[Pins::D] = pin_d;
    m_pins[Pins::E] = pin_e;
    m_pins[Pins::F] = pin_f;
    m_pins[Pins::G] = pin_g;
    m_pins[Pins::DP] = pin_dp;
    m_pins[Pins::D1] = pin_d1;
    m_pins[Pins::D2] = pin_d2;
    m_pins[Pins::D3] = pin_d3;
    m_pins[Pins::D4] = pin_d4;
    // Initialize pins.
    for (int i = 0; i < Pins::COUNT; i++) {
      pinMode(m_pins[i], OUTPUT);
      digitalWrite(m_pins[i], LOW);
    }
  }
  /**
   * Draw a digit at the given position.
   * Optionally display a dot.
   * @param pos Position of the digit (0-3).
   * @param digit Digit to draw (0-9).
   * @param displayDot Define whether the dot should be displayed.
   */
  void drawAt(int pos, int digit, bool displayDot = false)
  {
    // To avoid side-effects turn off all segments first.
    clean();
    // Set up which segments should light up keeping catodes in HIGH, so switched off.
    setDigit(digit);
    // Display the dot if requested.
    digitalWrite(m_pins[Pins::DP], displayDot ? HIGH : LOW);
    // Draw the digit on the requested position and light up segments.
    setPosition(pos);
    // Make a delay to make sure the digit becomes visible.
    delay(DIGIT_DISPLAY_DELAY);
  }

private:
  // Configuration of pins to display the particular digit.
  // Bits represent which segments should be siwtched on.
  // The order is following: A, B, C, D, E, F, G, 0. The least significant bit is always 0.
  static const byte D_0 = B11111100;
  static const byte D_1 = B01100000;
  static const byte D_2 = B11011010;
  static const byte D_3 = B11110010;
  static const byte D_4 = B01100110;
  static const byte D_5 = B10110110;
  static const byte D_6 = B10111110;
  static const byte D_7 = B11100000;
  static const byte D_8 = B11111110;
  static const byte D_9 = B11110110;
  /**
   * Array where each index maps to the corresponding pin configuration.
   * Used to simplify conversion from the digit to its configuration.
   */
  static const byte m_digits[10];
  /**
   * Enum that contains indices of pins in m_pins array.
   */
  enum Pins { A, B, C, D, E, F, G, DP, D1, D2, D3, D4, COUNT };
  /**
   * Map Pins enum to the particular values provided in the constructor.
   */
  int m_pins[Pins::COUNT];

  /**
   * Clean up the display.
   */
  void clean()
  {
    // Set all catodes to HIGH to clean up all digits.
    digitalWrite(m_pins[Pins::D1], HIGH);
    digitalWrite(m_pins[Pins::D2], HIGH);
    digitalWrite(m_pins[Pins::D3], HIGH);
    digitalWrite(m_pins[Pins::D4], HIGH);
  }
  /**
   * Setup the display to draw the given digit.
   * @param digit Digit to draw (0-9).
   */
  void setDigit(int digit)
  {
    // Use the mask that highlights one bit to go through the digit configuration
    // and check which segments should light up.
    // Start from the most significant one.
    byte mask = B10000000;
    // There are 7 bits in the configuration and the last one is always 0, so should be ignored.
    for (int i = 0; i < 7; i++) {
      digitalWrite(m_pins[i], (m_digits[digit] & mask) ? HIGH : LOW);
      // Switch to the next bit.
      mask = (mask >> 1);
    }
  }
  /**
   * Set up the display to draw at the particular position.
   * @param pos Position to draw at.
   */
  void setPosition(int pos)
  {
    // Selected position should be set to LOW in order to create potentials difference
    // and all other pins should be set to HIGH to prevent displaying anything on their positions.
    digitalWrite(m_pins[Pins::D1], pos == 0 ? LOW : HIGH);
    digitalWrite(m_pins[Pins::D2], pos == 1 ? LOW : HIGH);
    digitalWrite(m_pins[Pins::D3], pos == 2 ? LOW : HIGH);
    digitalWrite(m_pins[Pins::D4], pos == 3 ? LOW : HIGH);
  }
};

// Initialize map of digits to configurations.
static const byte Display::m_digits[10] = { D_0, D_1, D_2, D_3, D_4, D_5, D_6, D_7, D_8, D_9 };

// ====================================================
//                    VARIABLES
// ====================================================

/**
 * Real time clock module controller.
 */
DS3231 clock;
/**
 * Seven-segment display of four digits controller.
 */
Display d(DISPLAY_PIN_A, DISPLAY_PIN_B, DISPLAY_PIN_C, DISPLAY_PIN_D, DISPLAY_PIN_E, DISPLAY_PIN_F, DISPLAY_PIN_G,
          DISPLAY_PIN_DP, DISPLAY_PIN_D1, DISPLAY_PIN_D2, DISPLAY_PIN_D3, DISPLAY_PIN_D4);
/**
 * Current time and date value.
 */
RTCDateTime currentDateTime;
/**
 * Timestamp that defines when the data time value has been read.
 */
unsigned long lastReadTimestamp;

// ====================================================
//                 SETUP AND MAIN LOOP
// ====================================================

void setup()
{
  // Initialize DS3231 clock module.
  clock.begin();
  // Set up compilation time to synchronize the clock.
  clock.setDateTime(__DATE__, __TIME__);
  // Read initial time value.
  currentDateTime = clock.getDateTime();
  lastReadTimestamp = millis();
}

void loop()
{
  // Update time value every READ_TIME_PERIOD milliseconds.
  if (millis() - lastReadTimestamp >= READ_TIME_PERIOD) {
    currentDateTime = clock.getDateTime();
    lastReadTimestamp = millis();
  }
  // Blink the middle dot every DOT_BLINK_PERIOD milliseconds.
  // One blink consists of turning on and off the dot for equal period of time.
  // If the blink period is DOT_BLINK_PERIOD we should turn on the dot for DOT_BLINK_PERIOD / 2 milliseconds.
  bool blinkingDotState = millis() % DOT_BLINK_PERIOD >= DOT_BLINK_PERIOD / 2;
  // Drasw the first digit of hours.
  d.drawAt(0, currentDateTime.hour / 10);
  // Drasw the second digit of hours and blink the dot.
  d.drawAt(1, currentDateTime.hour % 10, blinkingDotState);
  // Drasw first digit of minutes.
  d.drawAt(2, currentDateTime.minute / 10);
  // Drasw second digit of minutes.
  d.drawAt(3, currentDateTime.minute % 10);
}
