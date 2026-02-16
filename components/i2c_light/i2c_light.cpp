/*
 * I2C Controlled Light
 * Copyright (c) 2024 Norris Nicholson
 *
 * Licensed under the MIT License. See LICENSE file in the project root.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
