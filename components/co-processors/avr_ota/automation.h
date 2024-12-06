#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include "avr_ota.h"

namespace esphome {
namespace avr_ota {

template<typename... Ts> class ResetAction : public Action<Ts...> {
 public:
  explicit ResetAction(AVROTAComponent *a_avr_ota) : avr_ota_(a_avr_ota) {}

  void play(Ts... x) override { this->avr_ota_->reset(); }

 protected:
  AVROTAComponent *avr_ota_;
};

template<typename... Ts> class EnableAction : public Action<Ts...> {
 public:
  explicit EnableAction(AVROTAComponent *a_avr_ota) : avr_ota_(a_avr_ota) {}

  void play(Ts... x) override { this->avr_ota_->enable_avr(); }

 protected:
  AVROTAComponent *avr_ota_;
};

template<typename... Ts> class DisableAction : public Action<Ts...> {
 public:
  explicit DisableAction(AVROTAComponent *a_avr_ota) : avr_ota_(a_avr_ota) {}

  void play(Ts... x) override { this->avr_ota_->disable_avr(); }

 protected:
  AVROTAComponent *avr_ota_;
};

template<typename... Ts> class ToggleAction : public Action<Ts...> {
 public:
  explicit ToggleAction(AVROTAComponent *a_avr_ota) : avr_ota_(a_avr_ota) {}

  void play(Ts... x) override { this->avr_ota_->toggle(); }

 protected:
  AVROTAComponent *avr_ota_;
};

template<typename... Ts> class AVRCondition : public Condition<Ts...> {
 public:
  AVRCondition(AVROTAComponent *parent, bool state) : parent_(parent), state_(state) {}
  bool check(Ts... x) override { return this->parent_->is_enabled() == this->state_; }

 protected:
  AVROTAComponent *parent_;
  bool state_;
};

template<typename... Ts> class AVRStateIdle : public Condition<Ts...> {
 public:
  AVRStateIdle(AVROTAComponent *parent, bool state) : parent_(parent), state_(state) {}
  bool check(Ts... x) override { return this->parent_->get_avr_state() == AVRISP_STATE_IDLE; }

 protected:
  AVROTAComponent *parent_;
  bool state_;
};

template<typename... Ts> class AVRStatePending : public Condition<Ts...> {
 public:
  AVRStatePending(AVROTAComponent *parent, bool state) : parent_(parent), state_(state) {}
  bool check(Ts... x) override { return this->parent_->get_avr_state() == AVRISP_STATE_PENDING; }

 protected:
  AVROTAComponent *parent_;
  bool state_;
};

template<typename... Ts> class AVRStateActive : public Condition<Ts...> {
 public:
  AVRStateActive(AVROTAComponent *parent, bool state) : parent_(parent), state_(state) {}
  bool check(Ts... x) override { return this->parent_->get_avr_state() == AVRISP_STATE_ACTIVE; }

 protected:
  AVROTAComponent *parent_;
  bool state_;
};

template<typename... Ts> class AVRStateForcedShutdown : public Condition<Ts...> {
 public:
  AVRStateForcedShutdown(AVROTAComponent *parent, bool state) : parent_(parent), state_(state) {}
  bool check(Ts... x) override { return this->parent_->get_avr_state() == AVRISP_STATE_FORCED_SHUTDOWN; }

 protected:
  AVROTAComponent *parent_;
  bool state_;
};

class EnableTrigger : public Trigger<> {
 public:
  EnableTrigger(AVROTAComponent *a_avr_ota) {
    // ESP_LOGI("Added Enable Trigger");
    a_avr_ota->add_on_enable_callback([this]() {
        this->trigger();
    });
  }
};

class DisableTrigger : public Trigger<> {
 public:
  DisableTrigger(AVROTAComponent *a_avr_ota) {
    // ESP_LOGI("Added Disable Trigger");
    a_avr_ota->add_on_disable_callback([this]() {
        this->trigger();
    });
  }
};

class AVRIdleTrigger : public Trigger<> {
 public:
  AVRIdleTrigger(AVROTAComponent *a_avr_ota) {
    // ESP_LOGI("Added Disable Trigger");
    a_avr_ota->add_on_avr_callback([this](AVRISPState_t state) {
        if (state == AVRISP_STATE_IDLE) this->trigger();
    });
  }
};

class AVRPendingTrigger : public Trigger<> {
 public:
  AVRPendingTrigger(AVROTAComponent *a_avr_ota) {
    // ESP_LOGI("Added Disable Trigger");
    a_avr_ota->add_on_avr_callback([this](AVRISPState_t state) {
        if (state == AVRISP_STATE_PENDING) this->trigger();
    });
  }
};

class AVRActiveTrigger : public Trigger<> {
 public:
  AVRActiveTrigger(AVROTAComponent *a_avr_ota) {
    // ESP_LOGI("Added Disable Trigger");
    a_avr_ota->add_on_avr_callback([this](AVRISPState_t state) {
        if (state == AVRISP_STATE_ACTIVE) this->trigger();
    });
  }
};

class AVRForcedShutdownTrigger : public Trigger<> {
 public:
  AVRForcedShutdownTrigger(AVROTAComponent *a_avr_ota) {
    // ESP_LOGI("Added Disable Trigger");
    a_avr_ota->add_on_avr_callback([this](AVRISPState_t state) {
        if (state == AVRISP_STATE_FORCED_SHUTDOWN) this->trigger();
    });
  }
};

}  // namespace avr_ota
}  // namespace esphome
