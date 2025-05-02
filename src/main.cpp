#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Control_Surface.h>
#include <Adafruit_TinyUSB.h>
#include <BitPacker.hpp>
#include "main.h"
#include "nvram.h"

Settings_t settings;

// Sraight Key/Paddle variables
PaddleState_t straightKey = {false, false, 0};
PaddleState_t ditPaddle = {false, false, 0};
PaddleState_t dahPaddle = {false, false, 0};

OutputState_t currentState = IDLE;
uint32_t stateEndTime = 0;
bool lastWasDit = false; // Tracks whether last output was a dit
int nextElement = 0;     // 0: none, 1: dit, 2: dah

// SysEx tokens
const uint8_t sysex_header[] = SYSEX_HEADER;
const uint8_t sysex_footer = SYSEX_FOOTER;
uint8_t sysExBuffer[MAX_SYSEX_LENGTH];
uint8_t sysExLength = 0;

// Control Surface variables
USBMIDI_Interface midi;
MIDIAddress address;

// Onboard RGB LED
Adafruit_NeoPixel pixel(NUM_LEDS, DEFAULT_RGBLED, NEO_GRB + NEO_KHZ800);

/** Clear MIDI send and reset inputs for key GPIOs */
void cleanUpKey()
{
  if (currentState == OutputState_t::OUTPUT_ON)
  {
    // Currently sending, need to stop and turn off the LED
    midi.sendNoteOff(address, settings.volume);
    currentState = OutputState_t::IDLE;
  }

  // Cleanup current Key
  if (settings.keyMode == keyMode_t::STRAIGHTKEY)
  {
    pinMode(settings.gpio.straightKey, INPUT); // Reset input pin
  }
  else if (settings.keyMode == keyMode_t::PADDLES)
  {
    // Default to normal Input for pins
    pinMode(settings.gpio.ditPaddle, INPUT); // Reset input pin
    pinMode(settings.gpio.dahPaddle, INPUT); // Reset input pin
  }
}

/** Turn off LED and reset input or deactivate */
void cleanUpLED()
{
  // Cleanup current LED
  if (settings.ledMode == ledMode_t::NORMAL)
  {
    // Turn off onboard LED and reset pin
    digitalWrite(settings.gpio.normalLED, LOW);
    pinMode(settings.gpio.normalLED, INPUT);
  }
  else if (settings.ledMode == ledMode_t::RGB)
  {
    // Turn off NeoPixel and stop output
    pixel.clear(); // Set all pixels to off
    pixel.show();  // Update to show off state
  }
}

/** Turns the LED on or off */
void setLed(bool state)
{
  if (settings.ledMode == ledMode_t::NORMAL)
  {
    digitalWrite(settings.gpio.normalLED, state ? HIGH : LOW); // On or off
  }
  else if (settings.ledMode == ledMode_t::RGB)
  {
    pixel.setPixelColor(0, state ? pixel.Color(255, 0, 0) : pixel.Color(0, 0, 0)); // Red or off
    pixel.show();
  }
}

/** Sets or updates the word per minute timings for paddle mode */
void setupWPM()
{
  settings.timings.dit = (INTTOFLOATSCALAR * 1200) / settings.wpm;
  settings.timings.dah = settings.timings.dit * 3;
  settings.timings.gap = settings.timings.dit;
}

/** Sets or updates the channel, note, and volume details for the MIDI output */
void setupMidi()
{
  address = MIDIAddress(settings.note, Channel(settings.channel - 1));
}

/** Configures the key mode and GPIO pins */
void setupKey()
{
  if (settings.keyMode == keyMode_t::STRAIGHTKEY)
  {
    pinMode(settings.gpio.straightKey, settings.pinMode);
  }
  else if (settings.keyMode == keyMode_t::PADDLES)
  {
    pinMode(settings.gpio.ditPaddle, settings.pinMode);
    pinMode(settings.gpio.dahPaddle, settings.pinMode);
  }
}

/** Configures the LED */
void setupLed()
{
  if (settings.ledMode == ledMode_t::NORMAL)
  {
    pinMode(settings.gpio.normalLED, OUTPUT);
    digitalWrite(settings.gpio.normalLED, LOW); // Start with LED off
  }
  else if (settings.ledMode == ledMode_t::RGB)
  {
    pixel.setPin(settings.gpio.rgbLED);

    pixel.clear(); // Ensure off initially
    pixel.show();
  }
}

/** Encode firmware version for sending over SysEx */
void encodeVersion(const uint16_t version, uint8_t *out, uint8_t &outSize)
{
  BitPacker packer(32);

  packer.addField(version & 0xFFFF, 16);
  packer.pack7Bit(out, outSize);
}

/** Send current configuration as SysEx */
void sendVersion()
{
  uint8_t packedSize;

  sysExLength = sizeof(sysex_header);

  memcpy(sysExBuffer, sysex_header, sysExLength);
  sysExBuffer[sysExLength++] = CMD_GET_VERSION;

  encodeVersion(VERSION, &sysExBuffer[sysExLength], packedSize);

  sysExLength += packedSize;
  sysExBuffer[sysExLength++] = SYSEX_FOOTER;

  // Send SysEx
  midi.sendSysEx(sysExBuffer, sysExLength, address.getCableNumber());
}

/** Encode Config struct into a contiguous 7-bit buffer */
void encodeConfig(const Settings_t &settings, uint8_t *out, uint8_t &outSize)
{
  BitPacker packer(MAX_SYSEX_LENGTH * 8);

  // Add fields in MSB-to-LSB order
  packer.addField(settings.keyMode & 0x3, 2);
  packer.addField(settings.pinMode & 0x3, 2);
  packer.addField(settings.ledMode & 0x3, 2);
  packer.addField(settings.gpio.normalLED & 0x7F, 7);
  packer.addField(settings.gpio.rgbLED & 0x7F, 7);
  packer.addField(settings.gpio.ditPaddle & 0x7F, 7);
  packer.addField(settings.gpio.dahPaddle & 0x7F, 7);
  packer.addField(settings.gpio.straightKey & 0x7F, 7);
  packer.addField(settings.wpm & 0xFFFF, 16);
  packer.addField(settings.channel & 0x7F, 7);
  packer.addField(settings.note & 0x7F, 7);
  packer.addField(settings.volume & 0x7F, 7);
  packer.pack7Bit(out, outSize);
}

/** Encode Config struct into a contiguous 7-bit buffer */
void decodeConfig(Settings_t &settings, const uint8_t *input, uint8_t inputSize)
{
  BitPacker packer(MAX_SYSEX_LENGTH * 8);

  packer.unpack7Bit(input, inputSize);

  settings.keyMode = (keyMode_t)packer.extractField(2);
  settings.pinMode = (PinMode)packer.extractField(2);
  settings.ledMode = (ledMode_t)packer.extractField(2);
  settings.gpio.normalLED = packer.extractField(7);
  settings.gpio.rgbLED = packer.extractField(7);
  settings.gpio.ditPaddle = packer.extractField(7);
  settings.gpio.dahPaddle = packer.extractField(7);
  settings.gpio.straightKey = packer.extractField(7);
  settings.wpm = (uint16_t)packer.extractField(16);
  settings.channel = packer.extractField(7);
  settings.note = packer.extractField(7);
  settings.volume = packer.extractField(7);
}

/** Send current configuration as SysEx */
void sendConfig()
{
  uint8_t packedSize;

  sysExLength = sizeof(sysex_header);

  memcpy(sysExBuffer, sysex_header, sysExLength);
  sysExBuffer[sysExLength++] = CMD_GET_CONFIG;

  encodeConfig(settings, &sysExBuffer[sysExLength], packedSize);

  sysExLength += packedSize;
  sysExBuffer[sysExLength++] = SYSEX_FOOTER;

  // Send SysEx
  midi.sendSysEx(sysExBuffer, sysExLength, address.getCableNumber());
}

/** Handle received SysEx */
void handleSysEx(const uint8_t *data, unsigned int length)
{
  if (length < sizeof(sysex_header) + 2)
    return; // Too short

  // Check SysEx prefix using memcmp
  if (memcmp(data, sysex_header, sizeof(sysex_header)) != 0)
    return; // Invalid prefix

  // Check end byte
  if (data[length - 1] != sysex_footer)
    return; // Missing end byte

  // Extract command (byte after prefix)
  uint8_t command = data[sizeof(sysex_header)];

  switch (command)
  {
  case CMD_GET_VERSION: // Detection request
  {
    sendVersion();
    break;
  }
  case CMD_GET_CONFIG: // Config request
  {
    sendConfig();
    break;
  }
  case CMD_SET_CONFIG: // Config request
  {
    cleanUpKey();
    cleanUpLED();

    decodeConfig(settings, &data[sizeof(sysex_header) + 1], length - sizeof(sysex_header) - 1);

    setupKey();
    setupLed();
    setupWPM();
    setupMidi();
    break;
  }
  case CMD_SAVE_CONFIG: // Config save request
  {
    save(settings);
    break;
  }
  case CMD_REBOOT: // Reboot request
  {
    rp2040.reboot();
    break;
  }
  case CMD_BOOTSEL: // Firmware update mode
  {
    reset_usb_boot(0, 0);
    break;
  }
  }
}

/** Callback for Control Surface to receive a MIDI message */
struct MyMIDI_Callbacks : MIDI_Callbacks
{

  // This callback function is called when a SysEx message is received.
  void onSysExMessage(MIDI_Interface &, SysExMessage sysex) override
  {
    handleSysEx(sysex.data, sysex.length);
  }
} callback{};

/** Updates the debounced state of a key/paddle based on its current reading. */
void updateKeyState(PaddleState_t &paddle, int pin)
{
  bool reading = !digitalRead(pin); // Active low due to pull-up
  if (reading != paddle.lastReading)
  {
    paddle.lastChangeTime = millis();
  }
  if (millis() - paddle.lastChangeTime >= DEBOUNCE_TIME)
  {
    paddle.currentState = reading;
  }
  paddle.lastReading = reading;
}

/** Starts sending a dit or dah by turning on the output and setting the duration. */
void startIambicOutput(bool isDit)
{
  currentState = OUTPUT_ON;

  // MIDI.sendNoteOn(settings.note, settings.volume, settings.channel);
  midi.sendNoteOn(address, settings.volume);

  stateEndTime = millis() + (isDit ? settings.timings.dit : settings.timings.dah);
  lastWasDit = isDit;
  nextElement = 0; // Reset nextElement when starting a new output

  setLed(true);
}

/** Processes the straight key. */
void processStraightKey()
{
  if (straightKey.currentState && currentState == IDLE)
  {
    midi.sendNoteOn(address, settings.volume);
    setLed(true);
    currentState = OutputState_t::OUTPUT_ON;
  }
  if (!straightKey.currentState && currentState == OUTPUT_ON)
  {
    midi.sendNoteOff(address, settings.volume);
    setLed(false);
    currentState = OutputState_t::IDLE;
  }
}

/** Processes the iambic keyer state machine. */
void processIambic()
{
  if (currentState == OUTPUT_ON)
  {
    if (millis() >= stateEndTime)
    {
      midi.sendNoteOff(address, settings.volume);

      currentState = OUTPUT_OFF;
      stateEndTime = millis() + settings.timings.gap;

      setLed(false);
    }
    else
    {
      // During output, check if the opposite paddle is pressed
      if (lastWasDit && dahPaddle.currentState)
      {
        nextElement = 2; // Queue a dah next
      }
      else if (!lastWasDit && ditPaddle.currentState)
      {
        nextElement = 1; // Queue a dit next
      }
    }
  }
  else if (currentState == OUTPUT_OFF)
  {
    if (millis() >= stateEndTime)
    {
      if (nextElement == 1)
      {
        startIambicOutput(true); // Send queued dit
      }
      else if (nextElement == 2)
      {
        startIambicOutput(false); // Send queued dah
      }
      else if (ditPaddle.currentState)
      {
        startIambicOutput(true); // Send dit if Dit Paddle is pressed
      }
      else if (dahPaddle.currentState)
      {
        startIambicOutput(false); // Send dah if Dah Paddle is pressed
      }
      else
      {
        currentState = IDLE; // Nothing pressed, go idle
      }
    }
  }
  else if (currentState == IDLE)
  {
    if (ditPaddle.currentState)
    {
      startIambicOutput(true); // Start with dit
    }
    else if (dahPaddle.currentState)
    {
      startIambicOutput(false); // Start with dah
    }
  }
}

/** Default values from main.h */
void setDefaultSettings()
{
  settings.version = VERSION;
  settings.keyMode = DEFAULT_KEYMODE;
  settings.pinMode = DEFAULT_PINMODE;
  settings.ledMode = DEFAULT_LEDMODE;
  settings.gpio.normalLED = DEFAULT_NORMALDLED;
  settings.gpio.rgbLED = DEFAULT_RGBLED;
  settings.gpio.ditPaddle = DEFAULT_DITPADDLE;
  settings.gpio.dahPaddle = DEFAULT_DAHPADDLE;
  settings.gpio.straightKey = DEFAULT_STRAIGHTKEY;
  settings.wpm = DEFAULT_WPM;
  settings.channel = DEFAULT_CHANNEL;
  settings.note = DEFAULT_NOTE;
  settings.volume = DEFAULT_VOLUME;
}

void setup()
{
  setDefaultSettings();

  //Serial.begin(115200);
  TinyUSBDevice.setManufacturerDescriptor(MANUFACTURER);
  TinyUSBDevice.setProductDescriptor(PRODUCT);
  while (!TinyUSBDevice.mounted())
    delay(1); // Wait for USB to mount

  midi.begin();
  midi.setCallbacks(callback);

  init(settings);

  setupKey();
  setupLed();
  setupWPM();
  setupMidi();
}

void loop()
{
  // MIDI.read();
  midi.update();

  if (settings.keyMode == keyMode_t::STRAIGHTKEY)
  {
    updateKeyState(straightKey, settings.gpio.straightKey);
    processStraightKey();
  }
  else if (settings.keyMode == keyMode_t::PADDLES)
  {
    updateKeyState(ditPaddle, settings.gpio.ditPaddle);
    updateKeyState(dahPaddle, settings.gpio.dahPaddle);
    processIambic();
  }
}
