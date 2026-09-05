#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

// Minimal host-test stand-in for esphome/components/binary_sensor/binary_sensor.h.

namespace esphome::binary_sensor {

class BinarySensor {
 public:
  virtual ~BinarySensor() = default;
  virtual void publish_state(bool state) { this->state = state; }

  bool state{false};
};

}  // namespace esphome::binary_sensor
