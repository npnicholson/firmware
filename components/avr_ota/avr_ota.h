#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/output/binary_output.h"

#include "WebSocket.h"

namespace esphome
{
namespace avr_ota
{

// programmer states
typedef enum {
    AVRISP_STATE_IDLE = 0,       // no active TCP session
    AVRISP_STATE_PENDING,        // TCP connected, pending SPI activation
    AVRISP_STATE_ACTIVE,         // programmer is active and owns the SPI bus
    AVRISP_STATE_FORCED_SHUTDOWN // programmer will shut down due to a socket failure or shutdown
} AVRISPState_t;

typedef enum {
  AVR_RESTORE_DEFAULT_OFF,
  AVR_RESTORE_DEFAULT_ON,
  AVR_ALWAYS_OFF,
  AVR_ALWAYS_ON,
  AVR_RESTORE_INVERTED_DEFAULT_OFF,
  AVR_RESTORE_INVERTED_DEFAULT_ON,
  AVR_RESTORE_AND_OFF,
  AVR_RESTORE_AND_ON,
} AVRRestoreMode_t;

// stk500 parameters
typedef struct {
    uint8_t devicecode;
    uint8_t revision;
    uint8_t progtype;
    uint8_t parmode;
    uint8_t polling;
    uint8_t selftimed;
    uint8_t lockbytes;
    uint8_t fusebytes;
    int flashpoll;
    int eeprompoll;
    int pagesize;
    int eepromsize;
    int flashsize;
} AVRISP_parameter_t;

// Struct for the data stored in persistent storage
typedef struct {
  bool enabled{false};
} AVRStateRTCState;

class AVROTAComponent : public EntityBase, public Component,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_200KHZ> {
                  // public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,spi::CLOCK_POLARITY_LOW, 
                  //           spi::CLOCK_PHASE_LEADING,spi::DATA_RATE_1KHZ>
                  //            {
  public:
    // Set Callbacks for triggers
    void add_on_enable_callback(std::function<void(void)> &&callback);
    void add_on_disable_callback(std::function<void(void)> &&callback);
    void add_on_avr_callback(std::function<void(AVRISPState_t)> &&callback);
    

    // Connect the AVR Enable output. For use by the code builder
    void set_avr_enable(output::BinaryOutput *avr_enable) { avr_enable_ = avr_enable; }

    // Set the restore mode of this avr
    void set_restore_mode(AVRRestoreMode_t restore_mode) { restore_mode_ = restore_mode; }

    // Getter and setter for the web socket port
    void set_ws_port(uint16_t port);
    uint16_t get_ws_port() const;

    // Enable the AVR programmer by creating the web socket server
    bool enable_avr();

    // Disables the AVR programmer by removing the web socket server
    void disable_avr();

    // Activate or Deactivate the AVR Programmer
    void toggle();

    // Returns true if the AVR Programmer is enabled (the socket is running)
    bool is_enabled();

    // Stops any in progress AVR actions and resets the coprocessor
    void reset();

    // Component Functions
    void setup() override;
    void dump_config() override;
    float get_setup_priority() const override;
    void loop() override;

    // check for pending clients if IDLE, check for disconnect otherwise
    // returns the updated state
    AVRISPState_t isp_update();

    AVRISPState_t get_avr_state();
    
  protected:
    CallbackManager<void(void)> enable_callback_{};
    CallbackManager<void(void)> disable_callback_{};
    CallbackManager<void(AVRISPState_t)> avr_callback_{};
    
    AVRRestoreMode_t restore_mode_;

    // Object used to store the persisted values of the AVR
    ESPPreferenceObject rtc_;
    void store_state();

    // Binary Output used for the AVR Enable
    output::BinaryOutput *avr_enable_;
    void set_enable_(bool);

    // SPI Vars
    bool spi_transaction_active;

    // Websocket vars
    WebSocket socket;
    uint16_t port_;

    //// AVR ISP Defineds ////
    inline void _reject_incoming(void);     // reject any incoming tcp connections

    void avrisp(void);           // handle incoming STK500 commands

    uint8_t getch(void);        // retrieve a character from the remote end
    uint8_t spi_transaction(uint8_t, uint8_t, uint8_t, uint8_t);
    void empty_reply(void);
    void breply(uint8_t);

    void get_parameter(uint8_t);
    void set_parameters(void);
    int addr_page(int);
    void flash(uint8_t, int, uint8_t);
    void write_flash(int);
    uint8_t write_flash_pages(int length);
    uint8_t write_eeprom(int length);
    uint8_t write_eeprom_chunk(int start, int length);
    void commit(int addr);
    void program_page();
    uint8_t flash_read(uint8_t hilo, int addr);
    void flash_read_page(int length);
    void eeprom_read_page(int length);
    void read_page();
    void read_signature();

    void universal(void);

    void fill(int);             // fill the buffer with n bytes
    void start_pmode(void);     // enter program mode
    void end_pmode(void);       // exit program mode

    AVRISPState_t _state;
    AVRISPState_t _last_state;

    // programmer settings, set by remote end
    AVRISP_parameter_t param;
    // page buffer
    uint8_t buff[256];

    int error = 0;
    bool pmode = 0;

    // address for reading and writing, set by 'U' command
    int here;


};

} // namespace avr_ota
} // namespace esphome
