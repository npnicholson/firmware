#include "esphome/core/log.h"
#include "WebSocket.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"

namespace esphome {
namespace avr_ota {

static const char *TAG = "avr_ota.web_socket";


WebSocket::WebSocket() {
    this->status = WebSocketShutdown;
}

bool WebSocket::start() {
  server_ = socket::socket_ip(SOCK_STREAM, 0);
  if (server_ == nullptr) {
    ESP_LOGW(TAG, "Could not create socket");
    return false;
  }
  int enable = 1;
  int err = server_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set reuseaddr: errno %d", err);
    // we can still continue
  }
  err = server_->setblocking(false);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set nonblocking mode: errno %d", err);
    return false;
  }

  struct sockaddr_storage server;

  socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), this->port_);
  if (sl == 0) {
    ESP_LOGW(TAG, "Socket unable to set sockaddr: errno %d", errno);
    return false;
  }

  err = server_->bind((struct sockaddr *) &server, sizeof(server));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to bind: errno %d", errno);
    return false;
  }

  err = server_->listen(4);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to listen: errno %d", errno);
    return false;
  }
  
  this->status = WebSocketIdle;
  ESP_LOGI(TAG, "Socket Started - Idle");
  return true;
}

void WebSocket::stop() {
    if (this->client_ != nullptr) {
        this->client_->close();
        this->client_ = nullptr;
    }
    this->server_->close();
    this->server_->shutdown(SHUT_RDWR);
    this->status = WebSocketShutdown;
}

void WebSocket::close() {
  status = WebSocketIdle;
  this->client_->close();
  this->client_ = nullptr;
}


// Handle the web socket connection. Returns true if a new connection has been established
bool WebSocket::handle() {
    // The web socket has not been started. Do nothing
    if (status == WebSocketShutdown) return false;

    // We are already connected to a client. Check to see if the web socket is still active
    else if (status == WebSocketConnected) {
        // TODO: Not sure how to do this
        return false;
    }

    // We are not connected to a client. See if we can establish a new connection
    else if (status == WebSocketIdle) {
        if (client_ == nullptr) {
            struct sockaddr_storage source_addr;
            socklen_t addr_len = sizeof(source_addr);
            client_ = server_->accept((struct sockaddr *) &source_addr, &addr_len);
        }
        if (client_ == nullptr)
            return false;

        // Mark this as an active connection
        status = WebSocketConnected;

        int enable = 1;
        int err = client_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
        if (err != 0) {
            ESP_LOGW(TAG, "Socket could not enable TCP nodelay, errno %d", errno);
            // return;
        }

        return true;
    }

    return false;
}

// Read a single byte from the socket
bool WebSocket::read(uint8_t *buf) {
    if (this->status != WebSocketConnected) return false;

    bool res = this->readall_(buf, 1);
    if (!res) this->close();
    return res;
}

// Read len bytes from the socket
bool WebSocket::read_bytes(uint8_t *buf, size_t len) {
  if (this->status != WebSocketConnected) return false;

    bool res = this->readall_(buf, len);
    if (!res) this->close();
    return res;
}

// Write a single byte to the socket
bool WebSocket::write(char b) {
  if (this->status != WebSocketConnected) return false;

    uint8_t buf[1] = { b };
    bool res = this->writeall_(buf, 1);
    if (!res) this->close();
    return res;
}

// Write a single byte to the socket
bool WebSocket::write_bytes(uint8_t *buf, size_t len) {
    if (this->status != WebSocketConnected) return false;

    bool res = this->writeall_(buf, len);
    if (!res) this->close();
    return res;
}

// Write a char array to the socket
bool WebSocket::print(const char* buf) {
    bool res = this->writeall_((const uint8_t *)buf, strlen(buf));
    if (!res) this->close();
    return res;
}

bool WebSocket::is_running() { 
  return this->status != WebSocketShutdown;
}

bool WebSocket::readall_(uint8_t *buf, size_t len) {
  uint32_t start = millis();
  uint32_t at = 0;
  while (len - at > 0) {
    uint32_t now = millis();
    if (now - start > 1000) {
      ESP_LOGW(TAG, "Timed out reading %d bytes of data", len);
      return false;
    }

    ssize_t read = this->client_->read(buf + at, len - at);
    if (read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        App.feed_wdt();
        delay(1);
        continue;
      }
      ESP_LOGW(TAG, "Failed to read %d bytes of data, errno %d", len, errno);
      return false;
    } else if (read == 0) {
      ESP_LOGW(TAG, "Remote closed connection");
      return false;
    } else {
      at += read;
    }
    App.feed_wdt();
    delay(1);
  }

  return true;
}
bool WebSocket::writeall_(const uint8_t *buf, size_t len) {
  uint32_t start = millis();
  uint32_t at = 0;
  while (len - at > 0) {
    uint32_t now = millis();
    if (now - start > 1000) {
      ESP_LOGW(TAG, "Timed out writing %d bytes of data", len);
      return false;
    }

    ssize_t written = this->client_->write(buf + at, len - at);
    if (written == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        App.feed_wdt();
        delay(1);
        continue;
      }
      ESP_LOGW(TAG, "Failed to write %d bytes of data, errno %d", len, errno);
      return false;
    } else {
      at += written;
    }
    App.feed_wdt();
    delay(1);
  }
  return true;
}

}  // namespace empty_web_socket
}  // namespace esphome