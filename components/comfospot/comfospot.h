#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cstdint>
#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/preferences.h"

#include "comfospot_protocol.h"

namespace esphome::comfospot {

class ComfoSpot final : public Component {
 public:
  enum class Direction : uint8_t {
    EXCHANGE = 0,
    INTAKE = 1,
    EXHAUST = 2,
    UNKNOWN = 255,
  };

  void set_led_1_pin(GPIOPin *pin) { this->led_1_pin_ = pin; }
  void set_led_2_pin(GPIOPin *pin) { this->led_2_pin_ = pin; }
  void set_led_3_pin(GPIOPin *pin) { this->led_3_pin_ = pin; }
  void set_led_4_pin(GPIOPin *pin) { this->led_4_pin_ = pin; }
  void set_filter_pin(GPIOPin *pin) { this->filter_pin_ = pin; }
  void set_error_pin(GPIOPin *pin) { this->error_pin_ = pin; }
  void set_auto_pin(GPIOPin *pin) { this->auto_pin_ = pin; }
  void set_plus_pin(GPIOPin *pin) { this->plus_pin_ = pin; }
  void set_minus_pin(GPIOPin *pin) { this->minus_pin_ = pin; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_fan(fan::Fan *fan) {
    if (std::find(this->fans_.begin(), this->fans_.end(), fan) != this->fans_.end()) {
      ESP_LOGW("comfospot", "Fan entity registered more than once");
      return;
    }
    this->fans_.push_back(fan);
    if (this->initialized_)
      this->sync_fan_();
  }
  void set_speed_sensor(sensor::Sensor *sensor) {
    if (std::find(this->speed_sensors_.begin(), this->speed_sensors_.end(), sensor) != this->speed_sensors_.end()) {
      ESP_LOGW("comfospot", "Speed sensor registered more than once");
      return;
    }
    this->speed_sensors_.push_back(sensor);
    if (this->initialized_)
      this->publish_speed_sensor_();
  }
  void set_runtime_sensor(sensor::Sensor *sensor) {
    if (std::find(this->runtime_sensors_.begin(), this->runtime_sensors_.end(), sensor) != this->runtime_sensors_.end()) {
      ESP_LOGW("comfospot", "Runtime sensor registered more than once");
      return;
    }
    this->runtime_sensors_.push_back(sensor);
    if (this->initialized_)
      this->publish_runtime_sensor_();
  }
  void set_filter_sensor(binary_sensor::BinarySensor *sensor) {
    if (std::find(this->filter_sensors_.begin(), this->filter_sensors_.end(), sensor) != this->filter_sensors_.end()) {
      ESP_LOGW("comfospot", "Filter sensor registered more than once");
      return;
    }
    this->filter_sensors_.push_back(sensor);
    if (this->initialized_)
      this->publish_status_sensors_();
  }
  void set_error_sensor(binary_sensor::BinarySensor *sensor) {
    if (std::find(this->error_sensors_.begin(), this->error_sensors_.end(), sensor) != this->error_sensors_.end()) {
      ESP_LOGW("comfospot", "Error sensor registered more than once");
      return;
    }
    this->error_sensors_.push_back(sensor);
    if (this->initialized_)
      this->publish_status_sensors_();
  }
  void set_auto_sensor(binary_sensor::BinarySensor *sensor) {
    if (std::find(this->auto_sensors_.begin(), this->auto_sensors_.end(), sensor) != this->auto_sensors_.end()) {
      ESP_LOGW("comfospot", "Automatic-mode sensor registered more than once");
      return;
    }
    this->auto_sensors_.push_back(sensor);
    if (this->initialized_)
      this->publish_status_sensors_();
  }
  void set_direction_select(select::Select *select) {
    if (std::find(this->direction_selects_.begin(), this->direction_selects_.end(), select) != this->direction_selects_.end()) {
      ESP_LOGW("comfospot", "Direction select registered more than once");
      return;
    }
    this->direction_selects_.push_back(select);
    if (this->initialized_)
      this->publish_direction_();
  }

  void request_speed(int speed);
  void request_direction(Direction direction);
  void reset_filter();

  int current_speed() const { return this->current_speed_; }
  bool speed_known() const { return this->speed_known_; }
  int preferred_speed() const { return this->last_active_speed_; }
  Direction current_direction() const { return this->direction_; }
  bool filter_change_needed() const { return this->filter_latched_; }
  bool error_active() const { return this->error_latched_; }
  bool auto_active() const { return this->auto_latched_; }
  uint32_t filter_runtime_seconds() const;
  float filter_runtime_days() const;

 protected:
  struct PanelSample {
    uint8_t led_mask{0};
    bool filter{false};
    bool error{false};
    bool auto_mode{false};
    bool plus{false};
    bool minus{false};
  };

  struct PhysicalButtonState {
    bool raw{false};
    bool stable{false};
    bool pressed{false};
    uint32_t raw_changed_at{0};
    uint32_t press_started_at{0};
    bool started_with_display_off{false};
  };

  enum class Operation : uint8_t {
    IDLE,
    WAIT_PANEL_OFF,
    SPEED_WAKE_PRESS,
    SPEED_WAKE_WAIT,
    SPEED_STEP_PRESS,
    SPEED_STEP_WAIT,
    SPEED_CONFIRM,
    MODE_HOLD_WAIT,
    MODE_HOLD,
    MODE_CONFIRM,
    POST_OPERATION_OFF,
    FILTER_WAIT_OFF,
    FILTER_WAKE_PRESS,
    FILTER_WAKE_RELEASE,
    FILTER_HOLD,
    FILTER_CONFIRM,
  };

  static constexpr uint8_t OUTPUT_NONE = 0;
  static constexpr uint8_t OUTPUT_PLUS = 1;
  static constexpr uint8_t OUTPUT_MINUS = 2;
  static constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;
  static constexpr uint32_t CONTROL_SHORT_PRESS_MS = 150;
  static constexpr uint32_t CONTROL_KEY_INTERVAL_MS = 500;
  static constexpr uint32_t CONTROL_LONG_PRESS_MS = 8000;
  static constexpr uint32_t SHORT_PRESS_MAX_MS = 500;
  static constexpr uint32_t DIRECTION_PRESS_MS = CONTROL_LONG_PRESS_MS;
  static constexpr uint32_t PANEL_OFF_BARRIER_MS = 1000;
  static constexpr uint32_t PANEL_ON_DISPLAY_MS = 9000;
  static constexpr uint32_t STATE_CONFIRM_MS = 9000;
  static constexpr uint32_t MODE_CONFIRM_MS = 6000;
  static constexpr uint32_t WAKE_SETTLE_MS = 500;
  static constexpr uint32_t DIRECTION_MARKER_SETTLE_MS = 1000;
  static constexpr uint32_t FILTER_PRESS_MS = 3000;
  // Direction marker: LED4 (intake) or LED1 (exhaust) alternates with the
  // current speed display for roughly three cycles over about two seconds.
  // We confirm a marker once at least MARKER_BLINK_MIN_EDGES marker-display
  // transitions are observed within MARKER_BLINK_WINDOW_MS. Requiring more
  // than one edge rejects a single clean transition to all-off
  // (energy-saving display sleep / standby), which is not a marker. The
  // window comfortably spans the marker burst while being short enough that
  // stale edges do not accumulate.
  static constexpr uint8_t MARKER_BLINK_MIN_EDGES = 3;
  static constexpr uint32_t MARKER_BLINK_WINDOW_MS = 10000;
  // The filter, error and direction-marker LEDs signal some states by
  // *blinking* rather than by a steady level (see the ComfoSpot manual).
  // The blink frequency is not documented, but its off-phase is assumed to
  // never exceed MAX_LED_OFF_PHASE_MS. A latched status is therefore only
  // considered cleared once the LED has not been observed lit for longer
  // than that off-phase plus a safety margin: a genuinely dark LED stays
  // off across the whole window, whereas a blinking one is caught lit
  // again within one off-phase. This only holds if the panel is sampled
  // well above the LED's blink frequency (see PANEL_POLL_INTERVAL_MS), so
  // that no on-phase is ever aliased away.
  static constexpr uint32_t MAX_LED_OFF_PHASE_MS = 5000;
  static constexpr uint32_t STATUS_CLEAR_MS = MAX_LED_OFF_PHASE_MS + 1000;
  // Entity state is pushed to Home Assistant immediately on a confirmed
  // change and otherwise refreshed at most this often, so the fast panel
  // poll below does not spam the API.
  static constexpr uint32_t ENTITY_PUBLISH_INTERVAL_MS = 5000;
  // The panel is sampled quickly (well above the LEDs' unknown blink
  // frequency) so a blinking filter/error/direction LED is never aliased
  // away; see MAX_LED_OFF_PHASE_MS. Correctness of remote commands does
  // not depend on this poll: every command re-validates against a fresh
  // read of its own on its sub-second operation timers. Entity updates to
  // Home Assistant are rate-limited separately (ENTITY_PUBLISH_INTERVAL_MS)
  // so this fast poll does not translate into fast API traffic.
  static constexpr uint32_t PANEL_POLL_INTERVAL_MS = 50;
  // Standby lights all four fan LEDs for about 1.2 s after the wake tap.
  // This is an acknowledgement frame, not the normal speed-4 display.
  static constexpr uint32_t STANDBY_WAKE_TIMEOUT_MS = 1200;
  static constexpr uint32_t OPERATION_TIMEOUT_MS = 90000;
  static bool deadline_reached_(uint32_t now, uint32_t deadline) { return protocol::deadline_reached(now, deadline); }

  bool pins_ready_() const;
  PanelSample read_panel_sample_() const;
  static bool standby_wake_frame_(const PanelSample &sample);
  uint8_t read_led_mask_() const;
  static int speed_for_led_mask_(uint8_t mask);
  int read_speed_() const;
  bool all_leds_off_() const;
  bool direction_marker_mask_(uint8_t mask) const;
  bool direction_pattern_(Direction direction) const;
  bool direction_confirmed_(Direction direction) const;
  bool external_panel_buttons_idle_() const;
  bool physical_activity_pending_() const;
  void update_led_state_(uint32_t now, uint8_t mask);
  void update_status_latches_(uint32_t now, const PanelSample &sample);
  void update_speed_from_panel_(int speed, bool all_off_is_standby);
  void account_filter_runtime_(uint32_t now);
  void set_speed_state_(int speed, bool known);
  bool filter_reset_confirmed_(uint32_t now) const;
  void poll_inputs_();
  void process_operation_();
  void process_physical_buttons_();
  void synchronize_physical_direction_();
  bool update_physical_button_(PhysicalButtonState &button, bool raw, uint32_t now);
  void process_filter_runtime_();
  void sync_fan_();
  void publish_speed_sensor_();
  void invalidate_speed_sensor_();
  void publish_runtime_sensor_();
  void publish_status_sensors_();
  void publish_direction_();
  void finish_operation_(bool success);
  void fail_operation_(const char *message);
  void synchronize_direction_after_command_();
  enum class RequestKind : uint8_t { NONE, SPEED, DIRECTION };
  void start_request_(RequestKind kind, int speed, Direction direction);
  void start_next_pending_request_();
  void begin_after_panel_off_();
  void begin_active_request_();
  void prepare_speed_steps_(uint32_t now);
  void start_post_operation_(bool success, const char *message = nullptr);
  bool panel_off_barrier_reached_(uint32_t now);
  void start_speed_operation_(int speed);
  void start_direction_operation_(Direction direction);
  void start_filter_reset_();
  void reset_filter_runtime_(uint32_t now);
  GPIOPin *direction_command_pin_() const;
  void press_(GPIOPin *pin);
  void set_outputs_(uint8_t output_mask);
  void release_outputs_();
  bool panel_inputs_idle_() const;
  void save_filter_runtime_(bool force = false);

  GPIOPin *led_1_pin_{nullptr};
  GPIOPin *led_2_pin_{nullptr};
  GPIOPin *led_3_pin_{nullptr};
  GPIOPin *led_4_pin_{nullptr};
  GPIOPin *filter_pin_{nullptr};
  GPIOPin *error_pin_{nullptr};
  GPIOPin *auto_pin_{nullptr};
  GPIOPin *plus_pin_{nullptr};
  GPIOPin *minus_pin_{nullptr};

  std::vector<fan::Fan *> fans_;
  std::vector<sensor::Sensor *> speed_sensors_;
  std::vector<sensor::Sensor *> runtime_sensors_;
  std::vector<binary_sensor::BinarySensor *> filter_sensors_;
  std::vector<binary_sensor::BinarySensor *> error_sensors_;
  std::vector<binary_sensor::BinarySensor *> auto_sensors_;
  std::vector<select::Select *> direction_selects_;
  Operation operation_{Operation::IDLE};
  RequestKind request_kind_{RequestKind::NONE};
  Direction direction_{Direction::UNKNOWN};
  Direction requested_direction_{Direction::EXCHANGE};
  Direction operation_marker_direction_{Direction::UNKNOWN};
  GPIOPin *operation_command_pin_{nullptr};
  uint8_t active_outputs_{OUTPUT_NONE};
  int current_speed_{0};
  int target_speed_{0};
  int steps_remaining_{0};
  int step_sign_{0};
  int expected_step_speed_{-1};
  uint8_t retries_{0};
  int operation_speed_{0};
  int operation_start_speed_{-1};
  bool pending_speed_valid_{false};
  int pending_speed_{0};
  bool pending_direction_valid_{false};
  Direction pending_direction_{Direction::UNKNOWN};
  bool panel_known_{false};
  bool speed_known_{false};
  bool panel_active_{false};
  bool initialized_{false};
  bool preference_warning_{false};
  bool speed_wake_pending_{false};
  bool standby_wake_pending_{false};
  bool standby_wake_display_settled_{false};
  bool direction_wake_ack_seen_{false};
  bool physical_standby_wake_pending_{false};
  uint32_t physical_standby_wake_started_at_{0};
  int last_active_speed_{1};
  uint32_t operation_deadline_{0};
  uint32_t operation_started_at_{0};
  uint32_t operation_timeout_ms_{0};
  uint32_t panel_off_since_{0};
  uint32_t next_action_at_{0};
  uint32_t operation_action_started_at_{0};
  bool standby_wake_seen_{false};
  bool wake_standby_possible_{false};
  int operation_observed_speed_{-1};
  bool operation_target_seen_{false};
  bool post_operation_success_{false};
  const char *post_operation_error_{nullptr};
  uint32_t last_input_poll_{0};
  uint32_t last_runtime_update_{0};
  uint32_t last_runtime_save_{0};
  uint32_t filter_runtime_base_{0};
  uint32_t filter_runtime_last_accounted_{0};
  uint32_t filter_runtime_remainder_ms_{0};
  bool filter_runtime_active_{false};
  bool filter_runtime_dirty_{false};
  bool reset_filter_was_active_{false};
  bool reset_error_was_active_{false};
  ESPPreferenceObject filter_runtime_preference_{};

  PhysicalButtonState physical_plus_;
  PhysicalButtonState physical_minus_;
  bool controller_gesture_active_{false};
  bool physical_chord_{false};
  uint32_t physical_chord_started_at_{0};
  bool physical_speed_sync_pending_{false};
  uint32_t physical_speed_sync_started_at_{0};
  uint8_t physical_short_button_{OUTPUT_NONE};
  bool physical_filter_sync_pending_{false};
  bool physical_filter_reset_was_active_{false};
  bool physical_error_reset_was_active_{false};
  uint32_t physical_filter_sync_started_at_{0};
  GPIOPin *physical_direction_pin_{nullptr};
  bool physical_direction_sync_pending_{false};
  uint32_t physical_direction_sync_started_at_{0};

  // The most recently read LED mask. Every read is used immediately (no
  // multi-sample confirmation): a single poll is trusted as the panel's
  // current display, since a genuine change of interest (an explicitly
  // requested speed step) is always independently validated and retried by
  // the operation state machine, not inferred from this value.
  uint8_t observed_led_mask_{0};
  bool led_mask_initialized_{false};
  // Fast single-LED marker detector for the direction marker. See
  // update_led_state_() and MARKER_BLINK_* below. marker_blink_direction_ is
  // the marker currently accumulating display<->marker edges
  // (LED4=intake, LED1=exhaust), and marker_blink_count_ is the number of
  // marker edges seen since marker_blink_window_start_.
  Direction marker_blink_direction_{Direction::UNKNOWN};
  uint8_t marker_blink_count_{0};
  uint32_t marker_blink_window_start_{0};
  uint32_t last_direction_marker_edge_at_{0};
  bool direction_marker_seen_{false};
  uint32_t last_direction_marker_at_{0};
  uint32_t direction_detected_at_{0};
  Direction detected_direction_{Direction::UNKNOWN};
  bool detected_direction_valid_{false};
  bool filter_latched_{false};
  bool error_latched_{false};
  bool auto_latched_{false};
  bool raw_filter_state_{false};
  bool raw_error_state_{false};
  bool auto_candidate_state_{false};
  uint8_t auto_candidate_count_{0};
  bool published_filter_state_{false};
  bool published_error_state_{false};
  bool published_auto_state_{false};
  // Last time each LED was observed lit; used to hold a latch across the
  // dark phases of a blinking LED and only clear it once the LED has been
  // continuously dark for STATUS_CLEAR_MS. See MAX_LED_OFF_PHASE_MS.
  uint32_t filter_last_asserted_at_{0};
  uint32_t error_last_asserted_at_{0};
  uint32_t normal_speed_since_{0};
  // Rate-limits periodic entity refreshes to Home Assistant so the fast
  // panel poll does not spam the API; on-change publishes are immediate.
  uint32_t last_entity_publish_{0};
};

}  // namespace esphome::comfospot
