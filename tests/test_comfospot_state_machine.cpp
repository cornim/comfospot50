// Host-side tests for the timed ComfoSpot panel controller. The tests use the
// fake ESPHome surface in tests/stubs/ and drive the real component with a
// controllable clock and scripted panel LED masks.
// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <cstdio>
#include <vector>

#include "esphome/core/preferences.h"

#include "test_helpers.h"

using comfospot_test::Fixture;
using esphome::comfospot::ComfoSpot;

namespace {

int g_failures = 0;

#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      g_failures++; \
    } \
  } while (0)

struct PreferenceFixture {
  esphome::ESPPreferences preferences;

  PreferenceFixture() { esphome::global_preferences = &this->preferences; }
  ~PreferenceFixture() { esphome::global_preferences = nullptr; }
};

void advance_to_panel_off(Fixture &f) {
  f.set_led_mask(0x00);
  f.advance(1050, 50);  // one second dark-panel barrier, plus scheduler margin
}

void wake_active_panel(Fixture &f, uint8_t speed_mask) {
  f.advance(150, 50);  // controller wake press
  f.set_led_mask(speed_mask);
  f.advance(550, 50);  // allow the wake read and settle interval
}

void confirm_operation_after_display_off(Fixture &f, uint32_t extra_ms = 1100) {
  f.set_led_mask(0x00);
  f.advance(extra_ms, 50);  // post-operation one-second dark-panel barrier
}

void blink_marker(Fixture &f, uint8_t marker_mask, uint8_t speed_mask) {
  for (int i = 0; i < 3; i++) {
    f.set_led_mask(marker_mask);
    f.advance(100, 50);
    f.set_led_mask(0x00);
    f.advance(400, 50);
  }
  f.set_led_mask(speed_mask);
}

void test_boot_state_sync() {
  Fixture f;
  f.set_led_mask(0x03);
  f.comfospot.setup();

  esphome::fan::Fan fan;
  esphome::sensor::Sensor sensor;
  f.comfospot.set_fan(&fan);
  f.comfospot.set_speed_sensor(&sensor);

  CHECK(f.comfospot.speed_known());
  CHECK(f.comfospot.current_speed() == 2);
  CHECK(fan.state);
  CHECK(fan.speed == 2);
  CHECK(sensor.has_state);
  CHECK(sensor.state == 2.0f);
}

void test_boot_blank_is_unknown() {
  Fixture f;
  f.set_led_mask(0x00);
  f.comfospot.setup();

  esphome::fan::Fan fan;
  esphome::sensor::Sensor sensor;
  f.comfospot.set_fan(&fan);
  f.comfospot.set_speed_sensor(&sensor);

  CHECK(!f.comfospot.speed_known());
  CHECK(!fan.state);
  CHECK(!sensor.has_state);
}

// A normal speed command waits for one second of dark display, wakes the
// panel, then emits exactly the calculated number of 150 ms presses. The next
// press starts 500 ms after the previous press started, and the requested
// each state-changing press is published immediately and the final state is
// reconciled after the nine-second confirmation interval and panel-off barrier.
void test_speed_sequence_uses_fixed_timing_and_final_confirmation() {
  Fixture f;
  f.set_led_mask(0x01);  // speed 1
  f.comfospot.setup();

  f.comfospot.request_speed(3);
  advance_to_panel_off(f);
  CHECK(f.plus.driven);  // wake press
  f.advance(150, 50);
  CHECK(!f.plus.driven);

  f.set_led_mask(0x01);
  f.advance(550, 50);  // wake read, settle, and first state-changing press
  CHECK(f.plus.driven);
  f.advance(150, 50);
  CHECK(!f.plus.driven);
  CHECK(f.comfospot.current_speed() == 2);

  // The second press starts 500 ms after the first press. The optimistic state
  // advances immediately, while the final display is validated later.
  f.set_led_mask(0x07);
  f.advance(350, 50);
  f.advance(150, 50);
  CHECK(f.comfospot.current_speed() == 3);

  f.advance(7000, 50);
  CHECK(f.comfospot.current_speed() == 3);
  f.advance(3000, 50);
  CHECK(f.comfospot.current_speed() == 3);

  confirm_operation_after_display_off(f);
  CHECK(!f.plus.driven);
}

void test_failed_speed_operation_rolls_back_optimistic_state() {
  Fixture f;
  f.set_led_mask(0x03);  // speed 2
  f.comfospot.setup();

  esphome::fan::Fan fan;
  esphome::sensor::Sensor sensor;
  f.comfospot.set_fan(&fan);
  f.comfospot.set_speed_sensor(&sensor);

  f.comfospot.request_speed(3);
  advance_to_panel_off(f);
  f.advance(550, 50);  // wake press
  f.advance(150, 50);
  f.set_led_mask(0x03);
  f.advance(550, 50);  // one state-changing press starts
  CHECK(f.comfospot.current_speed() == 3);
  CHECK(sensor.state == 3.0f);

  // The panel never changes to the requested target. Let the final
  // confirmation and post-failure panel-off barrier complete.
  f.advance(9000, 50);
  f.set_led_mask(0x00);
  f.advance(1100, 50);

  CHECK(f.comfospot.current_speed() == 2);
  CHECK(sensor.state == 2.0f);
  CHECK(fan.state);
  CHECK(fan.speed == 2);
}

// The first short press from standby produces the temporary all-on frame. It
// must not be interpreted as speed 4; the actual speed press is sent only
// after the frame has blanked again.
void test_standby_wake_is_not_speed_four() {
  Fixture f;
  f.set_led_mask(0x01);
  f.comfospot.setup();

  f.comfospot.request_speed(0);
  advance_to_panel_off(f);
  f.advance(10500, 50);  // speed-0 confirmation plus dark-panel barrier
  CHECK(f.comfospot.current_speed() == 0);
  confirm_operation_after_display_off(f);

  f.comfospot.request_speed(1);
  advance_to_panel_off(f);
  f.advance(150, 50);
  CHECK(!f.plus.driven);

  f.filter.level = true;
  f.error.level = true;
  f.auto_mode.level = true;
  f.set_led_mask(0x0F);
  f.advance(600, 50);
  CHECK(f.comfospot.current_speed() == 0);
  const int writes_after_wake = f.plus.write_count;
  f.advance(500, 50);
  CHECK(f.plus.write_count == writes_after_wake);

  f.filter.level = false;
  f.error.level = false;
  f.auto_mode.level = false;
  f.set_led_mask(0x00);
  f.advance(1200, 50);
  CHECK(f.plus.write_count > writes_after_wake);

  f.set_led_mask(0x01);
  f.advance(10500, 50);
  CHECK(f.comfospot.current_speed() == 1);
  confirm_operation_after_display_off(f);
}

void test_speed_four_to_one_after_panel_blank_uses_minus_and_recovers_actual_speed() {
  Fixture f;
  f.set_led_mask(0x0F);  // active speed 4, not a standby acknowledgement
  f.comfospot.setup();

  f.comfospot.request_speed(1);
  advance_to_panel_off(f);
  f.advance(150, 50);  // controller wake press
  f.set_led_mask(0x0F);  // plain speed-4 display after wake
  f.plus.ever_driven_true = false;
  f.minus.ever_driven_true = false;
  f.advance(550, 50);

  CHECK(f.minus.driven);
  CHECK(!f.plus.ever_driven_true);
  CHECK(f.comfospot.current_speed() == 3);

  f.advance(150, 50);  // release the first decrement press
  f.advance(11000, 50);  // requested speed 1 never appears
  f.set_led_mask(0x00);
  f.advance(1100, 50);  // post-failure panel-off barrier

  CHECK(f.comfospot.current_speed() == 4);
  CHECK(f.comfospot.speed_known());
}

// With an unknown blank display, the controller performs a wake/read
// transaction before calculating the requested path.
void test_unknown_state_is_read_before_speed_change() {
  Fixture f;
  f.set_led_mask(0x00);
  f.comfospot.setup();

  f.comfospot.request_speed(2);
  advance_to_panel_off(f);
  f.advance(150, 50);
  CHECK(!f.plus.driven);
  f.set_led_mask(0x03);
  f.advance(550, 50);
  CHECK(!f.plus.driven);
  f.advance(9000, 50);
  CHECK(f.comfospot.current_speed() == 2);
  confirm_operation_after_display_off(f);
}

// A mode hold is never released merely because a marker starts. It remains a
// complete eight-second gesture, then waits six seconds for the marker result.
void test_mode_change_holds_for_eight_seconds_and_confirms_after_six() {
  Fixture f;
  f.set_led_mask(0x03);
  f.comfospot.setup();
  f.advance(5100, 50);  // infer exchange from a plain speed display

  f.comfospot.request_direction(ComfoSpot::Direction::INTAKE);
  advance_to_panel_off(f);
  wake_active_panel(f, 0x03);
  f.advance(500, 50);  // 500 ms between wake and the long gesture
  CHECK(f.plus.driven);

  f.advance(1000, 50);
  blink_marker(f, 0x08, 0x03);
  CHECK(f.plus.driven);  // marker start does not release the hold

  f.advance(4500, 50);
  CHECK(f.plus.driven);  // still inside the eight-second hold
  f.advance(2100, 50);
  CHECK(!f.plus.driven);

  f.advance(7000, 50);  // six-second mode confirmation plus margin
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::INTAKE);

  confirm_operation_after_display_off(f);
}

// Physical direction synchronization is based solely on a confirmed marker.
// No qualifying physical button duration is simulated here.
void test_led_marker_synchronizes_direction_without_button_duration() {
  Fixture f;
  f.set_led_mask(0x03);
  f.comfospot.setup();

  esphome::select::Select select;
  f.comfospot.set_direction_select(&select);
  f.advance(5100, 50);  // infer Exchange from the plain speed display
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::EXCHANGE);

  blink_marker(f, 0x08, 0x03);  // LED-only Intake evidence

  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::INTAKE);
}

void test_led_marker_synchronizes_direction_with_error_latched() {
  Fixture f;
  f.set_led_mask(0x03);
  f.error.level = true;
  f.comfospot.setup();

  esphome::select::Select select;
  f.comfospot.set_direction_select(&select);
  f.advance(5100, 50);  // infer Exchange
  CHECK(f.comfospot.error_active());

  blink_marker(f, 0x08, 0x03);

  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::INTAKE);
}

void test_constant_speed_display_clears_stale_exhaust_direction() {
  Fixture f;
  f.set_led_mask(0x03);
  f.comfospot.setup();

  esphome::select::Select select;
  f.comfospot.set_direction_select(&select);
  f.advance(5100, 50);  // infer Exchange
  blink_marker(f, 0x01, 0x03);
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::EXHAUST);

  // Reproduce standby wake followed by Speed 2. The single 0x01 frame is
  // Speed 1, not an Exhaust marker; the final constant 0x03 display means
  // Exchange and must replace the stale cached Exhaust direction.
  f.set_led_mask(0x00);
  f.comfospot.request_speed(2);
  f.advance(1050, 50);
  f.advance(150, 50);  // controller wake press
  f.set_led_mask(0x0F);
  f.advance(1300, 50);  // standby acknowledgement
  f.set_led_mask(0x00);
  f.advance(550, 50);
  f.set_led_mask(0x01);  // normal Speed 1 display
  f.advance(550, 50);
  f.set_led_mask(0x03);  // normal Speed 2 display
  f.advance(9500, 50);  // state confirmation
  f.set_led_mask(0x00);
  f.advance(1100, 50);  // post-command panel-off barrier

  CHECK(f.comfospot.current_speed() == 2);
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::EXCHANGE);
}

void test_constant_speed_display_returns_direction_to_exchange() {
  Fixture f;
  f.set_led_mask(0x03);
  f.comfospot.setup();

  esphome::select::Select select;
  f.comfospot.set_direction_select(&select);
  f.advance(5100, 50);  // infer Exchange
  blink_marker(f, 0x08, 0x03);
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::INTAKE);

  // No LED1/LED4 marker follows the Intake indication. After the marker
  // expiry and a plain speed display interval, synchronize back to Exchange.
  f.set_led_mask(0x03);
  f.advance(11000, 50);
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::EXCHANGE);
}

// Exchange uses the marker for the direction being left. This is the
// operation-local mapping needed for direct Intake -> Exchange transitions.
void test_exchange_uses_source_mode_marker() {
  Fixture f;
  f.set_led_mask(0x03);
  f.comfospot.setup();
  f.advance(5100, 50);

  f.comfospot.request_direction(ComfoSpot::Direction::INTAKE);
  advance_to_panel_off(f);
  wake_active_panel(f, 0x03);
  f.advance(500, 50);
  f.advance(8000, 50);
  f.set_led_mask(0x08);
  f.advance(100, 50);
  f.set_led_mask(0x00);
  f.advance(400, 50);
  f.set_led_mask(0x08);
  f.advance(100, 50);
  f.set_led_mask(0x00);
  f.advance(400, 50);
  f.set_led_mask(0x08);
  f.advance(100, 50);
  f.set_led_mask(0x03);
  f.advance(6000, 50);
  confirm_operation_after_display_off(f);
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::INTAKE);

  f.comfospot.request_direction(ComfoSpot::Direction::EXCHANGE);
  advance_to_panel_off(f);
  wake_active_panel(f, 0x03);
  f.advance(500, 50);
  CHECK(f.plus.driven);
  blink_marker(f, 0x08, 0x03);  // source Intake marker
  f.advance(8000, 50);
  CHECK(!f.plus.driven);
  f.advance(6000, 50);
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::EXCHANGE);
  confirm_operation_after_display_off(f);
}

// A request that arrives while the controller is still waiting for the panel
// to go dark does not replace the first request. A later speed request is
// retained independently and starts only after the direction operation ends.
void test_waiting_request_keeps_first_and_queues_other_type() {
  Fixture f;
  f.set_led_mask(0x03);
  f.comfospot.setup();
  f.advance(5100, 50);  // infer exchange from a plain speed display

  f.comfospot.request_direction(ComfoSpot::Direction::INTAKE);
  f.advance(30, 10);  // reproduce a request arriving 30 ms later
  f.comfospot.request_speed(1);
  f.set_led_mask(0x00);
  f.advance(1050, 50);

  CHECK(f.plus.driven);  // the first direction request is still active
  CHECK(!f.minus.ever_driven_true);
  f.advance(150, 50);
  f.set_led_mask(0x03);
  f.advance(550, 50);
  f.advance(500, 50);
  CHECK(f.plus.driven);

  f.advance(1000, 50);
  blink_marker(f, 0x08, 0x03);
  f.advance(6600, 50);
  CHECK(!f.plus.driven);
  f.advance(7000, 50);
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::INTAKE);

  f.set_led_mask(0x00);
  f.advance(1100, 50);  // finish direction barrier and start queued speed
  CHECK(!f.minus.driven);
  f.advance(1050, 50);
  f.advance(150, 50);
  f.set_led_mask(0x03);
  f.advance(550, 50);
  CHECK(f.minus.driven);  // queued speed 1, not a replacement direction action

  f.advance(150, 50);
  // A speed display after a direction change still alternates with the
  // selected direction marker; it is not evidence of Exchange mode.
  blink_marker(f, 0x08, 0x01);
  f.advance(9000, 50);
  f.set_led_mask(0x00);
  f.advance(1100, 50);

  CHECK(f.comfospot.current_speed() == 1);
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::INTAKE);
}

// A request arriving during execution is held as one pending latest request
// and starts only after the current operation has completed its dark-panel
// barrier.
void test_executing_request_is_queued_until_current_operation_finishes() {
  Fixture f;
  f.set_led_mask(0x01);
  f.comfospot.setup();

  f.comfospot.request_speed(2);
  advance_to_panel_off(f);
  f.advance(150, 50);
  f.set_led_mask(0x01);
  f.advance(550, 50);
  f.set_led_mask(0x03);
  f.advance(150, 50);

  f.comfospot.request_speed(3);  // queue while speed 2 is confirming
  f.advance(9000, 50);
  CHECK(f.comfospot.current_speed() == 2);
  f.set_led_mask(0x00);
  f.advance(1100, 50);

  // The queued operation now starts with its own one-second dark barrier and
  // wake press; it must not have been executed during the first confirmation.
  f.advance(150, 50);
  f.set_led_mask(0x02);  // deliberately invalid; the wake read must not pass
  f.advance(550, 50);
  CHECK(f.comfospot.current_speed() == 2);
}

// Speed and direction each keep their latest pending target while an active
// operation is running. The controller dispatches direction first while the
// fan is active, then executes the retained speed target.
void test_speed_and_direction_pending_slots_coalesce_independently() {
  Fixture f;
  f.set_led_mask(0x03);  // speed 2, exchange
  f.comfospot.setup();

  f.comfospot.request_speed(3);
  advance_to_panel_off(f);
  f.advance(150, 50);
  f.set_led_mask(0x03);
  f.advance(550, 50);
  CHECK(f.plus.driven);  // active speed 3 operation

  f.comfospot.request_speed(4);
  f.comfospot.request_speed(2);  // latest speed target wins
  f.comfospot.request_direction(ComfoSpot::Direction::INTAKE);
  f.comfospot.request_direction(ComfoSpot::Direction::EXHAUST);  // latest direction wins

  f.set_led_mask(0x07);
  f.advance(150, 50);
  f.advance(9000, 50);
  f.set_led_mask(0x00);
  f.advance(1100, 50);  // finish speed 3 and begin queued direction

  f.advance(1050, 50);
  f.advance(150, 50);
  f.set_led_mask(0x07);
  f.advance(550, 50);
  f.advance(500, 50);
  CHECK(f.minus.driven);  // queued Exhaust, not the overwritten Intake

  f.advance(1000, 50);
  blink_marker(f, 0x01, 0x07);
  f.advance(6600, 50);
  CHECK(!f.minus.driven);
  f.advance(7000, 50);
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::EXHAUST);

  f.set_led_mask(0x00);
  f.advance(1100, 50);  // finish direction and begin queued speed 2
  f.advance(1050, 50);
  f.advance(150, 50);
  f.set_led_mask(0x07);
  f.advance(550, 50);
  CHECK(f.minus.driven);  // speed 2 from speed 3

  f.advance(150, 50);
  blink_marker(f, 0x01, 0x03);
  f.advance(9000, 50);
  f.set_led_mask(0x00);
  f.advance(1100, 50);

  CHECK(f.comfospot.current_speed() == 2);
  CHECK(f.comfospot.current_direction() == ComfoSpot::Direction::EXHAUST);
}

void test_status_latch_behavior_is_unchanged() {
  Fixture f;
  f.set_led_mask(0x01);
  f.comfospot.setup();

  f.filter.level = true;
  f.advance(100, 50);
  CHECK(f.comfospot.filter_change_needed());
  f.filter.level = false;
  f.advance(6100, 50);
  CHECK(!f.comfospot.filter_change_needed());
}

void test_filter_runtime_is_reported_in_days_and_restored_after_checkpoint() {
  PreferenceFixture preferences;
  {
    Fixture f;
    f.set_led_mask(0x01);
    f.comfospot.setup();

    CHECK(std::fabs(f.comfospot.filter_runtime_days()) < 0.000001f);

    esphome::test_fake_millis() = 80000000;  // 80,000 seconds: just before the 24-hour checkpoint
    f.loop();
    CHECK(std::fabs(f.comfospot.filter_runtime_days() - (80000.0f / 86400.0f)) < 0.000001f);
    CHECK(!preferences.preferences.has_value_);  // The 24-hour checkpoint has not been reached.

    esphome::test_fake_millis() = 172800000;
    f.loop();
    CHECK(std::fabs(f.comfospot.filter_runtime_days() - 2.0f) < 0.000001f);
    CHECK(preferences.preferences.has_value_);
    CHECK(preferences.preferences.slot_ == 172800);
  }

  Fixture reboot;
  reboot.set_led_mask(0x01);
  reboot.comfospot.setup();
  CHECK(reboot.comfospot.filter_runtime_seconds() == 172800);
  CHECK(std::fabs(reboot.comfospot.filter_runtime_days() - 2.0f) < 0.000001f);
}

void test_confirmed_filter_reset_persists_zero_runtime() {
  PreferenceFixture preferences;
  preferences.preferences.slot_ = 123456;
  preferences.preferences.has_value_ = true;

  {
    Fixture f;
    f.set_led_mask(0x01);
    f.comfospot.setup();
    CHECK(f.comfospot.filter_runtime_seconds() == 123456);

    // The reset is valid even when the filter warning is already inactive.
    f.comfospot.reset_filter();
    f.set_led_mask(0x00);
    f.advance(11000, 10);  // panel-off barrier, reset hold, and confirmation

    CHECK(preferences.preferences.has_value_);
    CHECK(preferences.preferences.slot_ == 0);
  }

  Fixture reboot;
  reboot.set_led_mask(0x01);
  reboot.comfospot.setup();
  CHECK(reboot.comfospot.filter_runtime_seconds() == 0);
}

}  // namespace

int main() {
  test_boot_state_sync();
  test_boot_blank_is_unknown();
  test_speed_sequence_uses_fixed_timing_and_final_confirmation();
  test_failed_speed_operation_rolls_back_optimistic_state();
  test_standby_wake_is_not_speed_four();
  test_speed_four_to_one_after_panel_blank_uses_minus_and_recovers_actual_speed();
  test_unknown_state_is_read_before_speed_change();
  test_mode_change_holds_for_eight_seconds_and_confirms_after_six();
  test_led_marker_synchronizes_direction_without_button_duration();
  test_led_marker_synchronizes_direction_with_error_latched();
  test_constant_speed_display_clears_stale_exhaust_direction();
  test_constant_speed_display_returns_direction_to_exchange();
  test_exchange_uses_source_mode_marker();
  test_waiting_request_keeps_first_and_queues_other_type();
  test_executing_request_is_queued_until_current_operation_finishes();
  test_speed_and_direction_pending_slots_coalesce_independently();
  test_status_latch_behavior_is_unchanged();
  test_filter_runtime_is_reported_in_days_and_restored_after_checkpoint();
  test_confirmed_filter_reset_persists_zero_runtime();

  if (g_failures > 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("all comfospot state machine tests passed\n");
  return 0;
}
