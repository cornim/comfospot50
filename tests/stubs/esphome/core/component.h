#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "esphome/core/log.h"

// Minimal host-test stand-in for esphome/core/component.h.
//
// Only the subset of the real Component surface that components/comfospot
// actually calls is reproduced here: the setup()/loop()/dump_config()
// virtuals, the failed/warning/error status flags used for safety
// reporting, and the setup_priority constants referenced by
// ComfoSpot::get_setup_priority(). The real header pulls `clamp` into the
// esphome namespace via esphome/core/helpers.h; reproduce that here since
// comfospot.cpp uses it unqualified. It also transitively includes
// esphome/core/log.h, which matters here: components/comfospot's adapter
// headers (comfospot_fan.h, comfospot_sensor.h, etc.) call
// this->get_name() only as arguments to ESP_LOGCONFIG(...), and that macro
// must already be defined by the time those headers are processed so the
// (otherwise unimplemented, entity-registry-backed) get_name() call is
// discarded as dead macro-argument text rather than compiled.

namespace esphome {

using std::clamp;

namespace setup_priority {
constexpr float HARDWARE = 800.0f;
constexpr float DATA = 600.0f;
}  // namespace setup_priority

class Component {
 public:
  virtual ~Component() = default;

  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  virtual void on_shutdown() {}
  virtual float get_setup_priority() const { return setup_priority::DATA; }

  bool is_failed() const { return failed_; }
  void mark_failed() { failed_ = true; }

  bool status_has_warning() const { return warning_; }
  bool status_has_error() const { return error_; }
  void status_set_warning() { warning_ = true; }
  void status_set_error() { error_ = true; }
  void status_clear_warning() { warning_ = false; }
  void status_clear_error() { error_ = false; }

 private:
  bool failed_{false};
  bool warning_{false};
  bool error_{false};
};

class PollingComponent : public Component {
 public:
  PollingComponent() = default;
  explicit PollingComponent(uint32_t update_interval) : update_interval_(update_interval) {}
  virtual void update() {}

 private:
  uint32_t update_interval_{0};
};

}  // namespace esphome
