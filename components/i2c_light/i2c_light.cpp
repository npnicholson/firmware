#include "i2c_light.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome {
namespace i2c_light {

static const char *const TAG = "i2c_light";

void I2CLight::write_state(light::LightState *state) {
    float bright;
    state->current_values_as_brightness(&bright);

    // Convert the brightness from a 0-1 float to a 0-255 uint8_t and send
    uint8_t b = (bright * 255);
    uint8_t brightness[1] = { b };
    this->write(brightness, 1);
  }

}  // namespace i2c_light
}  // namespace esphome
