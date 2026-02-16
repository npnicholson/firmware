# I2C Controlled Light

An ESPHome light component that controls brightness over I2C. The ESP32 sends a single byte (0–255) representing the brightness level to an I2C peripheral at a configurable address. This is designed for use with an AVR coprocessor (or similar microcontroller) that handles the actual dimming — for example, an AC leading/trailing edge dimmer synchronized to zero-crossing detection.

## How It Works

The component registers as a brightness-only light in ESPHome. When the brightness changes, it converts the 0.0–1.0 float value to a 0–255 byte and writes it over I2C to the configured address. The receiving microcontroller is responsible for interpreting this value and driving the load.

An example Arduino sketch for the AVR coprocessor side is provided in [`examples/components/i2c_light/arduino_i2c_light_example.ino`](../../../examples/components/i2c_light/arduino_i2c_light_example.ino). It implements an I2C client on address `0x55` with interrupt-driven zero-crossing AC dimming (supporting both leading and trailing edge modes).

## Wiring

Connect the ESP32 I2C pins to the AVR coprocessor:

| ESP32 Pin | AVR Pin |
|-----------|---------|
| SCL       | SCL     |
| SDA       | SDA     |

Both buses should share a common ground and use appropriate pull-up resistors.

## Configuration

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/npnicholson/firmware
      ref: master
    components: [ i2c_light ]

i2c:
  - scl: GPIOXX
    sda: GPIOXX

light:
  - platform: i2c_light
    name: Light
    address: 0x55
    gamma_correct: 2.0
```

### Configuration Variables

- **name** (**Required**, string): The name of the light entity.
- **address** (**Required**, int): The I2C address of the peripheral device (e.g. `0x55`).
- **gamma_correct** (*Optional*, float): Gamma correction factor applied to the brightness curve. Default: `2.8`. A value of `1.0` disables correction.
- All other options from the [ESPHome Brightness Light](https://esphome.io/components/light/#brightness-only-light) are supported.

## Example

A full example configuration is available in [`examples/components/i2c_light/i2c_light_example.yaml`](../../../examples/components/i2c_light/i2c_light_example.yaml).

The companion Arduino sketch [`examples/components/i2c_light/arduino_i2c_light_example.ino`](../../../examples/components/i2c_light/arduino_i2c_light_example.ino) demonstrates the coprocessor side, implementing:

- An I2C client on address `0x55` that receives brightness values
- Zero-crossing detection via external interrupt for AC dimming
- Selectable leading or trailing edge dimming via a mode pin
- Serial output at 19200 baud for debugging