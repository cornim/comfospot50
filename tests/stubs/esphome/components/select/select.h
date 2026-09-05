#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstddef>

// Minimal host-test stand-in for esphome/components/select/select.h.

namespace esphome::select {

class Select {
 public:
  virtual ~Select() = default;
  virtual void publish_state(size_t index) {
    this->has_state = true;
    this->state_index = index;
  }

  bool has_state{false};
  size_t state_index{0};

 protected:
  virtual void control(size_t index) { (void) index; }
};

}  // namespace esphome::select
