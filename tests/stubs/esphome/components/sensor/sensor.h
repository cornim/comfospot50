#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

// Minimal host-test stand-in for esphome/components/sensor/sensor.h.

namespace esphome::sensor {

class Sensor {
 public:
  virtual ~Sensor() = default;
  virtual void publish_state(float state) {
    this->has_state = true;
    this->state = state;
    this->publish_count++;
  }

  bool has_state{false};
  float state{0};
  int publish_count{0};
};

}  // namespace esphome::sensor
