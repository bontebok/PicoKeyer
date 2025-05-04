#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>

// Firmware compatability
#define VERSION 0x2

// USB MIDI Config
#define MANUFACTURER "bontebok"
#define PRODUCT "PicoKeyer"
#define SYSEX_HEADER {0xF0, 0x7D}
#define SYSEX_FOOTER 0xF7

// Define default settings
#define DEFAULT_WPM INTTOFLOATSCALAR * 12 // Fixed point value (WPM / INTTOFLOATSCALAR), allows for half/quarter timings if desired
#define DEFAULT_KEYMODE keyMode_t::KEY_NONE
#define DEFAULT_PINMODE PinMode::INPUT_PULLUP
#define DEFAULT_LEDMODE ledMode_t::LED_DISABLED
#define DEFAULT_OUTPUTMODE gpioOutputMode_t::OUTPUT_DISABLED
#define DEFAULT_GPIO_OUTPUT 14
#define DEFAULT_GPIO_DITPADDLE 3
#define DEFAULT_GPIO_DAHPADDLE 29
#define DEFAULT_GPIO_STRAIGHT 3
#define DEFAULT_GPIO_NORMALDLED 25
#define DEFAULT_GPIO_RGBLED 16
#define DEFAULT_MIDI_NOTE 77
#define DEFAULT_MIDI_CHANNEL 1
#define DEFAULT_MIDI_VOLUME 40

// RGB LED Settings
#define NEOPIXELTYPE NEO_GRB + NEO_KHZ800
#define NEOPIXELBRIGHTNESS 127

// SysEx Commands
#define CMD_GET_VERSION 0
#define CMD_GET_CONFIG 1
#define CMD_SET_CONFIG 2
#define CMD_SAVE_CONFIG 3
#define CMD_REBOOT 4
#define CMD_BOOTSEL 5

// Byte array SysEx buffer
#define MAX_SYSEX_LENGTH 32

// WS2812 LED setup
#define NUM_LEDS 1

// Timing constants (in milliseconds)
#define DEBOUNCE_TIME 10 // Debounce period in ms

// Int scalar value
#define INTTOFLOATSCALAR 100.0

// Settings file path
#define SETTINGS_FILE "/settings.bin"

// Key Modes
enum keyMode_t : uint8_t
{
    KEY_NONE,
    KEY_STRAIGHT,
    KEY_PADDLES
};

// LED Modes
enum ledMode_t : uint8_t
{
    LED_DISABLED,
    LED_NORMAL,
    LED_RGB
};

// GPIO Output Mode
enum gpioOutputMode_t : uint8_t
{
    OUTPUT_DISABLED,
    OUTPUT_NORMAL,
    OUTPUT_INVERSED
};

// GPIO pins
struct GPIO_t
{
    uint8_t normalLED;
    uint8_t rgbLED;
    uint8_t output;
    uint8_t ditPaddle;
    uint8_t dahPaddle;
    uint8_t straightKey;
};

// Timing values for iambic key
struct Timings_t
{
    uint dit;
    uint dah;
    uint gap;
};

// Menu structure
struct Settings_t
{
    uint16_t version;
    Timings_t timings;
    GPIO_t gpio;
    uint16_t wpm;
    keyMode_t keyMode;
    PinMode pinMode;
    ledMode_t ledMode;
    gpioOutputMode_t gpioOutputMode;
    uint8_t note;
    uint8_t channel;
    uint8_t volume;
};

// Paddle state tracking
struct PaddleState_t
{
    bool currentState;       // Current debounced state
    bool lastReading;        // Last raw reading
    uint32_t lastChangeTime; // Time of last state change
};

// Output state
enum OutputState_t : uint8_t
{
    IDLE,
    OUTPUT_ON,
    OUTPUT_OFF
};

// Next output
enum NextElement_t : uint8_t
{
    NONE,
    DIT,
    DAH
};

#endif