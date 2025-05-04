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

OutputState_t currentState = OutputState_t::IDLE;
uint32_t stateEndTime = 0;
bool lastWasDit = false; // Tracks whether last output was a dit
NextElement_t nextElement = NextElement_t::NONE;

// SysEx tokens
const uint8_t sysex_header[] = SYSEX_HEADER;
const uint8_t sysex_footer = SYSEX_FOOTER;
uint8_t sysExBuffer[MAX_SYSEX_LENGTH];
uint8_t sysExLength = 0;

// Adafruit_NeoPixel strip(1, 16, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel *strip = nullptr;

// Control Surface variables
USBMIDI_Interface midi;
MIDIAddress address;

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
  if (settings.keyMode == keyMode_t::KEY_STRAIGHT)
  {
    pinMode(settings.gpio.straightKey, INPUT); // Reset input pin
  }
  else if (settings.keyMode == keyMode_t::KEY_PADDLES)
  {
    // Default to normal Input for pins
    pinMode(settings.gpio.ditPaddle, INPUT); // Reset input pin
    pinMode(settings.gpio.dahPaddle, INPUT); // Reset input pin
  }
}

/** Reset GPIO output, if enabled */
void cleanUpOutput()
{
  if (settings.gpioOutputMode == gpioOutputMode_t::OUTPUT_DISABLED)
    return;

  // Turn switch GPIO back to input
  pinMode(settings.gpio.output, INPUT);
}

/** Configures the GPIO Output */
void setupOutput()
{
  if (settings.gpioOutputMode != gpioOutputMode_t::OUTPUT_DISABLED)
  {
    PinStatus initialState = (settings.gpioOutputMode == gpioOutputMode_t::OUTPUT_NORMAL) ? LOW : HIGH;
    pinMode(settings.gpio.output, OUTPUT);
    digitalWrite(settings.gpio.normalLED, initialState); // Set initial output
  }
}

/** Turns the GPIO output on or off */
void setOutput(bool state)
{
  if (settings.gpioOutputMode == gpioOutputMode_t::OUTPUT_DISABLED)
    return;

  bool setState = (settings.gpioOutputMode == gpioOutputMode_t::OUTPUT_NORMAL) ? state : !state;

  PinStatus outputState = (setState) ? HIGH : LOW;

  digitalWrite(settings.gpio.output, outputState);
}

/** Turns the LED on or off */
void setLed(bool state)
{
  if (settings.ledMode == ledMode_t::LED_NORMAL)
  {
    digitalWrite(settings.gpio.normalLED, state ? HIGH : LOW); // On or off
  }
  else if (settings.ledMode == ledMode_t::LED_RGB)
  {
    strip->setPixelColor(0, state ? strip->Color(255, 0, 0) : strip->Color(0, 0, 0)); // Red or off
    strip->show();
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
  if (settings.keyMode == keyMode_t::KEY_STRAIGHT)
  {
    pinMode(settings.gpio.straightKey, settings.pinMode);
  }
  else if (settings.keyMode == keyMode_t::KEY_PADDLES)
  {
    pinMode(settings.gpio.ditPaddle, settings.pinMode);
    pinMode(settings.gpio.dahPaddle, settings.pinMode);
  }
}

/** Turn off LED and reset input or deactivate */
void cleanUpLED()
{
  // Cleanup current LED
  if (settings.ledMode == ledMode_t::LED_NORMAL)
  {
    // Turn off onboard LED and reset pin
    digitalWrite(settings.gpio.normalLED, LOW);
    pinMode(settings.gpio.normalLED, INPUT);
  }
  else if (settings.ledMode == ledMode_t::LED_RGB)
  {
    strip->clear(); // Set all pixels to off
    strip->show();  // Update to show off state
  }
}

/** Configures the LED */
void setupLed()
{
  if (settings.ledMode == ledMode_t::LED_NORMAL)
  {
    pinMode(settings.gpio.normalLED, OUTPUT);
    digitalWrite(settings.gpio.normalLED, LOW); // Start with LED off
  }
  else if (settings.ledMode == ledMode_t::LED_RGB)
  {
    if (strip == nullptr)
    {
      strip = new Adafruit_NeoPixel(NUM_LEDS, settings.gpio.rgbLED, NEOPIXELTYPE);
      strip->begin();
    }

    strip->clear(); // Set all pixels to off
    strip->show();
    strip->setBrightness(NEOPIXELBRIGHTNESS);
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
  packer.addField(settings.gpioOutputMode & 0x3, 2);
  packer.addField(settings.gpio.output & 0x7F, 7);
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
  settings.gpioOutputMode = (gpioOutputMode_t)packer.extractField(2);
  settings.gpio.output = packer.extractField(7);
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

/** Applies received SysEx configuration */
void setConfig(const uint8_t *data, unsigned int length)
{
  // Clear/clean up before applying
  cleanUpKey();
  cleanUpOutput();
  cleanUpLED();

  decodeConfig(settings, &data[sizeof(sysex_header) + 1], length - sizeof(sysex_header) - 1);

  // Apply new config
  setupKey();
  setupOutput();
  setupLed();
  setupWPM();
  setupMidi();
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
    setConfig(data, length);
    break;
  }
  case CMD_SAVE_CONFIG: // Config save request
  {
    setConfig(data, length);
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
  currentState = OutputState_t::OUTPUT_ON;

  // MIDI.sendNoteOn(settings.note, settings.volume, settings.channel);
  midi.sendNoteOn(address, settings.volume);

  stateEndTime = millis() + (isDit ? settings.timings.dit : settings.timings.dah);
  lastWasDit = isDit;
  nextElement = NextElement_t::NONE; // Reset nextElement when starting a new output

  setOutput(true);
  setLed(true);
}

/** Processes the straight key. */
void processStraightKey()
{
  if (straightKey.currentState && currentState == OutputState_t::IDLE)
  {
    midi.sendNoteOn(address, settings.volume);
    setOutput(true);
    setLed(true);
    currentState = OutputState_t::OUTPUT_ON;
  }
  if (!straightKey.currentState && currentState == OutputState_t::OUTPUT_ON)
  {
    midi.sendNoteOff(address, settings.volume);
    setOutput(false);
    setLed(false);
    currentState = OutputState_t::IDLE;
  }
}

/** Processes the iambic keyer state machine. */
void processIambic()
{
  if (currentState == OutputState_t::OUTPUT_ON)
  {
    if (millis() >= stateEndTime)
    {
      midi.sendNoteOff(address, settings.volume);

      currentState = OutputState_t::OUTPUT_OFF;
      stateEndTime = millis() + settings.timings.gap;

      setOutput(false);
      setLed(false);
    }
    else
    {
      // During output, check if the opposite paddle is pressed
      if (lastWasDit && dahPaddle.currentState)
      {
        nextElement = NextElement_t::DAH; // Queue a dah next
      }
      else if (!lastWasDit && ditPaddle.currentState)
      {
        nextElement = NextElement_t::DIT; // Queue a dit next
      }
    }
  }
  else if (currentState == OutputState_t::OUTPUT_OFF)
  {
    if (millis() >= stateEndTime)
    {
      if (nextElement == NextElement_t::DIT)
      {
        startIambicOutput(true); // Send queued dit
      }
      else if (nextElement == NextElement_t::DAH)
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
        currentState = OutputState_t::IDLE; // Nothing pressed, go idle
      }
    }
  }
  else if (currentState == OutputState_t::IDLE)
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
  settings.gpioOutputMode = DEFAULT_OUTPUTMODE;
  settings.gpio.output = DEFAULT_GPIO_OUTPUT;
  settings.gpio.normalLED = DEFAULT_GPIO_NORMALDLED;
  settings.gpio.rgbLED = DEFAULT_GPIO_RGBLED;
  settings.gpio.ditPaddle = DEFAULT_GPIO_DITPADDLE;
  settings.gpio.dahPaddle = DEFAULT_GPIO_DAHPADDLE;
  settings.gpio.straightKey = DEFAULT_GPIO_STRAIGHT;
  settings.wpm = DEFAULT_WPM;
  settings.channel = DEFAULT_MIDI_CHANNEL;
  settings.note = DEFAULT_MIDI_NOTE;
  settings.volume = DEFAULT_MIDI_VOLUME;
}

void setup()
{
  //Serial.begin(115200);

  setDefaultSettings();
  init(settings);

  TinyUSBDevice.setManufacturerDescriptor(MANUFACTURER);
  TinyUSBDevice.setProductDescriptor(PRODUCT);
  while (!TinyUSBDevice.mounted())
    delay(1); // Wait for USB to mount

  midi.begin();
  midi.setCallbacks(callback);

  setupKey();
  setupLed();
  setupOutput();
  setupMidi();
  setupWPM();
}

void loop()
{
  midi.update();

  if (settings.keyMode == keyMode_t::KEY_STRAIGHT)
  {
    updateKeyState(straightKey, settings.gpio.straightKey);
    processStraightKey();
  }
  else if (settings.keyMode == keyMode_t::KEY_PADDLES)
  {
    updateKeyState(ditPaddle, settings.gpio.ditPaddle);
    updateKeyState(dahPaddle, settings.gpio.dahPaddle);
    processIambic();
  }
}