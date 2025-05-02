# PicoKeyer
PicoKeyer is a CW keyer for the Raspberry Pi Pico that allows you to practice CW (morse code) in Desktop or Mobile browser that supports the [Web MIDI API](https://developer.mozilla.org/en-US/docs/Web/API/MIDIAccess#browser_compatibility). PicoKeyer also provide a USB MIDI interface that enables you to use any MIDI app of your choice to record or listen to your CW.

[PicoKeyer Browser App](https://bontebok.github.io/PicoKeyer/)

![image](https://github.com/user-attachments/assets/5bd73a71-4009-45b7-8543-9921a7099cf8)

## Getting Started
You will need a Raspberry Pi Pico based on the RP2040 chip and a CW key (straight or paddles). The Pi Pico is available in a variety of configurations, any of them should work. If you do not already have a Pi Pico, I recommend picking up either an original Raspberry Pi Pico or a Waveshare RP2040-Zero board as I have tested both. You can find the original Raspberry Pi Pico, Waveshare RP2040-Zero, or a clone from $3-10/ea on Amazon, Aliexpress, or at retailers like Micro Center. If you want to use the case design included in the repo, you will need a Waveshare RP2040-Zero or clone.

You will also need a way to connnect your RP2040 to your key. Most keys use a standard 3.5MM (1/8") stereo jack, however bare wire will also work fine. If you want to use the standard 3.5MM jack, I recommend the Lsgoodcare headphone jacks in the link below.

Amazon Links (non-affiliate)
* [Raspberry Pi Pico (Quantity 2) $10](https://www.amazon.com/Raspberry-Development-Dual-core-Processor-Integrated/dp/B0CPMBRVDX/)
* [Waveshare RP2040 Zero (Quantity 3) $20](https://www.amazon.com/RP2040-Zero-High-Performance-Microcontroller-Castellated-Boards-3pcs/dp/B0B2ZFGSMD/)
* [Lsgoodcare 10PCS 3.5MM Female Headphone Stereo Jack](https://www.amazon.com/dp/B01CVGD4UI)

## Assembly

### Straight Key
The simplest configuration is a straight key as only two wires are needed. The common (ground) of the key should be wired to one of the Raspberry Pi Pico ground pins, while the other contact needs to be wired to any GPIO pin. Take note of the GPIO pin number as you will need to set that pin in the settings.

### Paddles
Three wires are needed to connect a paddle key to the PicoKeyer. The common (ground) of the key should be wired to one of the Raspberry Pi Pico ground pins, while the other two contacts (left and right paddle) need to be wired to any GPIO pin. Take note of each GPIO pin number as you will need to set that pin in the settings.

### Firmware Installation
Head over to [Releases](https://github.com/bontebok/PicoKeyer/releases) and download the latest PicoKeyer.uf2 firmware. Plug in your Raspberry Pi Pico and open the RPI-RP2 drive. If the drive does not appear, hold down the Boot button on the Pi Pico before plugging it it. Copy the PicoKeyer.uf2 file to the RPI-RP2 drive. Once the firmware has finished copying, open the [PicoKeyer Browser App](https://bontebok.github.io/PicoKeyer/) in a browser that supports the Web MIDI API to configure your PicoKeyer.
