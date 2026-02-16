/*
 * AVR OTA Socket Programmer
 * Copyright (c) 2024 Norris Nicholson
 *
 * Based on:
 *   OTA_update_AVR_using_ESP32 by Laukik Hase (MIT License)
 *   https://github.com/ESP32-Musings/OTA_update_AVR_using_ESP32
 *
 *   ESP_AVRISP by Larry Bernstone (BSD License)
 *   https://github.com/lbernstone/ESP_AVRISP
 *   Based on ArduinoISP by Randall Bohn, with WiFi support by Kiril Zyapkov.
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

#include "automation.h"
#include "esphome/core/log.h"

namespace esphome {
namespace avr_ota {

static const char *const TAG = "avr_ota.automation";

}  // namespace avr_ota
}  // namespace esphome
