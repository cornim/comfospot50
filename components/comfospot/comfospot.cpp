// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include "comfospot.h"
#include "comfospot_binary_sensor.h"
#include "comfospot_button.h"
#include "comfospot_fan.h"
#include "comfospot_select.h"
#include "comfospot_sensor.h"

#include <cstdlib>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::comfospot {

static const char *const TAG = "comfospot";
static constexpr uint32_t FILTER_RUNTIME_PREFERENCE = 0x43465350;
static constexpr uint32_t FILTER_RUNTIME_SAVE_INTERVAL_MS = 86400000;
static constexpr uint32_t FILTER_RUNTIME_PUBLISH_INTERVAL_MS = 60000;
static constexpr uint32_t SECONDS_PER_DAY = 86400;

bool ComfoSpot::pins_ready_() const {
  return this->led_1_pin_ != nullptr && this->led_2_pin_ != nullptr && this->led_3_pin_ != nullptr &&
         this->led_4_pin_ != nullptr && this->filter_pin_ != nullptr && this->error_pin_ != nullptr &&
         this->auto_pin_ != nullptr && this->plus_pin_ != nullptr && this->minus_pin_ != nullptr;
}

ComfoSpot::PanelSample ComfoSpot::read_panel_sample_() const {
  PanelSample sample;
  sample.led_mask = this->read_led_mask_();
  sample.filter = this->filter_pin_->digital_read();
  sample.error = this->error_pin_->digital_read();
  sample.auto_mode = this->auto_pin_->digital_read();
  sample.plus = this->plus_pin_->digital_read();
  sample.minus = this->minus_pin_->digital_read();
  return sample;
}

bool ComfoSpot::standby_wake_frame_(const PanelSample &sample) {
  // Standby wake lights every panel indicator, not just the four speed LEDs.
  return sample.led_mask == 0x0F && sample.filter && sample.error && sample.auto_mode;
}

uint8_t ComfoSpot::read_led_mask_() const {
  return static_cast<uint8_t>(static_cast<uint8_t>(this->led_1_pin_->digital_read()) << 0) |
         static_cast<uint8_t>(static_cast<uint8_t>(this->led_2_pin_->digital_read()) << 1) |
         static_cast<uint8_t>(static_cast<uint8_t>(this->led_3_pin_->digital_read()) << 2) |
         static_cast<uint8_t>(static_cast<uint8_t>(this->led_4_pin_->digital_read()) << 3);
}

int ComfoSpot::speed_for_led_mask_(uint8_t mask) {
  return protocol::speed_for_led_mask(mask);
}

int ComfoSpot::read_speed_() const { return speed_for_led_mask_(this->read_led_mask_()); }

bool ComfoSpot::all_leds_off_() const { return this->read_led_mask_() == 0; }

bool ComfoSpot::direction_marker_mask_(uint8_t mask) const {
  if (this->direction_ == Direction::INTAKE)
    return mask == 0x08;
  if (this->direction_ == Direction::EXHAUST)
    return mask == 0x01;
  return false;
}

bool ComfoSpot::direction_pattern_(Direction direction) const {
  if (!this->led_mask_initialized_)
    return false;
  const uint32_t now = millis();
  if (direction == Direction::INTAKE)
    return this->detected_direction_valid_ && this->detected_direction_ == Direction::INTAKE &&
           now - this->direction_detected_at_ <= 5000;
  if (direction == Direction::EXHAUST)
    return this->detected_direction_valid_ && this->detected_direction_ == Direction::EXHAUST &&
           now - this->direction_detected_at_ <= 5000;
  if (direction == Direction::EXCHANGE)
    return speed_for_led_mask_(this->observed_led_mask_) > 0 &&
           this->marker_blink_direction_ == Direction::UNKNOWN &&
           (!this->direction_marker_seen_ || now - this->last_direction_marker_edge_at_ >= DIRECTION_MARKER_SETTLE_MS);
  return false;
}

bool ComfoSpot::direction_confirmed_(Direction direction) const {
  if ((this->operation_ == Operation::MODE_HOLD || this->operation_ == Operation::MODE_CONFIRM) &&
      this->operation_marker_direction_ != Direction::UNKNOWN) {
    const uint32_t now = millis();
    return this->detected_direction_valid_ && this->detected_direction_ == this->operation_marker_direction_ &&
           this->direction_detected_at_ >= this->operation_action_started_at_ &&
           now - this->direction_detected_at_ <= MARKER_BLINK_WINDOW_MS;
  }
  if (direction == Direction::EXCHANGE) {
    const uint32_t now = millis();
    return this->detected_direction_valid_ && this->detected_direction_ == Direction::EXCHANGE &&
           now - this->direction_detected_at_ <= MARKER_BLINK_WINDOW_MS;
  }
  return this->direction_pattern_(direction);
}

bool ComfoSpot::external_panel_buttons_idle_() const {
  const bool plus = this->plus_pin_->digital_read();
  const bool minus = this->minus_pin_->digital_read();
  const bool plus_allowed = (this->active_outputs_ & OUTPUT_PLUS) != 0;
  const bool minus_allowed = (this->active_outputs_ & OUTPUT_MINUS) != 0;
  return (plus_allowed || !plus) && (minus_allowed || !minus);
}

bool ComfoSpot::physical_activity_pending_() const {
  const bool plus_owned = (this->active_outputs_ & OUTPUT_PLUS) != 0;
  const bool minus_owned = (this->active_outputs_ & OUTPUT_MINUS) != 0;
  return ((!plus_owned && (this->physical_plus_.stable || this->physical_plus_.raw)) ||
          (!minus_owned && (this->physical_minus_.stable || this->physical_minus_.raw)) ||
          this->physical_speed_sync_pending_ || this->physical_filter_sync_pending_ ||
          this->physical_direction_sync_pending_);
}

void ComfoSpot::setup() {
  if (!this->pins_ready_()) {
    ESP_LOGE(TAG, "Required panel GPIOs are not configured");
    this->status_set_error();
    this->mark_failed();
    return;
  }

  // Set the inactive latch before switching the open-drain pins to output mode.
  this->plus_pin_->digital_write(false);
  this->minus_pin_->digital_write(false);
  this->led_1_pin_->setup();
  this->led_2_pin_->setup();
  this->led_3_pin_->setup();
  this->led_4_pin_->setup();
  this->filter_pin_->setup();
  this->error_pin_->setup();
  this->auto_pin_->setup();
  this->plus_pin_->setup();
  this->minus_pin_->setup();
  this->release_outputs_();

  const uint32_t now = millis();
  const PanelSample sample = this->read_panel_sample_();
  this->observed_led_mask_ = sample.led_mask;
  this->led_mask_initialized_ = true;
  this->filter_latched_ = sample.filter;
  this->error_latched_ = sample.error;
  this->auto_latched_ = sample.auto_mode;
  this->raw_filter_state_ = sample.filter;
  this->raw_error_state_ = sample.error;
  this->auto_candidate_state_ = sample.auto_mode;
  this->auto_candidate_count_ = 3;
  this->filter_last_asserted_at_ = now;
  this->error_last_asserted_at_ = now;

  this->physical_plus_.raw = sample.plus;
  this->physical_plus_.stable = sample.plus;
  this->physical_plus_.raw_changed_at = now;
  this->physical_plus_.started_with_display_off = sample.led_mask == 0;
  this->physical_minus_.raw = sample.minus;
  this->physical_minus_.stable = sample.minus;
  this->physical_minus_.raw_changed_at = now;
  this->physical_minus_.started_with_display_off = sample.led_mask == 0;
  this->published_filter_state_ = this->filter_latched_;
  this->published_error_state_ = this->error_latched_;
  this->published_auto_state_ = this->auto_latched_;

  const int boot_speed = sample.auto_mode ? -1 : speed_for_led_mask_(sample.led_mask);
  if (boot_speed > 0) {
    this->set_speed_state_(boot_speed, true);
    this->panel_active_ = true;
    this->normal_speed_since_ = now;
  } else {
    // All LEDs off is ambiguous: it can mean standby or energy-saving display mode.
    this->current_speed_ = 0;
    this->speed_known_ = false;
    this->panel_known_ = true;
    this->panel_active_ = false;
    this->normal_speed_since_ = 0;
  }
  if (sample.auto_mode)
    this->panel_active_ = true;

  if (global_preferences == nullptr) {
    this->preference_warning_ = true;
    this->status_set_warning();
  } else {
    this->filter_runtime_preference_ = global_preferences->make_preference<uint32_t>(FILTER_RUNTIME_PREFERENCE);
  }
  if (global_preferences == nullptr || !this->filter_runtime_preference_.load(&this->filter_runtime_base_))
    this->filter_runtime_base_ = 0;
  this->filter_runtime_last_accounted_ = now;
  this->filter_runtime_remainder_ms_ = 0;
  this->filter_runtime_active_ = (this->speed_known_ && this->current_speed_ > 0) || sample.auto_mode;
  this->filter_runtime_dirty_ = false;
  this->last_runtime_update_ = now;
  this->last_runtime_save_ = now;
  this->last_entity_publish_ = now;
  this->initialized_ = true;

  this->sync_fan_();
  this->publish_speed_sensor_();
  this->publish_runtime_sensor_();
  this->publish_status_sensors_();
  this->publish_direction_();
  ESP_LOGI(TAG, "Panel state read at boot: speed %d%s", this->current_speed_,
           this->speed_known_ ? "" : " (display state ambiguous)");
}

void ComfoSpot::loop() {
  if (this->is_failed())
    return;
  this->process_filter_runtime_();
  this->process_physical_buttons_();
  this->process_operation_();

  const uint32_t now = millis();
  if (now - this->last_input_poll_ >= PANEL_POLL_INTERVAL_MS) {
    this->last_input_poll_ = now;
    this->poll_inputs_();
  }
}

void ComfoSpot::dump_config() {
  ESP_LOGCONFIG(TAG, "ComfoSpot 50:");
  ESP_LOGCONFIG(TAG, "  Fan levels: standby (0) + speeds 1-4");
  ESP_LOGCONFIG(TAG, "  Control: panel LED feedback and simulated + / - buttons");
}

void ComfoSpot::on_shutdown() {
  this->account_filter_runtime_(millis());
  this->save_filter_runtime_(true);
  this->release_outputs_();
}

void ComfoSpot::update_led_state_(uint32_t now, uint8_t mask) {
  // Expire a stale in-progress blink detection.
  if (this->marker_blink_direction_ != Direction::UNKNOWN &&
      now - this->marker_blink_window_start_ > MARKER_BLINK_WINDOW_MS) {
    this->marker_blink_direction_ = Direction::UNKNOWN;
    this->marker_blink_count_ = 0;
  }
  // Expire a confirmed-but-stale marker so EXCHANGE can be re-inferred once
  // the panel returns to a plain speed display.
  if (this->direction_marker_seen_ && now - this->last_direction_marker_at_ > MARKER_BLINK_WINDOW_MS)
    this->direction_marker_seen_ = false;

  if (!this->led_mask_initialized_) {
    this->observed_led_mask_ = mask;
    this->led_mask_initialized_ = true;
    return;
  }
  if (mask == this->observed_led_mask_) {
    if (speed_for_led_mask_(mask) > 0 && this->marker_blink_direction_ != Direction::UNKNOWN &&
        now - this->last_direction_marker_edge_at_ >= DIRECTION_MARKER_SETTLE_MS) {
      // A normal speed frame has remained visible after the marker edge.
      // Discard the candidate so it cannot block a later operation forever.
      this->marker_blink_direction_ = Direction::UNKNOWN;
      this->marker_blink_count_ = 0;
      this->normal_speed_since_ = now;
    } else if (speed_for_led_mask_(mask) > 0 && this->marker_blink_direction_ == Direction::UNKNOWN &&
               this->normal_speed_since_ == 0) {
      this->normal_speed_since_ = now;
    }
    return;
  }

  // Every read is trusted immediately: there is no multi-sample
  // confirmation here. Background polling is only used for entity sync,
  // status latches, and direction-marker detection; an incorrect or
  // transient observation here is not safety-relevant, because an actual
  // command (request_speed()/request_direction()/reset_filter()) always
  // independently re-reads and validates the panel itself before acting.
  const uint8_t previous_mask = this->observed_led_mask_;
  this->observed_led_mask_ = mask;
  const int current_speed = speed_for_led_mask_(mask);

  // Direction is signalled by a single fan LED alternating with the current
  // speed display (see the ComfoSpot manual): supply/intake mode (the '+'
  // button, Zuluftbetrieb) alternates LED4 (mask 0x08), while extract/exhaust
  // mode ('-', Abluftbetrieb) alternates LED1 (mask 0x01). The marker runs
  // roughly three cycles over about two seconds before the panel returns to
  // the normal speed display. Count transitions between the current speed
  // mask and the marker mask. Requiring at least MARKER_BLINK_MIN_EDGES such
  // edges within MARKER_BLINK_WINDOW_MS distinguishes a real marker from a
  // single clean transition to all-off (energy-saving display or standby).
  // The all-off form is retained for older panel revisions; at speed 1 the
  // exhaust marker is inherently ambiguous when both phases use LED1.
  Direction edge_marker = Direction::UNKNOWN;
  const int observed_speed = speed_for_led_mask_(previous_mask);
  if (observed_speed > 0 && mask == 0x08)
    edge_marker = Direction::INTAKE;
  else if (observed_speed > 0 && mask == 0x01)
    edge_marker = Direction::EXHAUST;
  else if (previous_mask == 0x08 && speed_for_led_mask_(mask) > 0)
    edge_marker = Direction::UNKNOWN;
  else if (previous_mask == 0x01 && speed_for_led_mask_(mask) > 0)
    edge_marker = Direction::UNKNOWN;
  else if (previous_mask == 0x00 && mask == 0x08)
    edge_marker = Direction::INTAKE;
  else if (previous_mask == 0x00 && mask == 0x01)
    edge_marker = Direction::EXHAUST;

  // The first speed-1 frame after a standby wake is a normal display result,
  // not the first edge of an exhaust marker. The marker detector cannot
  // distinguish those masks from the GPIO snapshot alone, so the standby
  // operation supplies this short-lived context.
  if ((this->standby_wake_pending_ || this->standby_wake_display_settled_) && previous_mask == 0x00 && mask == 0x01) {
    edge_marker = Direction::UNKNOWN;
    this->standby_wake_display_settled_ = false;
  }

  // Direction markers remain authoritative even when the panel's Error LED is
  // active. Exhaust -> Intake can legitimately coexist with a latched panel
  // warning, and suppressing the marker would leave the direction stale.
  if (edge_marker != Direction::UNKNOWN) {
    if (this->marker_blink_direction_ != edge_marker) {
      this->marker_blink_direction_ = edge_marker;
      this->marker_blink_count_ = 0;
      this->marker_blink_window_start_ = now;
    }
    this->marker_blink_count_++;
    this->last_direction_marker_edge_at_ = now;
    this->normal_speed_since_ = 0;
    if (this->marker_blink_count_ >= MARKER_BLINK_MIN_EDGES) {
      this->detected_direction_ = edge_marker;
      this->detected_direction_valid_ = true;
      this->direction_detected_at_ = now;
      this->direction_marker_seen_ = true;
      this->last_direction_marker_at_ = now;
      if (this->request_kind_ == RequestKind::DIRECTION &&
          edge_marker == this->operation_marker_direction_ &&
          this->direction_detected_at_ >= this->operation_action_started_at_)
        this->operation_target_seen_ = true;
      ESP_LOGD(TAG, "Detected %s direction marker", edge_marker == Direction::INTAKE ? "intake" : "exhaust");
    }
  } else if (this->marker_blink_direction_ != Direction::UNKNOWN &&
             (((previous_mask == 0x08 || previous_mask == 0x01) && (current_speed > 0 || mask == 0x00)) ||
              ((mask == 0x08 || mask == 0x01) && (observed_speed > 0 || previous_mask == 0x00)))) {
    // The other phase of an in-progress marker may be the normal speed
    // display. Record it as a possible new normal interval; the stable-frame
    // path above clears the candidate after the settle interval.
    this->normal_speed_since_ = current_speed > 0 ? now : 0;
  } else if (current_speed > 0) {
    // A normal, steady speed display (single or multiple LEDs, not part of a
    // fast blink). Any pending blink of a *different* signature is abandoned.
    if (this->marker_blink_direction_ != Direction::UNKNOWN &&
        now - this->marker_blink_window_start_ > MARKER_BLINK_WINDOW_MS) {
      this->marker_blink_direction_ = Direction::UNKNOWN;
      this->marker_blink_count_ = 0;
    }
    this->normal_speed_since_ = now;
  }
}

void ComfoSpot::update_status_latches_(uint32_t now, const PanelSample &sample) {
  const bool was_auto = this->auto_latched_;
  // The filter/error LEDs may be blinking. A single lit sample latches the
  // status immediately; it is only cleared once the LED has not been
  // observed lit for STATUS_CLEAR_MS, which is longer than the LED's
  // maximum dark phase, so a blinking LED keeps the latch asserted while a
  // genuinely dark LED releases it. filter_last_asserted_at_ is refreshed on
  // every lit sample (not just the dark->lit edge) so a steadily lit LED
  // keeps it fresh too.
  if (sample.filter) {
    this->filter_last_asserted_at_ = now;
    this->raw_filter_state_ = true;
    this->filter_latched_ = true;
  } else {
    this->raw_filter_state_ = false;
    if (now - this->filter_last_asserted_at_ >= STATUS_CLEAR_MS)
      this->filter_latched_ = false;
  }

  if (sample.error) {
    this->error_last_asserted_at_ = now;
    this->raw_error_state_ = true;
    this->error_latched_ = true;
  } else {
    this->raw_error_state_ = false;
    if (now - this->error_last_asserted_at_ >= STATUS_CLEAR_MS)
      this->error_latched_ = false;
  }
  if (sample.auto_mode == this->auto_candidate_state_) {
    if (this->auto_candidate_count_ < 3)
      this->auto_candidate_count_++;
  } else {
    this->auto_candidate_state_ = sample.auto_mode;
    this->auto_candidate_count_ = 1;
  }
  if (this->auto_candidate_count_ >= 3)
    this->auto_latched_ = this->auto_candidate_state_;
  if (this->auto_latched_ && !was_auto) {
    this->account_filter_runtime_(now);
    this->speed_known_ = false;
    this->panel_active_ = true;
    this->filter_runtime_active_ = true;
    this->sync_fan_();
    this->invalidate_speed_sensor_();
  } else if (!this->auto_latched_ && was_auto) {
    this->account_filter_runtime_(now);
    this->panel_active_ = false;
    this->filter_runtime_active_ = this->speed_known_ && this->current_speed_ > 0;
    this->sync_fan_();
  }
}

void ComfoSpot::set_speed_state_(int speed, bool known) {
  const uint32_t now = millis();
  this->account_filter_runtime_(now);
  if (known && speed < 0)
    return;
  this->current_speed_ = clamp(speed, 0, 4);
  this->speed_known_ = known;
  this->panel_known_ = true;
  this->panel_active_ = known && this->current_speed_ > 0;
  if (this->current_speed_ > 0)
    this->last_active_speed_ = this->current_speed_;
  this->filter_runtime_active_ = known && this->current_speed_ > 0;
  if (known && this->current_speed_ > 0)
    this->normal_speed_since_ = now;
  else
    this->normal_speed_since_ = 0;
}

void ComfoSpot::update_speed_from_panel_(int speed, bool all_off_is_standby) {
  if (speed > 0) {
    if (!this->speed_known_ || speed != this->current_speed_) {
      this->set_speed_state_(speed, true);
      this->sync_fan_();
      this->publish_speed_sensor_();
    }
  } else if (all_off_is_standby && this->speed_known_ && this->current_speed_ != 0) {
    this->set_speed_state_(0, true);
    this->sync_fan_();
    this->publish_speed_sensor_();
  }
}

void ComfoSpot::poll_inputs_() {
  const uint32_t now = millis();
  const PanelSample sample = this->read_panel_sample_();
  ESP_LOGV(TAG, "Panel scan: LEDs=0x%02X filter=%d error=%d auto=%d plus=%d minus=%d", sample.led_mask,
           sample.filter, sample.error, sample.auto_mode, sample.plus, sample.minus);
  this->update_led_state_(now, sample.led_mask);
  // A standby wake acknowledgement briefly asserts every panel indicator,
  // including the filter, error, and automatic-mode inputs. It is a display
  // frame rather than three real status conditions, so do not latch it.
  const bool transient_wake_frame = this->standby_wake_frame_(sample);
  if (!transient_wake_frame)
    this->update_status_latches_(now, sample);

  if (this->operation_ == Operation::IDLE && !this->physical_activity_pending_()) {
    if (!this->auto_latched_) {
      const int observed_speed = speed_for_led_mask_(this->observed_led_mask_);
      if (this->physical_standby_wake_pending_ &&
          (observed_speed != 4 || now - this->physical_standby_wake_started_at_ >= STANDBY_WAKE_TIMEOUT_MS)) {
        this->physical_standby_wake_pending_ = false;
        this->physical_standby_wake_started_at_ = 0;
      }
      if (!(this->physical_standby_wake_pending_ && observed_speed == 4))
        this->update_speed_from_panel_(observed_speed, false);
    }
    if (this->direction_ == Direction::UNKNOWN && this->detected_direction_valid_) {
      this->direction_ = this->detected_direction_;
      this->publish_direction_();
    }
    if (!this->direction_marker_seen_ && speed_for_led_mask_(this->observed_led_mask_) > 0 &&
        this->normal_speed_since_ != 0 && now - this->normal_speed_since_ >= 5000 &&
        this->direction_ != Direction::EXCHANGE) {
      // Once the marker window has expired, a continuously plain speed
      // display is authoritative evidence that the panel is back in exchange
      // mode, even if the cached direction was Intake or Exhaust.
      this->direction_ = Direction::EXCHANGE;
      this->publish_direction_();
    }
    // A confirmed LED marker is authoritative for physical mode changes. It
    // repairs a stale cached direction without depending on button duration.
    if (this->detected_direction_valid_ && this->direction_marker_seen_ &&
        this->direction_ != this->detected_direction_) {
      this->direction_ = this->detected_direction_;
      this->publish_direction_();
      ESP_LOGD(TAG, "LED marker synchronized physical direction");
    }
  }

  if (this->filter_latched_ != this->published_filter_state_ ||
      this->error_latched_ != this->published_error_state_ || this->auto_latched_ != this->published_auto_state_) {
    this->published_filter_state_ = this->filter_latched_;
    this->published_error_state_ = this->error_latched_;
    this->published_auto_state_ = this->auto_latched_;
    this->publish_status_sensors_();
    this->last_entity_publish_ = now;
  }

  // The panel is polled far more often than Home Assistant needs updating.
  // On-change publishes above are immediate; otherwise entities are only
  // refreshed periodically so the fast poll does not spam the API.
  if (now - this->last_entity_publish_ >= ENTITY_PUBLISH_INTERVAL_MS) {
    this->last_entity_publish_ = now;
    this->sync_fan_();
    this->publish_speed_sensor_();
    this->publish_status_sensors_();
    this->publish_direction_();
  }
}

bool ComfoSpot::update_physical_button_(PhysicalButtonState &button, bool raw, uint32_t now) {
  if (raw != button.raw) {
    button.raw = raw;
    button.raw_changed_at = now;
  }
  if (button.stable == button.raw || now - button.raw_changed_at < BUTTON_DEBOUNCE_MS)
    return false;
  button.stable = button.raw;
  if (button.stable) {
    button.pressed = true;
    button.press_started_at = now;
    button.started_with_display_off = this->all_leds_off_();
  }
  return true;
}

void ComfoSpot::process_physical_buttons_() {
  if (!this->initialized_)
    return;
  const uint32_t now = millis();
  const bool plus_changed = this->update_physical_button_(this->physical_plus_, this->plus_pin_->digital_read(), now);
  const bool minus_changed = this->update_physical_button_(this->physical_minus_, this->minus_pin_->digital_read(), now);

  if (plus_changed && this->physical_plus_.stable && (this->active_outputs_ & OUTPUT_PLUS) == 0)
    ESP_LOGD(TAG, "Physical key press detected: +");
  if (minus_changed && this->physical_minus_.stable && (this->active_outputs_ & OUTPUT_MINUS) == 0)
    ESP_LOGD(TAG, "Physical key press detected: -");

  if (this->physical_plus_.stable && this->physical_minus_.stable && !this->physical_chord_) {
    this->physical_chord_ = true;
    this->physical_chord_started_at_ = std::min(this->physical_plus_.press_started_at, this->physical_minus_.press_started_at);
  }

  const bool plus_controller_owned = (this->active_outputs_ & OUTPUT_PLUS) != 0;
  const bool minus_controller_owned = (this->active_outputs_ & OUTPUT_MINUS) != 0;
  if ((plus_changed && this->physical_plus_.stable && !plus_controller_owned) ||
      (minus_changed && this->physical_minus_.stable && !minus_controller_owned)) {
    this->detected_direction_valid_ = false;
    this->direction_marker_seen_ = false;
    this->marker_blink_direction_ = Direction::UNKNOWN;
    this->marker_blink_count_ = 0;
  }

  if (this->operation_ != Operation::IDLE) {
    this->controller_gesture_active_ = this->active_outputs_ != OUTPUT_NONE;
    return;
  }

  if (this->controller_gesture_active_) {
    if (!this->physical_plus_.stable && !this->physical_minus_.stable && this->active_outputs_ == OUTPUT_NONE)
      this->controller_gesture_active_ = false;
    return;
  }

  if (plus_changed && !this->physical_plus_.stable && this->physical_plus_.pressed) {
    this->physical_plus_.pressed = false;
    if (!this->physical_chord_) {
      const uint32_t duration = now - this->physical_plus_.press_started_at;
      if (duration <= SHORT_PRESS_MAX_MS) {
        this->physical_speed_sync_pending_ = true;
        this->physical_speed_sync_started_at_ = now;
        this->physical_short_button_ = OUTPUT_PLUS;
        this->physical_standby_wake_pending_ = this->physical_plus_.started_with_display_off;
        this->physical_standby_wake_started_at_ = this->physical_standby_wake_pending_ ? now : 0;
      }
    }
  }
  if (minus_changed && !this->physical_minus_.stable && this->physical_minus_.pressed) {
    this->physical_minus_.pressed = false;
    if (!this->physical_chord_) {
      const uint32_t duration = now - this->physical_minus_.press_started_at;
      if (duration <= SHORT_PRESS_MAX_MS) {
        this->physical_speed_sync_pending_ = true;
        this->physical_speed_sync_started_at_ = now;
        this->physical_short_button_ = OUTPUT_MINUS;
      }
    }
  }

  if (this->physical_chord_ && !this->physical_plus_.stable && !this->physical_minus_.stable) {
    const uint32_t duration = now - this->physical_chord_started_at_;
    if (duration >= FILTER_PRESS_MS) {
      this->physical_filter_sync_pending_ = true;
      this->physical_filter_sync_started_at_ = now;
      this->physical_filter_reset_was_active_ = this->filter_latched_;
      this->physical_error_reset_was_active_ = this->error_latched_;
    }
    this->physical_chord_ = false;
  }

  if (this->physical_speed_sync_pending_ && now - this->physical_speed_sync_started_at_ >= 250) {
    this->physical_speed_sync_pending_ = false;
    const int observed = this->read_speed_();
    if (this->physical_standby_wake_pending_ && observed == 4) {
      // Standby wakes with all four fan LEDs lit for roughly 1.5 s. That
      // frame is an acknowledgement of the wake-up tap, not speed 4.
      ESP_LOGD(TAG, "Standby wake acknowledged by physical button; waiting for the panel to blank");
    } else if (observed > 0) {
      this->physical_standby_wake_pending_ = false;
      this->update_speed_from_panel_(observed, false);
      ESP_LOGD(TAG, "Physical button press synchronized speed %d", observed);
    } else if (this->physical_short_button_ == OUTPUT_MINUS && this->speed_known_ && this->current_speed_ == 1 &&
               this->all_leds_off_() && !this->physical_minus_.started_with_display_off) {
      this->update_speed_from_panel_(0, true);
      ESP_LOGD(TAG, "Physical button press synchronized standby");
    }
    this->physical_short_button_ = OUTPUT_NONE;
  }

  if (this->physical_filter_sync_pending_ && now - this->physical_filter_sync_started_at_ >= STATUS_CLEAR_MS) {
    this->physical_filter_sync_pending_ = false;
    // The reset is only confirmed once the (possibly blinking) LED has not
    // been observed lit for a full STATUS_CLEAR_MS window, matching the
    // blink-robust latch clear rather than a single dark sample.
    const bool filter_clear = !this->physical_filter_reset_was_active_ ||
                              (!this->raw_filter_state_ &&
                               now - this->filter_last_asserted_at_ >= STATUS_CLEAR_MS);
    const bool error_clear = !this->physical_error_reset_was_active_ ||
                             (!this->raw_error_state_ &&
                              now - this->error_last_asserted_at_ >= STATUS_CLEAR_MS);
    if ((this->physical_filter_reset_was_active_ || this->physical_error_reset_was_active_) &&
        filter_clear && error_clear) {
      this->filter_latched_ = false;
      if (this->physical_filter_reset_was_active_) {
        this->reset_filter_runtime_(now);
        this->publish_runtime_sensor_();
      }
      if (this->physical_error_reset_was_active_)
        this->error_latched_ = false;
      this->publish_status_sensors_();
    }
  }

  if (this->physical_direction_sync_pending_ && now - this->physical_direction_sync_started_at_ >= 750) {
    this->physical_direction_sync_pending_ = false;
    this->synchronize_physical_direction_();
  }
}

void ComfoSpot::synchronize_physical_direction_() {
  if (this->physical_direction_pin_ == this->plus_pin_ && this->direction_pattern_(Direction::INTAKE)) {
    this->direction_ = Direction::INTAKE;
  } else if (this->physical_direction_pin_ == this->plus_pin_ && this->direction_ == Direction::INTAKE &&
             this->direction_pattern_(Direction::EXCHANGE)) {
    this->direction_ = Direction::EXCHANGE;
  } else if (this->physical_direction_pin_ == this->minus_pin_ && this->direction_pattern_(Direction::EXHAUST)) {
    this->direction_ = Direction::EXHAUST;
  } else if (this->physical_direction_pin_ == this->minus_pin_ && this->direction_ == Direction::EXHAUST &&
             this->direction_pattern_(Direction::EXCHANGE)) {
    this->direction_ = Direction::EXCHANGE;
  } else {
    this->physical_direction_pin_ = nullptr;
    return;
  }
  this->physical_direction_pin_ = nullptr;
  this->publish_direction_();
  ESP_LOGD(TAG, "Physical long button press synchronized direction");
}

void ComfoSpot::account_filter_runtime_(uint32_t now) {
  if (this->filter_runtime_active_) {
    const uint32_t elapsed = now - this->filter_runtime_last_accounted_;
    const uint64_t accumulated = static_cast<uint64_t>(this->filter_runtime_remainder_ms_) + elapsed;
    const uint32_t seconds = static_cast<uint32_t>(accumulated / 1000);
    this->filter_runtime_remainder_ms_ = static_cast<uint32_t>(accumulated % 1000);
    if (seconds > 0) {
      if (UINT32_MAX - this->filter_runtime_base_ < seconds)
        this->filter_runtime_base_ = UINT32_MAX;
      else
        this->filter_runtime_base_ += seconds;
      this->filter_runtime_dirty_ = true;
    }
  }
  this->filter_runtime_last_accounted_ = now;
}

uint32_t ComfoSpot::filter_runtime_seconds() const {
  uint64_t runtime = this->filter_runtime_base_;
  if (this->filter_runtime_active_) {
    const uint32_t elapsed = millis() - this->filter_runtime_last_accounted_;
    runtime += (static_cast<uint64_t>(this->filter_runtime_remainder_ms_) + elapsed) / 1000;
  }
  return runtime > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(runtime);
}

float ComfoSpot::filter_runtime_days() const {
  return static_cast<float>(this->filter_runtime_seconds()) / SECONDS_PER_DAY;
}

void ComfoSpot::process_filter_runtime_() {
  const uint32_t now = millis();
  this->account_filter_runtime_(now);
  if (now - this->last_runtime_update_ >= FILTER_RUNTIME_PUBLISH_INTERVAL_MS) {
    this->last_runtime_update_ = now;
    this->publish_runtime_sensor_();
  }
  if (now - this->last_runtime_save_ >= FILTER_RUNTIME_SAVE_INTERVAL_MS) {
    this->last_runtime_save_ = now;
    this->save_filter_runtime_();
  }
}

void ComfoSpot::save_filter_runtime_(bool force) {
  this->account_filter_runtime_(millis());
  if (!force && !this->filter_runtime_dirty_)
    return;
  if (global_preferences == nullptr) {
    this->preference_warning_ = true;
    this->status_set_warning();
    return;
  }
  if (!this->filter_runtime_preference_.save(&this->filter_runtime_base_)) {
    this->preference_warning_ = true;
    this->status_set_warning();
    return;
  }
  if (!global_preferences->sync()) {
    this->filter_runtime_dirty_ = true;
    this->preference_warning_ = true;
    this->status_set_warning();
    return;
  }
  this->filter_runtime_dirty_ = false;
}

void ComfoSpot::sync_fan_() {
  if (!this->panel_known_)
    return;
  for (auto *fan : this->fans_) {
    fan->speed = this->current_speed_ > 0 ? this->current_speed_ : this->last_active_speed_;
    fan->state = this->panel_active_;
    fan->publish_state();
  }
}

void ComfoSpot::publish_speed_sensor_() {
  if (!this->panel_known_ || !this->speed_known_)
    return;
  for (auto *sensor : this->speed_sensors_)
    sensor->publish_state(this->current_speed_);
}

void ComfoSpot::invalidate_speed_sensor_() {
  for (auto *sensor : this->speed_sensors_)
    sensor->publish_state(NAN);
}

void ComfoSpot::publish_runtime_sensor_() {
  for (auto *sensor : this->runtime_sensors_)
    sensor->publish_state(this->filter_runtime_days());
}

void ComfoSpot::publish_status_sensors_() {
  for (auto *sensor : this->filter_sensors_)
    sensor->publish_state(this->filter_change_needed());
  for (auto *sensor : this->error_sensors_)
    sensor->publish_state(this->error_active());
  for (auto *sensor : this->auto_sensors_)
    sensor->publish_state(this->auto_active());
}

void ComfoSpot::publish_direction_() {
  if (this->direction_ == Direction::UNKNOWN)
    return;
  for (auto *select : this->direction_selects_)
    select->publish_state(static_cast<size_t>(this->direction_));
}

void ComfoSpot::request_speed(int speed) {
  speed = clamp(speed, 0, 4);
  if (!this->initialized_ || this->physical_activity_pending_()) {
    ESP_LOGW(TAG, "Ignoring speed request while panel input is busy or unknown");
    return;
  }
  if (this->auto_latched_) {
    ESP_LOGW(TAG, "Ignoring speed request while automatic mode is active");
    return;
  }
  if (this->operation_ != Operation::IDLE) {
    this->pending_speed_ = speed;
    this->pending_speed_valid_ = true;
    ESP_LOGD(TAG, "Queued latest speed request: %d", speed);
    return;
  }
  if (speed == this->current_speed_ && this->speed_known_ && !this->all_leds_off_()) {
    this->sync_fan_();
    return;
  }
  this->start_speed_operation_(speed);
}

void ComfoSpot::start_speed_operation_(int speed) {
    this->start_request_(RequestKind::SPEED, speed, Direction::UNKNOWN);
}

void ComfoSpot::request_direction(Direction direction) {
  if (!this->initialized_ || this->physical_activity_pending_()) {
    ESP_LOGW(TAG, "Ignoring direction request while panel input is busy or unknown");
    return;
  }
  if (direction == Direction::UNKNOWN)
    return;
  if (this->operation_ != Operation::IDLE) {
    this->pending_direction_ = direction;
    this->pending_direction_valid_ = true;
    ESP_LOGD(TAG, "Queued latest direction request");
    return;
  }
  if (direction == this->direction_ && this->speed_known_ && this->current_speed_ > 0 && !this->all_leds_off_())
    return;
  if (direction == Direction::INTAKE && this->error_active()) {
    this->fail_operation_("ComfoSpot rejected intake mode");
    return;
  }
  this->start_direction_operation_(direction);
}

void ComfoSpot::start_direction_operation_(Direction direction) {
  this->start_request_(RequestKind::DIRECTION, 0, direction);
}

void ComfoSpot::start_request_(RequestKind kind, int speed, Direction direction) {
  this->request_kind_ = kind;
  this->operation_speed_ = speed;
  this->operation_start_speed_ = -1;
  this->target_speed_ = speed;
  this->requested_direction_ = direction;
  this->operation_ = Operation::WAIT_PANEL_OFF;
  this->operation_started_at_ = millis();
  this->operation_timeout_ms_ = OPERATION_TIMEOUT_MS;
  this->operation_deadline_ = this->operation_started_at_ + OPERATION_TIMEOUT_MS;
  this->panel_off_since_ = 0;
  this->next_action_at_ = 0;
  this->operation_action_started_at_ = 0;
  this->standby_wake_seen_ = false;
  this->wake_standby_possible_ = !this->speed_known_ || this->current_speed_ == 0;
  this->operation_observed_speed_ = -1;
  this->operation_target_seen_ = false;
  this->detected_direction_valid_ = false;
  this->direction_marker_seen_ = false;
  this->marker_blink_direction_ = Direction::UNKNOWN;
  this->marker_blink_count_ = 0;
  this->last_direction_marker_edge_at_ = 0;
  this->normal_speed_since_ = 0;
  this->operation_marker_direction_ = Direction::UNKNOWN;
  this->operation_command_pin_ = nullptr;
  this->controller_gesture_active_ = true;
  this->operation_start_speed_ = this->speed_known_ ? this->current_speed_ : -1;
  ESP_LOGD(TAG, "Queued %s request", kind == RequestKind::SPEED ? "speed" : "direction");
}

bool ComfoSpot::panel_off_barrier_reached_(uint32_t now) {
  if (!this->all_leds_off_()) {
    this->panel_off_since_ = 0;
    return false;
  }
  if (this->panel_off_since_ == 0)
    this->panel_off_since_ = now;
  return now - this->panel_off_since_ >= PANEL_OFF_BARRIER_MS;
}

void ComfoSpot::begin_after_panel_off_() {
  // Every state change starts with the same wake/read transaction. This is
  // deliberate even when the cached state is known: the display may have
  // entered energy-saving mode or the physical panel may have changed since
  // the last scan.
  this->begin_active_request_();
  ESP_LOGD(TAG, "Waking panel before state change");
}

void ComfoSpot::begin_active_request_() {
  const uint32_t now = millis();
  this->operation_action_started_at_ = now;
  this->operation_deadline_ = now + CONTROL_SHORT_PRESS_MS;
  this->press_(this->plus_pin_);
  this->operation_ = Operation::SPEED_WAKE_PRESS;
  ESP_LOGD(TAG, "Activating panel before state change");
}

void ComfoSpot::prepare_speed_steps_(uint32_t now) {
  if (!this->speed_known_) {
    this->fail_operation_("Panel state is still unknown after wake-up");
    return;
  }
  if (this->request_kind_ == RequestKind::DIRECTION && this->current_speed_ == 0) {
    this->fail_operation_("Direction changes require a non-zero fan speed");
    return;
  }
  if (this->request_kind_ == RequestKind::SPEED) {
    const int starting_speed = this->operation_start_speed_ >= 0 ? this->operation_start_speed_ : this->current_speed_;
    this->steps_remaining_ = abs(this->operation_speed_ - starting_speed);
    this->step_sign_ = this->operation_speed_ > starting_speed ? 1 : -1;
    if (this->steps_remaining_ == 0) {
      this->set_speed_state_(this->operation_speed_, true);
      this->start_post_operation_(true);
      return;
    }
    this->current_speed_ = starting_speed;
  this->operation_ = Operation::SPEED_STEP_WAIT;
  this->next_action_at_ = now;
    return;
  }

  if (this->request_kind_ == RequestKind::DIRECTION && this->direction_ == Direction::UNKNOWN) {
    if (this->requested_direction_ == Direction::EXCHANGE) {
      this->fail_operation_("Cannot select exchange while panel direction is unknown");
      return;
    }
  }

  this->operation_command_pin_ = this->direction_command_pin_();
  this->operation_marker_direction_ = this->requested_direction_ == Direction::EXCHANGE
                                          ? this->direction_
                                          : this->requested_direction_;
  if (this->operation_command_pin_ == nullptr || this->operation_marker_direction_ == Direction::UNKNOWN) {
    this->fail_operation_("Cannot determine the direction transition from the current state");
    return;
  }
  this->detected_direction_valid_ = false;
  this->direction_marker_seen_ = false;
  this->marker_blink_direction_ = Direction::UNKNOWN;
  this->marker_blink_count_ = 0;
  this->last_direction_marker_edge_at_ = 0;
  this->normal_speed_since_ = 0;
  if (this->requested_direction_ == this->direction_) {
    this->start_post_operation_(true);
    return;
  }
  this->operation_action_started_at_ = now;
  this->operation_ = Operation::MODE_HOLD_WAIT;
  this->next_action_at_ = now + CONTROL_KEY_INTERVAL_MS;
  ESP_LOGD(TAG, "Changing direction to %s", this->requested_direction_ == Direction::INTAKE ? "Intake"
                                                         : this->requested_direction_ == Direction::EXHAUST ? "Exhaust" : "Exchange");
}

void ComfoSpot::start_post_operation_(bool success, const char *message) {
  this->post_operation_success_ = success;
  this->post_operation_error_ = message;
  this->release_outputs_();
  this->operation_ = Operation::POST_OPERATION_OFF;
  this->operation_started_at_ = millis();
  this->operation_timeout_ms_ = OPERATION_TIMEOUT_MS;
  this->panel_off_since_ = 0;
  this->operation_deadline_ = this->operation_started_at_ + OPERATION_TIMEOUT_MS;
}

void ComfoSpot::start_next_pending_request_() {
  // Keep the fan running while a direction change is pending. This avoids
  // making a direction request fail its non-zero-speed precondition when both
  // request types were queued during one operation.
  if (this->pending_direction_valid_ && this->current_speed_ > 0) {
    const Direction direction = this->pending_direction_;
    this->pending_direction_valid_ = false;
    this->start_request_(RequestKind::DIRECTION, 0, direction);
    return;
  }

  if (this->pending_speed_valid_) {
    const int speed = this->pending_speed_;
    this->pending_speed_valid_ = false;
    this->start_request_(RequestKind::SPEED, speed, Direction::UNKNOWN);
    return;
  }

  if (this->pending_direction_valid_) {
    const Direction direction = this->pending_direction_;
    this->pending_direction_valid_ = false;
    this->start_request_(RequestKind::DIRECTION, 0, direction);
  }
}

GPIOPin *ComfoSpot::direction_command_pin_() const {
  if (this->requested_direction_ == Direction::INTAKE)
    return this->plus_pin_;
  if (this->requested_direction_ == Direction::EXHAUST)
    return this->minus_pin_;
  if (this->direction_ == Direction::INTAKE)
    return this->plus_pin_;
  if (this->direction_ == Direction::EXHAUST)
    return this->minus_pin_;
  return nullptr;
}

void ComfoSpot::reset_filter() {
  if (!this->initialized_ || this->operation_ != Operation::IDLE || this->physical_activity_pending_()) {
    ESP_LOGW(TAG, "Ignoring filter reset while panel input is busy or unknown");
    return;
  }
  this->start_filter_reset_();
}

void ComfoSpot::start_filter_reset_() {
  this->reset_filter_was_active_ = this->filter_latched_;
  this->reset_error_was_active_ = this->error_latched_;
  this->operation_ = Operation::FILTER_WAIT_OFF;
  this->operation_started_at_ = millis();
  this->operation_timeout_ms_ = 30000;
  this->controller_gesture_active_ = true;
  this->operation_deadline_ = millis() + 15000;
  ESP_LOGD(TAG, "Starting filter reset");
}

void ComfoSpot::set_outputs_(uint8_t output_mask) {
  if (this->plus_pin_ != nullptr)
    this->plus_pin_->digital_write((output_mask & OUTPUT_PLUS) != 0);
  if (this->minus_pin_ != nullptr)
    this->minus_pin_->digital_write((output_mask & OUTPUT_MINUS) != 0);
  this->active_outputs_ = output_mask;
}

void ComfoSpot::press_(GPIOPin *pin) {
  if (pin == this->plus_pin_)
    this->set_outputs_(OUTPUT_PLUS);
  else if (pin == this->minus_pin_)
    this->set_outputs_(OUTPUT_MINUS);
  else
    this->set_outputs_(OUTPUT_NONE);
}

void ComfoSpot::release_outputs_() { this->set_outputs_(OUTPUT_NONE); }

bool ComfoSpot::panel_inputs_idle_() const { return this->external_panel_buttons_idle_(); }

bool ComfoSpot::filter_reset_confirmed_(uint32_t now) const {
  // See the physical-chord reset above: a reset is confirmed only once the
  // (possibly blinking) LED has been dark for a full STATUS_CLEAR_MS window,
  // never on a single lucky dark sample of a still-blinking LED.
  const bool filter_clear = !this->reset_filter_was_active_ ||
                            (!this->raw_filter_state_ &&
                             now - this->filter_last_asserted_at_ >= STATUS_CLEAR_MS);
  const bool error_clear = !this->reset_error_was_active_ ||
                           (!this->raw_error_state_ &&
                            now - this->error_last_asserted_at_ >= STATUS_CLEAR_MS);
  return filter_clear && error_clear;
}

void ComfoSpot::process_operation_() {
  if (this->operation_ == Operation::IDLE)
    return;
  const uint32_t now = millis();
  if (this->operation_timeout_ms_ > 0 &&
      now - this->operation_started_at_ >= this->operation_timeout_ms_) {
    this->fail_operation_("Panel operation exceeded its safety timeout");
    return;
  }

  // Never fight a user's non-owned button. The output mask allows the line owned
  // by this operation to be active while still detecting the other line.
  if (this->active_outputs_ != (OUTPUT_PLUS | OUTPUT_MINUS) && !this->panel_inputs_idle_()) {
    this->fail_operation_("Panel button pressed during controller operation");
    return;
  }

  switch (this->operation_) {
    case Operation::WAIT_PANEL_OFF:
      if (this->panel_off_barrier_reached_(now)) {
        if (this->panel_inputs_idle_())
          this->begin_after_panel_off_();
      }
      break;

    case Operation::SPEED_WAKE_PRESS:
      if (this->deadline_reached_(now, this->operation_deadline_)) {
        this->release_outputs_();
        this->operation_ = Operation::SPEED_WAKE_WAIT;
        this->operation_deadline_ = now + PANEL_ON_DISPLAY_MS;
        this->operation_action_started_at_ = now;
      }
      break;

    case Operation::SPEED_WAKE_WAIT: {
      const PanelSample sample = this->read_panel_sample_();
      const uint8_t mask = sample.led_mask;
      const int observed = speed_for_led_mask_(mask);
      if (this->standby_wake_frame_(sample)) {
        // Only the all-indicator frame is a standby acknowledgement. A plain
        // 0x0F frame is the normal speed-4 display and must remain the
        // operation baseline for a downward speed request.
        this->standby_wake_seen_ = true;
        break;
      }
      if (this->standby_wake_seen_ && mask == 0x00) {
        this->standby_wake_seen_ = false;
        if (this->operation_start_speed_ <= 0)
          this->operation_start_speed_ = 0;
        this->wake_standby_possible_ = false;
        this->speed_known_ = true;
        this->prepare_speed_steps_(now);
        break;
      }
      if (mask == 0x00 && this->operation_speed_ == 0 && this->operation_start_speed_ > 0) {
        this->prepare_speed_steps_(now);
        break;
      }
      if (observed > 0 && now - this->operation_action_started_at_ >= WAKE_SETTLE_MS) {
        this->operation_start_speed_ = observed;
        this->operation_observed_speed_ = observed;
        this->wake_standby_possible_ = false;
        this->speed_known_ = true;
        this->prepare_speed_steps_(now);
      } else if (this->deadline_reached_(now, this->operation_deadline_)) {
        this->fail_operation_("Panel did not show its state after wake-up");
      }
      break;
    }

    case Operation::SPEED_STEP_WAIT:
      if (this->request_kind_ == RequestKind::SPEED && this->steps_remaining_ == 0) {
        this->operation_ = Operation::SPEED_CONFIRM;
        this->operation_deadline_ = now + STATE_CONFIRM_MS;
      } else if (this->deadline_reached_(now, this->next_action_at_)) {
        const int expected_speed = this->current_speed_ + this->step_sign_;
        this->press_(this->step_sign_ > 0 ? this->plus_pin_ : this->minus_pin_);
        // Publish the expected intermediate state immediately after the
        // state-changing press. The wake press is handled separately and does
        // not reach this path, so it cannot create a false speed transition.
        this->set_speed_state_(expected_speed, true);
        this->sync_fan_();
        this->publish_speed_sensor_();
        this->operation_ = Operation::SPEED_STEP_PRESS;
        this->operation_action_started_at_ = now;
        this->operation_deadline_ = now + CONTROL_SHORT_PRESS_MS;
        --this->steps_remaining_;
      }
      break;

    case Operation::SPEED_STEP_PRESS:
      if (this->deadline_reached_(now, this->operation_deadline_)) {
        this->release_outputs_();
        if (this->steps_remaining_ == 0) {
          this->operation_ = Operation::SPEED_CONFIRM;
          this->operation_deadline_ = now + STATE_CONFIRM_MS;
        } else {
          this->operation_ = Operation::SPEED_STEP_WAIT;
          this->next_action_at_ = this->operation_action_started_at_ + CONTROL_KEY_INTERVAL_MS;
        }
      }
      break;

    case Operation::SPEED_CONFIRM: {
      const uint8_t mask = this->read_led_mask_();
      const int observed = speed_for_led_mask_(mask);
      if (observed > 0)
        this->operation_observed_speed_ = observed;
      const bool matched = this->operation_speed_ == 0 ? mask == 0x00 : observed == this->operation_speed_;
      if (matched)
        this->operation_target_seen_ = true;
      if (this->deadline_reached_(now, this->operation_deadline_)) {
        if (!this->operation_target_seen_) {
          this->fail_operation_("Panel did not confirm requested fan speed");
          break;
        }
        this->start_post_operation_(true);
      }
      break;
    }

    case Operation::MODE_HOLD_WAIT:
      if (this->deadline_reached_(now, this->next_action_at_)) {
        this->press_(this->operation_command_pin_);
        this->operation_ = Operation::MODE_HOLD;
        this->operation_action_started_at_ = now;
        this->operation_deadline_ = now + CONTROL_LONG_PRESS_MS;
      }
      break;

    case Operation::MODE_HOLD:
      if (this->deadline_reached_(now, this->operation_deadline_)) {
        this->release_outputs_();
        this->operation_ = Operation::MODE_CONFIRM;
        this->operation_deadline_ = now + MODE_CONFIRM_MS;
      }
      break;

    case Operation::MODE_CONFIRM:
      if (this->operation_target_seen_ && this->deadline_reached_(now, this->operation_deadline_)) {
        this->direction_ = this->requested_direction_;
        this->start_post_operation_(true);
      } else if (this->deadline_reached_(now, this->operation_deadline_) && this->requested_direction_ == Direction::INTAKE &&
                 this->error_active()) {
        this->fail_operation_("ComfoSpot rejected intake mode");
      } else if (this->deadline_reached_(now, this->operation_deadline_)) {
        this->fail_operation_("Panel did not confirm requested direction");
      }
      break;

    case Operation::POST_OPERATION_OFF:
      if (this->panel_off_barrier_reached_(now)) {
        if (this->post_operation_success_)
          this->finish_operation_(true);
        else
          this->fail_operation_(this->post_operation_error_ != nullptr ? this->post_operation_error_ :
                                                                         "Panel operation failed");
      }
      break;

    case Operation::FILTER_WAIT_OFF:
      if (this->deadline_reached_(now, this->operation_deadline_)) {
        this->fail_operation_("Timed out waiting for panel buttons to be released");
      } else if (this->panel_inputs_idle_()) {
        // See the identical comment in DIRECTION_WAIT_OFF: a raw read is
        // intentional here too, for the same reason.
        if (this->all_leds_off_()) {
          this->press_(this->plus_pin_);
          this->operation_ = Operation::FILTER_WAKE_PRESS;
          this->operation_deadline_ = now + 100;
        } else {
          this->set_outputs_(OUTPUT_PLUS | OUTPUT_MINUS);
          this->operation_ = Operation::FILTER_HOLD;
          this->operation_deadline_ = now + FILTER_PRESS_MS;
        }
      }
      break;

    case Operation::FILTER_WAKE_PRESS:
      if (this->deadline_reached_(now, this->operation_deadline_)) {
        this->release_outputs_();
        this->operation_ = Operation::FILTER_WAKE_RELEASE;
        this->operation_deadline_ = now + 500;
      }
      break;

    case Operation::FILTER_WAKE_RELEASE:
      if (this->deadline_reached_(now, this->operation_deadline_)) {
        this->set_outputs_(OUTPUT_PLUS | OUTPUT_MINUS);
        this->operation_ = Operation::FILTER_HOLD;
        this->operation_deadline_ = now + FILTER_PRESS_MS;
      }
      break;

    case Operation::FILTER_HOLD:
      if (this->deadline_reached_(now, this->operation_deadline_)) {
        this->release_outputs_();
        this->operation_ = Operation::FILTER_CONFIRM;
        this->operation_deadline_ = now + STATUS_CLEAR_MS;
      }
      break;

    case Operation::FILTER_CONFIRM:
      if (this->deadline_reached_(now, this->operation_deadline_)) {
        if (this->filter_reset_confirmed_(now) &&
            (!this->reset_error_was_active_ || !this->error_latched_)) {
          this->filter_latched_ = false;
          if (this->reset_error_was_active_)
            this->error_latched_ = false;
          this->reset_filter_runtime_(now);
          this->finish_operation_(true);
        } else {
          this->fail_operation_("Panel did not clear the filter warning");
        }
      }
      break;

    case Operation::IDLE:
      break;
  }
}

void ComfoSpot::reset_filter_runtime_(uint32_t now) {
  this->account_filter_runtime_(now);
  this->filter_runtime_base_ = 0;
  this->filter_runtime_remainder_ms_ = 0;
  this->filter_runtime_last_accounted_ = now;
  this->filter_runtime_dirty_ = true;
  this->save_filter_runtime_(true);
  this->last_runtime_save_ = now;
}

void ComfoSpot::finish_operation_(bool success) {
  const bool speed_request = this->request_kind_ == RequestKind::SPEED;
  if (speed_request && this->operation_start_speed_ >= 0) {
    // The intermediate values were optimistic. Reconcile and publish the
    // final state only after the operation's validation and panel-off barrier.
    const int final_speed = success
                                ? this->operation_speed_
                                : this->operation_observed_speed_ > 0 ? this->operation_observed_speed_
                                                                       : this->operation_start_speed_;
    this->set_speed_state_(final_speed, true);
  }
  if (success)
    this->synchronize_direction_after_command_();
  if (success && speed_request)
    ESP_LOGD(TAG, "Speed operation confirmed: %d", this->operation_speed_);
  if (success && this->request_kind_ == RequestKind::DIRECTION) {
    const char *direction_name = this->requested_direction_ == Direction::INTAKE
                                     ? "Intake"
                                     : this->requested_direction_ == Direction::EXHAUST ? "Exhaust" : "Exchange";
    ESP_LOGD(TAG, "Direction operation confirmed: %s", direction_name);
  }
  this->release_outputs_();
  this->operation_ = Operation::IDLE;
  this->operation_deadline_ = 0;
  this->operation_started_at_ = 0;
  this->operation_timeout_ms_ = 0;
  this->operation_command_pin_ = nullptr;
  this->request_kind_ = RequestKind::NONE;
  this->operation_speed_ = 0;
  this->operation_start_speed_ = -1;
  this->operation_observed_speed_ = -1;
  this->operation_marker_direction_ = Direction::UNKNOWN;
  this->speed_wake_pending_ = false;
  this->standby_wake_pending_ = false;
  this->standby_wake_display_settled_ = false;
  this->physical_standby_wake_pending_ = false;
  this->physical_standby_wake_started_at_ = 0;
  this->physical_speed_sync_pending_ = false;
  this->physical_filter_sync_pending_ = false;
  this->physical_filter_reset_was_active_ = false;
  this->physical_error_reset_was_active_ = false;
  this->physical_direction_sync_pending_ = false;
  this->physical_direction_pin_ = nullptr;
  this->controller_gesture_active_ = false;
  this->physical_chord_ = false;
  const uint32_t now = millis();
  this->physical_plus_.raw = this->plus_pin_ != nullptr && this->plus_pin_->digital_read();
  this->physical_plus_.stable = this->physical_plus_.raw;
  this->physical_plus_.pressed = false;
  this->physical_plus_.raw_changed_at = now;
  this->physical_minus_.raw = this->minus_pin_ != nullptr && this->minus_pin_->digital_read();
  this->physical_minus_.stable = this->physical_minus_.raw;
  this->physical_minus_.pressed = false;
  this->physical_minus_.raw_changed_at = now;
  this->reset_filter_was_active_ = false;
  this->reset_error_was_active_ = false;
  if (success) {
    if (!this->preference_warning_)
      this->status_clear_warning();
    this->sync_fan_();
    this->publish_speed_sensor_();
    this->publish_runtime_sensor_();
    this->publish_status_sensors_();
    this->publish_direction_();
  } else if (speed_request) {
    this->sync_fan_();
    this->publish_speed_sensor_();
  }
  this->start_next_pending_request_();
}

void ComfoSpot::synchronize_direction_after_command_() {
  if (this->request_kind_ != RequestKind::SPEED || this->operation_speed_ <= 0)
    return;

  // A speed operation receives the same direction marker as any other panel
  // update. Keep the confirmed marker result even if its short-lived display
  // window expires before the speed confirmation and panel-off barrier end.
  // This is especially important for a speed request queued behind a mode
  // change: the new speed display still alternates with the selected mode's
  // marker and is not evidence of Exchange by itself.
  if (this->detected_direction_valid_) {
    this->direction_ = this->detected_direction_;
    return;
  }

  // A normal, non-zero speed display with no command-local marker is the
  // Exchange indication. Do this before clearing the operation-local marker
  // detector so a stale cached direction cannot survive a successful speed
  // command.
  if (!this->direction_marker_seen_ && this->marker_blink_direction_ == Direction::UNKNOWN &&
      this->normal_speed_since_ != 0)
    this->direction_ = Direction::EXCHANGE;
}

void ComfoSpot::fail_operation_(const char *message) {
  ESP_LOGE(TAG, "%s", message);
  this->status_set_warning();
  if (this->operation_ == Operation::POST_OPERATION_OFF) {
    this->finish_operation_(false);
  } else {
    this->start_post_operation_(false, message);
  }
}

}  // namespace esphome::comfospot
