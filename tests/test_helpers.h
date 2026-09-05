#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

// Test harness for host-testing esphome::comfospot::ComfoSpot without real
// ESPHome/ESP-IDF dependencies. See tests/stubs/esphome/... for the fake
// component surface this relies on.

#include <cstdint>

#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"

#include "comfospot.h"

namespace comfospot_test {

/// A plain input-only fake GPIO pin. Tests set `level` directly to
/// simulate a panel LED or status line (already normalized to the logical,
/// post-inversion value ComfoSpot expects: true means "lit"/"asserted").
struct FakeInputPin : public esphome::GPIOPin {
  bool level{false};
  bool digital_read() override { return this->level; }
  void digital_write(bool) override {}
};

/// A fake for the bidirectional `+`/`-` button lines. Real hardware is
/// open-drain and inverted, so the asserted (logical true) state is the
/// logical OR of "the controller is driving it" and "a person is physically
/// holding the button", exactly like a wired-AND active-low line viewed
/// through the inversion layer. `external_pressed` simulates a physical
/// user; `driven` records ComfoSpot's own last commanded output.
///
/// Note that ComfoSpot::set_outputs_() writes *both* button pins on every
/// call (whichever isn't asserted is explicitly written false), so
/// `write_count` alone cannot tell which line was actually pressed.
/// `ever_driven_true` is the signal to use for that.
struct FakeButtonPin : public esphome::GPIOPin {
  bool external_pressed{false};
  bool driven{false};
  bool ever_driven_true{false};
  int write_count{0};

  bool digital_read() override { return this->external_pressed || this->driven; }
  void digital_write(bool value) override {
    this->driven = value;
    this->write_count++;
    if (value)
      this->ever_driven_true = true;
  }
};

/// Test double for esphome::ESPPreferences, giving tests control over
/// save/sync failures to exercise ComfoSpot's preference-warning paths.
using esphome::ESPPreferences;

/// Advances the fake clock in fixed steps, calling loop() after each step,
/// which mirrors how ESPHome repeatedly calls Component::loop() from the
/// scheduler rather than sleeping for the full duration at once.
template<typename LoopFn> void advance(uint32_t total_ms, LoopFn &&loop, uint32_t step_ms = 10) {
  uint32_t elapsed = 0;
  while (elapsed < total_ms) {
    uint32_t inc = step_ms < (total_ms - elapsed) ? step_ms : (total_ms - elapsed);
    esphome::test_fake_millis() += inc;
    elapsed += inc;
    loop();
  }
}

/// Bundles a ComfoSpot instance with fake pins for all nine panel signals.
struct Fixture {
  esphome::comfospot::ComfoSpot comfospot;
  FakeInputPin led_1, led_2, led_3, led_4;
  FakeInputPin filter, error, auto_mode;
  FakeButtonPin plus, minus;

  Fixture() {
    esphome::test_fake_millis() = 0;
    this->comfospot.set_led_1_pin(&this->led_1);
    this->comfospot.set_led_2_pin(&this->led_2);
    this->comfospot.set_led_3_pin(&this->led_3);
    this->comfospot.set_led_4_pin(&this->led_4);
    this->comfospot.set_filter_pin(&this->filter);
    this->comfospot.set_error_pin(&this->error);
    this->comfospot.set_auto_pin(&this->auto_mode);
    this->comfospot.set_plus_pin(&this->plus);
    this->comfospot.set_minus_pin(&this->minus);
  }

  /// Sets the four fan LEDs from a speed_for_led_mask()-compatible bitmask
  /// (bit0=led_1, bit1=led_2, bit2=led_3, bit3=led_4), matching the wiring
  /// documented in README.md (speed N lights led_1..led_N).
  void set_led_mask(uint8_t mask) {
    this->led_1.level = (mask & 0x01) != 0;
    this->led_2.level = (mask & 0x02) != 0;
    this->led_3.level = (mask & 0x04) != 0;
    this->led_4.level = (mask & 0x08) != 0;
  }

  void loop() { this->comfospot.loop(); }
  void advance(uint32_t total_ms, uint32_t step_ms = 10) {
    comfospot_test::advance(total_ms, [this]() { this->loop(); }, step_ms);
  }
};

}  // namespace comfospot_test
