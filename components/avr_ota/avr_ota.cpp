#include "avr_ota.h"
#include "avr_commands.h"

// #include "esphome/components/md5/md5.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include "esphome/components/ota/ota_backend.h"

#include <cerrno>
#include <cstdio>

namespace esphome {
namespace avr_ota {

#define AVRISP_DEBUG(...)
#define AVRISP_HWVER 2
#define AVRISP_SWMAJ 1
#define AVRISP_SWMIN 18
#define AVRISP_PTIME 10
#define EECHUNK (32)
#define beget16(addr) (*addr * 256 + *(addr + 1))

static const char *TAG = "avr_ota.component";

void AVROTAComponent::store_state() {
  AVRStateRTCState saved;
  saved.enabled = this->is_enabled();
  this->rtc_.save(&saved);
}

// Enables the AVR programmer by creating the web socket server
bool AVROTAComponent::enable_avr() {
  // If the web socket is already running, then don't do anything
  if (this->is_enabled()) return true;

  ESP_LOGD(TAG, "Enabling AVR");

  bool res = this->socket.start();
  if (res) {
    ESP_LOGI(TAG, "Started Web Socket:");
    ESP_LOGI(TAG, "  Address: %s:%u", network::get_use_address(), this->port_);
    ESP_LOGI(TAG, "  $ avrdude -c stk500v1 -p m328p -P net:%s:%u -b 19200 ...", network::get_use_address(), 
            this->port_);
    
    this->store_state();
    this->enable_callback_.call();
  } else
    ESP_LOGW(TAG, "Error starting Web Socket");

  return res;
}

// Disables the AVR programmer by removing the web socket server
void AVROTAComponent::disable_avr() {
  // If the web socket isn't running, don't do anything
  if (!this->is_enabled()) return;

  ESP_LOGD(TAG, "Disabling AVR");

  // Set the ISP State to idle to prevent it from 
  this->_state = AVRISP_STATE_FORCED_SHUTDOWN;

  this->socket.stop();

  this->store_state();
  this->disable_callback_.call();
}

void AVROTAComponent::toggle() {
  if (this->is_enabled()) this->disable();
  else this->enable_avr();
}

// Returns true if the AVR Programmer is enabled (by having its socket active)
bool AVROTAComponent::is_enabled() {
  return this->socket.is_running();
}

void AVROTAComponent::reset() {
  if (this->_state != AVRISP_STATE_IDLE)
    this->_state = AVRISP_STATE_FORCED_SHUTDOWN;

  // Reset the AVR Device
  this->set_enable_(false);
  delay(10);
  this->set_enable_(true);
}

void AVROTAComponent::add_on_enable_callback(std::function<void(void)> &&callback) {
  this->enable_callback_.add(std::move(callback));
}

void AVROTAComponent::add_on_disable_callback(std::function<void(void)> &&callback) {
  this->disable_callback_.add(std::move(callback));
}

void AVROTAComponent::add_on_avr_callback(std::function<void(AVRISPState_t)> &&callback) {
  this->avr_callback_.add(std::move(callback));
}

// Setup from Component
void AVROTAComponent::setup() {
  // Set up the SPI bus
  this->spi_setup();
  this->spi_transaction_active = false;

  // The detault state for the AVR Enable pin should be the Enabled state
  // Note that this setup does not run until after wifi is connected, so it
  // may take a moment for the AVR to come online
  this->set_enable_(true);

  // Set up the web socket
  this->socket = WebSocket();
  this->socket.set_port(this->port_);

  // Restore the AVR as needed based on the restore state
  AVRStateRTCState recovered{};
  switch (this->restore_mode_) {
    case AVR_RESTORE_DEFAULT_OFF:
    case AVR_RESTORE_DEFAULT_ON:
    case AVR_RESTORE_INVERTED_DEFAULT_OFF:
    case AVR_RESTORE_INVERTED_DEFAULT_ON:
      this->rtc_ = global_preferences->make_preference<AVRStateRTCState>(this->get_object_id_hash());
      // Attempt to load from preferences, else fall back to default values
      if (!this->rtc_.load(&recovered)) {
        recovered.enabled = false;
        if (this->restore_mode_ == AVR_RESTORE_DEFAULT_ON ||
            this->restore_mode_ == AVR_RESTORE_INVERTED_DEFAULT_ON) {
          recovered.enabled = true;
        }
      } else if (this->restore_mode_ == AVR_RESTORE_INVERTED_DEFAULT_OFF ||
                 this->restore_mode_ == AVR_RESTORE_INVERTED_DEFAULT_ON) {
        // Inverted restore state
        recovered.enabled = !recovered.enabled;
      }
      break;
    case AVR_RESTORE_AND_OFF:
    case AVR_RESTORE_AND_ON:
      this->rtc_ = global_preferences->make_preference<AVRStateRTCState>(this->get_object_id_hash());
      this->rtc_.load(&recovered);
      recovered.enabled = (this->restore_mode_ == AVR_RESTORE_AND_ON);
      break;
    case AVR_ALWAYS_OFF:
      recovered.enabled = false;
      break;
    case AVR_ALWAYS_ON:
      recovered.enabled = true;
      break;
  }

  // Enable the AVR if needed by the restore mode
  if (recovered.enabled) {
    this->enable_avr();
  }
}

// Dump Config from Component
void AVROTAComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "AVR Update Port:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address(), this->port_);
  ESP_LOGCONFIG(TAG, "  $ avrdude -c stk500v1 -p m328p -P net:%s:%u -b 19200 ...", network::get_use_address(),
                this->port_);
}

// Main loop from Component
void AVROTAComponent::loop() {
  bool newConnection = this->socket.handle();

  // Update the ISP State based on the websocket
  this->isp_update();

  // Simple print statement if the state changed since last time
  if (this->_state != this->_last_state) {
    switch (this->_state) {
      case AVRISP_STATE_IDLE: {
        if (this->_last_state == AVRISP_STATE_ACTIVE)
          ESP_LOGI(TAG, "[AVRISP] Complete. Now idle");
        else
          ESP_LOGI(TAG, "[AVRISP] Now idle");
        break;
      }
      case AVRISP_STATE_PENDING: {
        ESP_LOGI(TAG, "[AVRISP] Connection pending");
        break;
      }
      case AVRISP_STATE_ACTIVE: {
        ESP_LOGI(TAG, "[AVRISP] Programming mode");
        break;
      }
      case AVRISP_STATE_FORCED_SHUTDOWN: {
        ESP_LOGI(TAG, "[AVRISP] Forced shutdown pending");
        break;
      }
    }
    // Execute the AVR Callback since the state changed
    this->avr_callback_.call(this->_state);
    this->_last_state = this->_state;
  }
}

// Set the enable gpio pin
void AVROTAComponent::set_enable_(bool state) {
  // ESP_LOGI(TAG, "Enable Pin Set %s", state ? "on" : "off");
  this->avr_enable_->set_state(state);
}

float AVROTAComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }
uint16_t AVROTAComponent::get_ws_port() const { return this->port_; }
void AVROTAComponent::set_ws_port(uint16_t port) { this->port_ = port; }

//// AVR ISP Defines ////

// Update the ISP State based on the websocket status
AVRISPState_t AVROTAComponent::isp_update() {
  switch (this->_state) {
    case AVRISP_STATE_FORCED_SHUTDOWN: {
      if (this->spi_transaction_active)
        this->disable();  // SPI.end();
      this->spi_transaction_active = false;

      // Reset the AVR Device
      this->set_enable_(false);
      delay(10);
      this->set_enable_(true);

      _state = AVRISP_STATE_IDLE;
      break;
    }
    case AVRISP_STATE_IDLE: {
      // If a client is now connected, move to pending
      if (this->socket.status == WebSocketConnected) {
        _state = AVRISP_STATE_PENDING;
      }
      break;
    }
    case AVRISP_STATE_PENDING: {
      if (this->socket.status == WebSocketConnected) {
        _state = AVRISP_STATE_ACTIVE;
      }
      break;
    }
    case AVRISP_STATE_ACTIVE: {
      // If the websocket is still active, then run the avr isp
      if (this->socket.status == WebSocketConnected) {
        _state = AVRISP_STATE_ACTIVE;
        avrisp();
      }
      // If the websocket is in any other state, then go idle
      else {
        // If we were in programming mode, then stop the spi transaction
        if (pmode) {
          if (this->spi_transaction_active)
            this->disable();  // SPI.end();
          this->spi_transaction_active = false;
          pmode = 0;
        }

        // Enable the AVR device
        this->set_enable_(true);

        _state = AVRISP_STATE_IDLE;
      }
      break;
    }
  }
  return this->_state;
}

AVRISPState_t AVROTAComponent::get_avr_state() { return this->_state; }

inline void AVROTAComponent::_reject_incoming(void) {
  // TODO: ?? Seems to limit the server to only one client, but these funcions don't exist in esphome
  // while (this->ws_server_.hasClient()) ws_server_.available().stop();
}

uint8_t AVROTAComponent::getch() {
  // Make a buffer of length 1
  uint8_t buf[1];

  // Read a single byte into the buffer. If the read times out, then set the state to idle
  if (!this->socket.read(buf)) {
    this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    buf[0] = '\00';
  }

  return buf[0];
}

void AVROTAComponent::fill(int n) {
  // Read n bytes into the main buffer. If the read times out, then set the state to idle
  if (!this->socket.read_bytes(this->buff, n)) {
    this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    for (int i = 0; i < n; i++) this->buff[i] = '\00';
  }
}

uint8_t AVROTAComponent::spi_transaction(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  this->transfer_byte(a);
  this->transfer_byte(b);
  this->transfer_byte(c);
  return this->transfer_byte(d);
}

void AVROTAComponent::empty_reply() {
  // ESP_LOGI(TAG, "[AVRISP] Empty reply");
  if (Sync_CRC_EOP == getch()) {
    
    // If the write fails, then we are done
    if (!this->socket.write(Resp_STK_INSYNC)) {
      this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
      return;
    }

    // If the write fails, then we are done
    if (!this->socket.write(Resp_STK_OK)) {
      this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
      return;
    }
  } else {
    error++;

    // If the write fails, then we are done
    if (!this->socket.write(Resp_STK_NOSYNC)) {
      this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
      return;
    }
  }
}

void AVROTAComponent::breply(uint8_t b) {
  // ESP_LOGI(TAG, "[AVRISP] Breply %02x", b);
  if (Sync_CRC_EOP == getch()) {
    uint8_t resp[3];
    resp[0] = Resp_STK_INSYNC;
    resp[1] = b;
    resp[2] = Resp_STK_OK;

    // If the write fails, then we are done
    if (!this->socket.write_bytes(resp, 3)) {
      this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
      return;
    }
  } else {
    error++;

    // If the write fails, then we are done
    if (!this->socket.write(Resp_STK_NOSYNC)) {
      this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
      return;
    }
  }
}

void AVROTAComponent::get_parameter(uint8_t c) {
  // ESP_LOGI(TAG, "[AVRISP] Get Parameger %02x", c);
  switch (c) {
    case 0x80:
      breply(AVRISP_HWVER);
      break;
    case 0x81:
      breply(AVRISP_SWMAJ);
      break;
    case 0x82:
      breply(AVRISP_SWMIN);
      break;
    case 0x93:
      breply('S');  // serial programmer
      break;
    default:
      breply(0);
  }
}

void AVROTAComponent::set_parameters() {
  // call this after reading paramter packet into buff[]
  param.devicecode = buff[0];
  param.revision = buff[1];
  param.progtype = buff[2];
  param.parmode = buff[3];
  param.polling = buff[4];
  param.selftimed = buff[5];
  param.lockbytes = buff[6];
  param.fusebytes = buff[7];
  param.flashpoll = buff[8];
  // ignore buff[9] (= buff[8])
  // following are 16 bits (big endian)
  param.eeprompoll = beget16(&buff[10]);
  param.pagesize = beget16(&buff[12]);
  param.eepromsize = beget16(&buff[14]);

  // 32 bits flashsize (big endian)
  param.flashsize = buff[16] * 0x01000000 + buff[17] * 0x00010000 + buff[18] * 0x00000100 + buff[19];
}

void AVROTAComponent::start_pmode() {
  // ESP_LOGI(TAG, "[AVRISP] Start PMode");
  if (!this->spi_transaction_active) this->enable();
  this->spi_transaction_active = true;

  // try to sync the bus
  this->transfer_byte(0x00);

  this->set_enable_(true);

  delayMicroseconds(50);

  this->set_enable_(false);

  delay(30);

  uint8_t buf[4] = {0xAC, 0x53, 0x00, 0x00};
  this->transfer_array(buf, 4);

  // ESP_LOGI(TAG, "Start Buffer %02x %02x %02x %02x", buf[0], buf[1], buf[2], buf[3]);
  pmode = 1;
}

void AVROTAComponent::end_pmode() {
  // ESP_LOGI(TAG, "[AVRISP] End PMode");
  if (this->spi_transaction_active) this->disable();
  this->spi_transaction_active = false;
  this->set_enable_(true);  // setReset(_reset_state);
  pmode = 0;
}

void AVROTAComponent::universal() {
  int w;
  uint8_t ch;

  fill(4);
  ch = spi_transaction(buff[0], buff[1], buff[2], buff[3]);
  breply(ch);
}

void AVROTAComponent::flash(uint8_t hilo, int addr, uint8_t data) {
  // ESP_LOGI(TAG, "[AVRISP] Flash");
  spi_transaction(0x40 + 8 * hilo, addr >> 8 & 0xFF, addr & 0xFF, data);
}

void AVROTAComponent::commit(int addr) {
  // ESP_LOGI(TAG, "[AVRISP] Commit");
  spi_transaction(0x4C, (addr >> 8) & 0xFF, addr & 0xFF, 0);
  delay(AVRISP_PTIME);
}

// #define _addr_page(x) (here & 0xFFFFE0)
int AVROTAComponent::addr_page(int addr) {
  // ESP_LOGI(TAG, "[AVRISP] Addr Page");
  if (param.pagesize == 32)
    return addr & 0xFFFFFFF0;
  if (param.pagesize == 64)
    return addr & 0xFFFFFFE0;
  if (param.pagesize == 128)
    return addr & 0xFFFFFFC0;
  if (param.pagesize == 256)
    return addr & 0xFFFFFF80;
  ESP_LOGW(TAG, "[AVRISP] unknown page size: %d", param.pagesize);
  return addr;
}

void AVROTAComponent::write_flash(int length) {
  // ESP_LOGI(TAG, "[AVRISP] Write Flash");
  uint32_t started = millis();

  fill(length);

  if (Sync_CRC_EOP == getch()) {
    // If the write fails, then we are done
    if (!this->socket.write(Resp_STK_INSYNC)) {
      this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
      return;
    }

    // If the write fails, then we are done
    if (!this->socket.write(write_flash_pages(length))) {
      this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
      return;
    }
  } else {
    error++;
    // If the write fails, then we are done
    if (!this->socket.write(Resp_STK_NOSYNC)) {
      this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
      return;
    }
  }
}

uint8_t AVROTAComponent::write_flash_pages(int length) {
  // ESP_LOGI(TAG, "[AVRISP] Write flash pages");
  int x = 0;
  int page = addr_page(here);
  while (x < length) {
    App.feed_wdt();
    yield();
    if (page != addr_page(here)) {
      commit(page);
      page = addr_page(here);
    }
    flash(0, here, buff[x++]);
    flash(1, here, buff[x++]);
    here++;
  }
  commit(page);
  return Resp_STK_OK;
}

uint8_t AVROTAComponent::write_eeprom(int length) {
  // ESP_LOGI(TAG, "[AVRISP] Write EEPROM");
  // here is a word address, get the byte address
  int start = here * 2;
  int remaining = length;
  if (length > param.eepromsize) {
    error++;
    return Resp_STK_FAILED;
  }
  while (remaining > EECHUNK) {
    write_eeprom_chunk(start, EECHUNK);
    start += EECHUNK;
    remaining -= EECHUNK;
    App.feed_wdt();
    yield();
  }
  write_eeprom_chunk(start, remaining);
  return Resp_STK_OK;
}
// write (length) bytes, (start) is a byte address
uint8_t AVROTAComponent::write_eeprom_chunk(int start, int length) {
  // ESP_LOGI(TAG, "[AVRISP] Write EEPROM Chunk");
  // this writes byte-by-byte,
  // page writing may be faster (4 bytes at a time)
  fill(length);
  // prog_lamp(0);
  for (int x = 0; x < length; x++) {
    int addr = start + x;
    spi_transaction(0xC0, (addr >> 8) & 0xFF, addr & 0xFF, buff[x]);
    App.feed_wdt();
    yield();
    delay(45);
  }
  // prog_lamp(1);
  return Resp_STK_OK;
}

void AVROTAComponent::program_page() {
  // ESP_LOGI(TAG, "[AVRISP] Program Page");
  char result = (char) Resp_STK_FAILED;
  int length = 256 * getch();
  length += getch();
  char memtype = getch();
  char buf[100];

  // flash memory @here, (length) bytes
  if (memtype == 'F') {
    write_flash(length);
    return;
  }

  if (memtype == 'E') {
    result = (char) write_eeprom(length);
    if (Sync_CRC_EOP == getch()) {
      // If the write fails, then we are done
      if (!this->socket.write(Resp_STK_INSYNC)) this->_state = AVRISP_STATE_FORCED_SHUTDOWN;

      // If the write fails, then we are done
      if (!this->socket.write(result)) this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    } else {
      error++;

      // If the write fails, then we are done
      if (!this->socket.write(Resp_STK_NOSYNC)) this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    }
    return;
  }
  // If the write fails, then we are done
  if (!this->socket.write(Resp_STK_FAILED)) this->_state = AVRISP_STATE_FORCED_SHUTDOWN;

  return;
}

uint8_t AVROTAComponent::flash_read(uint8_t hilo, int addr) {
  // ESP_LOGI(TAG, "[AVRISP] Flash Read");
  return spi_transaction(0x20 + hilo * 8, (addr >> 8) & 0xFF, addr & 0xFF, 0);
}

void AVROTAComponent::flash_read_page(int length) {
  // ESP_LOGI(TAG, "[AVRISP] Flash Read Page");
  uint8_t *data = (uint8_t *) malloc(length + 1);
  for (int x = 0; x < length; x += 2) {
    *(data + x) = flash_read(0, here);
    *(data + x + 1) = flash_read(1, here);
    here++;
    App.feed_wdt();
  }
  *(data + length) = Resp_STK_OK;
  
  // If the write fails, then set the state to idle
  if (!this->socket.write_bytes(data, length + 1)) this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
  
  free(data);
  return;
}

void AVROTAComponent::eeprom_read_page(int length) {
  // ESP_LOGI(TAG, "[AVRISP] EEPROM Read Page");
  // here again we have a word address
  uint8_t *data = (uint8_t *) malloc(length + 1);
  int start = here * 2;
  for (int x = 0; x < length; x++) {
    int addr = start + x;
    uint8_t ee = spi_transaction(0xA0, (addr >> 8) & 0xFF, addr & 0xFF, 0xFF);
    *(data + x) = ee;
    App.feed_wdt();
  }
  *(data + length) = Resp_STK_OK;

  // If the write fails, then set the state to idle
  if (!this->socket.write_bytes(data, length + 1)) this->_state = AVRISP_STATE_FORCED_SHUTDOWN;

  free(data);
  return;
}

void AVROTAComponent::read_page() {
  // ESP_LOGI(TAG, "[AVRISP] Read Page");
  char result = (char) Resp_STK_FAILED;
  int length = 256 * getch();
  length += getch();
  char memtype = getch();
  if (Sync_CRC_EOP != getch()) {
    error++;
    // If the write fails, then we are done
    if (!this->socket.write(Resp_STK_NOSYNC)) this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    return;
  }

  // If the write fails, then we are done
  if (!this->socket.write(Resp_STK_INSYNC)) {
    this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    return;
  }

  if (memtype == 'F')
    flash_read_page(length);
  if (memtype == 'E')
    eeprom_read_page(length);
  return;
}

void AVROTAComponent::read_signature() {
  // ESP_LOGI(TAG, "[AVRISP] Read Signature");
  if (Sync_CRC_EOP != getch()) {
    error++;
    if (!this->socket.write(Resp_STK_NOSYNC)) this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    return;
  }
  if (!this->socket.write(Resp_STK_INSYNC)) {
    this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    return;
  }

  uint8_t high = spi_transaction(0x30, 0x00, 0x00, 0x00);
  if (!this->socket.write(high)) {
    this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    return;
  }

  uint8_t middle = spi_transaction(0x30, 0x00, 0x01, 0x00);
  if (!this->socket.write(middle)) {
    this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    return;
  }

  uint8_t low = spi_transaction(0x30, 0x00, 0x02, 0x00);
  if (!this->socket.write(low)) {
    this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    return;
  }

  if (!this->socket.write(Resp_STK_OK)) {
    this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
    return; 
  }
}

// It seems ArduinoISP is based on the original STK500 (not v2)
// but implements only a subset of the commands.
void AVROTAComponent::avrisp() {
  uint8_t data, low, high;
  uint8_t ch = getch();
  // AVRISP_DEBUG("CMD 0x%02x", ch);
  switch (ch) {
    case Cmnd_STK_GET_SYNC:
      error = 0;
      empty_reply();
      break;

    case Cmnd_STK_GET_SIGN_ON:
      if (getch() == Sync_CRC_EOP) {
        if(!this->socket.write(Resp_STK_INSYNC)) {
          this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
          break;
        }

        if(!this->socket.print("AVR ISP")) {
          this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
          break;
        }

        if(!this->socket.write(Resp_STK_OK)) {
          this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
          break;
        }
      }
      break;

    case Cmnd_STK_GET_PARAMETER:
      get_parameter(getch());
      break;

    case Cmnd_STK_SET_DEVICE:
      fill(20);
      set_parameters();
      empty_reply();
      break;

    case Cmnd_STK_SET_DEVICE_EXT:  // ignored
      fill(5);
      empty_reply();
      break;

    case Cmnd_STK_ENTER_PROGMODE:
      start_pmode();
      empty_reply();
      break;

    case Cmnd_STK_LOAD_ADDRESS:
      here = getch();
      here += 256 * getch();
      // AVRISP_DEBUG("here=0x%04x", here);
      empty_reply();
      break;

    // XXX: not implemented!
    case Cmnd_STK_PROG_FLASH:
      low = getch();
      high = getch();
      empty_reply();
      break;

    // XXX: not implemented!
    case Cmnd_STK_PROG_DATA:
      data = getch();
      empty_reply();
      break;

    case Cmnd_STK_PROG_PAGE:
      program_page();
      break;

    case Cmnd_STK_READ_PAGE:
      read_page();
      break;

    case Cmnd_STK_UNIVERSAL:
      universal();
      break;

    case Cmnd_STK_LEAVE_PROGMODE:
      error = 0;
      end_pmode();
      empty_reply();
      delay(5);
      ESP_LOGI(TAG, "Command STK Leave Program Mode -> Socket Close");
      this->socket.close();
      // AVRISP_DEBUG("left progmode");

      break;

    case Cmnd_STK_READ_SIGN:
      read_signature();
      break;
      // expecting a command, not Sync_CRC_EOP
      // this is how we can get back in sync
    case Sync_CRC_EOP:  // 0x20, space
      error++;
      if(!this->socket.write(Resp_STK_NOSYNC)) {
        this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
      }
      break;

      // anything else we will return STK_UNKNOWN
    default:
      ESP_LOGE(TAG, "[AVRISP] Unknown avr isp state: %02x", ch);
      error++;
      if (Sync_CRC_EOP == getch()) {
        if(!this->socket.write(Resp_STK_UNKNOWN)) {
          this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
        }
      } else {
        if(!this->socket.write(Resp_STK_NOSYNC)) {
          this->_state = AVRISP_STATE_FORCED_SHUTDOWN;
        }
      }
  }
}

}  // namespace avr_ota
}  // namespace esphome
