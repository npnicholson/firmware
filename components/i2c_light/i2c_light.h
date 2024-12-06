#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace i2c_light {

class I2CLight : public light::LightOutput, public i2c::I2CDevice {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    return traits;
  }
  void write_state(light::LightState *state) override;
 protected:
};

}  // namespace i2c_light
}  // namespace esphome