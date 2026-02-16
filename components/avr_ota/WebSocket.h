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

#pragma once

#include "esphome/components/socket/socket.h"

namespace esphome {
namespace avr_ota {

// programmer states
typedef enum {
    WebSocketIdle = 0,    // no active TCP session
    WebSocketConnected,   // TCP connected
    WebSocketShutdown,    // WebSocket is not online at all
} WebSocketStatus_t;

class WebSocket {
 public:

  WebSocket();

  // Getter and setter for the web socket port
  void set_port(uint16_t port) { this->port_ = port; }
  uint16_t get_port() { return this->port_; }

  bool start();
  void stop();
  void close();
  bool handle();

  bool read(uint8_t *buf);
  bool read_bytes(uint8_t *buf, size_t len);
  bool write(char b);
  bool write_bytes(uint8_t *buf, size_t len);
  bool print(const char *buf);

  bool is_running();

  WebSocketStatus_t status;

 protected:
  bool readall_(uint8_t *buf, size_t len);
  bool writeall_(const uint8_t *buf, size_t len);

  uint16_t port_;
  std::unique_ptr<socket::Socket> server_;
  std::unique_ptr<socket::Socket> client_;
};

}  // namespace empty_web_socket
}  // namespace esphome