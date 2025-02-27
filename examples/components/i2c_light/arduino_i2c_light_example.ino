/* Arduino Firmware to dim an AC light as a co-processor in conjunction with the i2c_light ESPHome component.

   This program can be broken down into three parts.

   1) A I2C client on 0x55 that receives single uint8_t bytes (0-255) brightness value commands from esphome
   2) A leading or trailing edge dimmer (based on a mode selection input pin) that is timed to the AC zero
      crossing point. This could also be set using the I2C interface with some modification, or it could
      be hard coded as needed.
   3) A Serial interface running at 19200 baud for debugging (primarily co-processor -> esphome)

   The I2C interface and the zero crossing detector use interrupts to function. The main loop handles waiting
   the correct amount of time after a zero crossing to change the state of the output.
*/

//// Pin definitions ////

// Board LED pin
#define LED 5 // PD5

// Digital input pin for setting the dimming mode (leading or trailing edge dimming)
#define DIMMING_MODE A1 // PC1

// Digital input pin for detecting the AC Zero crossing point
#define ZERO_CROSSING 2 // PD2

// Digital output pin for controlling the load using a pair of MOSFETS
#define DIM_CONTROL 9 // PB1

// Timing variables
//
// A single AC half wave (the time between two zero crossings) occurs at 120Hz. This single half wave
// is referred to here as a cycle, and takes 8333 us to complete (assuming a perfect 60Hz AC signal)
//
// Single Cycle (time ->)
//  ___________________________________
// |  SM  |     Dimming Zone    |  EM  |
// |______|_____________________|______|
// Start                             End
// 0us                            8333us
//
// The Cycle Time Start Margin (SM) is the time after the zero crossing point where we will never dim
// the light. The Cycle Time End Margin (EM) is the time before which we will be done dimming. These
// margin values account for the fact that a typical dimmable LED bulb does not need the entire 8333us
// cycle time to become fully bright. Without these margins, the LED would reach full brightness about
// half way through the cycle (which would skew the 0-255 input).
#define CYCLE_TIME_START_MARGIN 2200
#define CYCLE_TIME_END_MARGIN 3000
#define CYCLE_TIME 8333 // (1/120 * 1000000)

#include <Wire.h>
#include <digitalWriteFast.h>

// #define ENABLE_HEARTBEAT_LED

/** Brightness value obtained from the ESP32. This is a uint8_t value between 0 and 255 */
volatile uint8_t brightness;

#ifdef ENABLE_HEARTBEAT_LED
/** Interlal counter for keeping track of when to blink the internal LED */
uint8_t led_count;
#endif

/** Flag for the dimming mode */
bool mode_trailing_edge_dimming;

/** Time in micro seconds to delay after the zero crossing point before acting again */
long delay_time;

/** Time at which the zero crossing occurred in micro seconds */
volatile unsigned long zero_time;

/** Time at which the last zero crossing occurred in micro seconds */
volatile unsigned long last_zero_time;

/** Time at which the callback occurred in micro seconds */
unsigned long callback_time;

/** Flag that indicates that the a second zero crossing occurred before the first was handled */
volatile bool zero_overrun;

/** Flag that indicates the zero crossing has occurred */
volatile bool zero_crossing_flag;

// the setup function runs once when you press reset or power the board
void setup()
{
    //// Initialize pins ////
#ifdef ENABLE_HEARTBEAT_LED
    pinMode(LED, OUTPUT);
#endif
    pinMode(DIMMING_MODE, INPUT_PULLUP);
    pinMode(ZERO_CROSSING, INPUT);
    pinMode(DIM_CONTROL, OUTPUT);
    digitalWriteFast(DIM_CONTROL, LOW);

    //// Initialize flags and vars ////
    zero_crossing_flag = false;
    mode_trailing_edge_dimming = true;
    zero_time = 0;
    delay_time = 0;
    last_zero_time = 0;
    callback_time = 0;
    brightness = 0;

    // Activate external interrupt on the ZERO_CROSSING pin at a falling edge
    attachInterrupt(digitalPinToInterrupt(ZERO_CROSSING), zerocrossing, FALLING);

    // Activate interrupts
    sei();

    // Start the Serial Interface
    Serial.begin(19200);
    Serial.println("AVR Co-processor Boot");

    // Start the I2C interface as a client on address 0x55
    Wire.begin(0x55);
    Wire.onReceive(I2C_RxHandler);
}

/** IRQ handler which is called during each AC zero crossing.
 *  Primary function is to set the zero crossing flag to true
 */
void zerocrossing()
{

    // If the Zero Crossing Flag is still true, then we did not handle it
    // during the main loop. This means we had a zero crossing overrun
    if (zero_crossing_flag == true) {
        zero_overrun = true;
        return;
    } else {
        last_zero_time = zero_time;
        zero_time = micros();
        zero_crossing_flag = true;
    }
}

void loop()
{

#ifdef ENABLE_HEARTBEAT_LED
    // Heartbeat Led tuned to blink around every half second
    if (led_count == 0) {
        digitalWriteFast(LED, HIGH);
    } else if (led_count == 60) {
        digitalWriteFast(LED, LOW);
    } else if (led_count == 120) {
        led_count = 255;
    }
    led_count++;
#endif

    // Detect the dimming mode
    mode_trailing_edge_dimming = digitalReadFast(DIMMING_MODE);

    // Calculate the length of time for the callback. Should be between the buffer length and the
    // full cycle time. For trailing edge dimming, zero brightness means the delay will be almost
    // the entire cycle time
    if (mode_trailing_edge_dimming)
        delay_time = map(brightness, 0, 255, CYCLE_TIME - CYCLE_TIME_END_MARGIN, CYCLE_TIME_START_MARGIN);
    else
        delay_time = map(brightness, 0, 255, CYCLE_TIME_START_MARGIN, CYCLE_TIME - CYCLE_TIME_END_MARGIN);

    // Wait for the zero crossing
    while (!zero_crossing_flag);

    // If the brightness is set to 0 or 255, then just set the output pin as needed. Otherwise. we will
    // dim the output
    if (brightness == 0) {
        digitalWriteFast(DIM_CONTROL, LOW);
    } else if (brightness == 255) {
        digitalWriteFast(DIM_CONTROL, HIGH);
    } else {

        // Zero crossing has happened
        // For trailing edge dimming, the light will start off, then turn on for the end of the cycle
        if (mode_trailing_edge_dimming) {
            digitalWriteFast(DIM_CONTROL, LOW);
            // For leading edge dimming, the light will start on, then turn off for the end of the cycle
        } else {
            digitalWriteFast(DIM_CONTROL, HIGH);
        }

        // Wait until the delay expires
        while (micros() - zero_time < delay_time);

        // Record the callback time
        callback_time = micros() - zero_time;

        // For trailing edge dimming, the light will end on
        if (mode_trailing_edge_dimming) {
            digitalWriteFast(DIM_CONTROL, HIGH);
            // For leading edge dimming, the light will end off
        } else {
            digitalWriteFast(DIM_CONTROL, LOW);
        }
    }

    // Detect if there was a zero crossing overrun and print a warning character if so
    if (zero_overrun) {
        Serial.write('#');
        zero_overrun = false;
    }

    zero_crossing_flag = false;
}

/**
 * I2C Callback handler
 * Assigns the brightness based on I2C data
 */
void I2C_RxHandler(int numBytes) {
    while (Wire.available()) brightness = Wire.read();
}
