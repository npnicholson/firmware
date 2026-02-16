# AVR OTA Socket Programmer

An ESPHome component that turns an ESP32 into a wireless AVR ISP programmer. It runs a TCP socket server that speaks the STK500v1 protocol, allowing tools like `avrdude` to program a connected AVR microcontroller (e.g. ATmega328P) over the network via SPI.

## Wiring

Connect the ESP32 SPI pins to the AVR's ISP header:

| ESP32 Pin | AVR ISP Pin |
|-----------|-------------|
| CLK       | SCK         |
| MOSI      | MOSI        |
| MISO      | MISO        |
| GPIO (configurable) | RESET (active low) |

The `avr_enable_output` GPIO controls the AVR reset/enable line.

## Configuration

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/npnicholson/firmware
      ref: master
    components: [ avr_ota ]

spi:
  - clk_pin: GPIOXX
    miso_pin: GPIOXX
    mosi_pin: GPIOXX

output:
  - platform: gpio
    id: avr_enable
    pin: GPIO02

avr_ota:
  id: avr
  avr_enable_output: avr_enable
  port: 328               # Optional, default: 328
  restore_mode: ALWAYS_OFF # Optional, default: ALWAYS_OFF
```

### Configuration Variables

- **id** (**Required**, string): Component ID.
- **avr_enable_output** (**Required**, ID): A `gpio` binary output connected to the AVR reset/enable pin.
- **port** (*Optional*, int): TCP port for the socket server. Default: `328`.
- **restore_mode** (*Optional*, enum): Controls whether the socket server is enabled on boot. Default: `ALWAYS_OFF`.
  - `ALWAYS_OFF` / `ALWAYS_ON`
  - `RESTORE_DEFAULT_OFF` / `RESTORE_DEFAULT_ON`
  - `RESTORE_INVERTED_DEFAULT_OFF` / `RESTORE_INVERTED_DEFAULT_ON`
  - `RESTORE_AND_OFF` / `RESTORE_AND_ON`

### Triggers

- **on_enable**: Fires when the socket server starts.
- **on_disable**: Fires when the socket server stops.
- **on_avr_idle**: Fires when the AVR programmer returns to the idle state (no active TCP session).
- **on_avr_pending**: Fires when a client connects but programming has not yet started.
- **on_avr_active**: Fires when the programmer enters active programming mode.
- **on_avr_failure**: Fires on a forced shutdown (socket failure or error).

### Actions

- **avr_ota.enable**: Start the socket server.
- **avr_ota.disable**: Stop the socket server.
- **avr_ota.toggle**: Toggle the socket server on/off.
- **avr_ota.reset**: Reset the AVR coprocessor.

### Conditions

- **avr_ota.is_enabled** / **avr_ota.is_disabled**: Check if the socket server is running.
- **avr_ota.is_idle**: No active TCP session.
- **avr_ota.is_pending**: Client connected, waiting for programming to start.
- **avr_ota.is_active**: Programming in progress.
- **avr_ota.is_error**: Forced shutdown state.

## Example

A full example configuration is available in [`examples/components/avr_ota/avr_ota_example.yaml`](../../../examples/components/avr_ota/avr_ota_example.yaml). It demonstrates:

- A template switch to enable/disable the programmer from Home Assistant
- A button to reset the AVR
- Automatic disable after programming completes (using `on_avr_idle`)

```yaml
avr_ota:
  id: avr
  restore_mode: ALWAYS_OFF
  avr_enable_output: avr_enable
  on_avr_idle:
    then:
      - avr_ota.disable: avr

switch:
  - platform: template
    name: AVR Programmer
    lambda: |-
      if (id(avr_enabled).state) return true;
      else return false;
    turn_on_action:
      then:
        - avr_ota.enable: avr
    turn_off_action:
      then:
        - avr_ota.disable: avr

button:
  - platform: template
    name: Restart AVR
    on_press:
      then:
        - avr_ota.reset: avr
```

## Programming with avrdude

Once the socket server is enabled, use `avrdude` to flash the AVR over the network:

```bash
avrdude -c stk500v1 -p m328p -P net:<ESP_IP>:328 -b 19200 -U flash:w:firmware.hex:i
```

Replace `<ESP_IP>` with the IP address of the ESP32 and adjust `-p` for your target AVR device.

## Acknowledgments

This component is based on the following projects:

- **[OTA_update_AVR_using_ESP32](https://github.com/ESP32-Musings/OTA_update_AVR_using_ESP32)** by Laukik Hase — MIT License
- **[ESP_AVRISP](https://github.com/lbernstone/ESP_AVRISP)** by Larry Bernstone — BSD License, based on ArduinoISP by Randall Bohn with WiFi support by Kiril Zyapkov