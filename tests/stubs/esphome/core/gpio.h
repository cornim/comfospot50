#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

// Minimal host-test stand-in for esphome/core/gpio.h.
//
// components/comfospot only needs the abstract GPIOPin read/write surface;
// tests provide a FakeGpioPin implementation (see tests/test_helpers.h).

namespace esphome {

class GPIOPin {
 public:
  virtual ~GPIOPin() = default;
  virtual void setup() {}
  virtual bool digital_read() = 0;
  virtual void digital_write(bool value) = 0;
};

}  // namespace esphome
