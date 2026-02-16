# ESPHome Firmware

A collection of custom [ESPHome](https://esphome.io) components and voice assistant configurations.

## Components

### [AVR OTA](components/avr_ota)

Turns an ESP32 into a wireless AVR ISP programmer. Runs a TCP socket server speaking the STK500v1 protocol so you can flash a connected AVR microcontroller over the network using `avrdude`.

### [I2C Light](components/i2c_light)

A brightness-only light that sends a 0–255 value over I2C to a peripheral microcontroller. Designed for use with an AVR coprocessor handling AC dimming (leading or trailing edge).

## Voice Assistant Configs

Ready-to-use ESPHome configurations for voice assistant hardware:

- **[ESP32-S3-Box](voice-assistant/esp32-s3-box)** — Voice assistant config for the ESP32-S3-Box, with optional night mode and silence packages.
- **[M5Stack Atom Echo](voice-assistant/m5stack-atom-echo)** — Voice assistant config for the M5Stack Atom Echo, with an optional silence package.

## Usage

Add a component to your ESPHome project as an external component:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/npnicholson/firmware
      ref: master
    components: [ avr_ota ]  # or i2c_light
```

Example configurations are available in the [`examples/`](examples/) directory.

## License

MIT