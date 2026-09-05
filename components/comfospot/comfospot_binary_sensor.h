#pragma once

#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include "comfospot.h"

namespace esphome::comfospot {

class ComfoSpotBinarySensor final : public Component, public binary_sensor::BinarySensor {
 public:
  ComfoSpotBinarySensor(ComfoSpot *parent, bool error, bool auto_mode)
      : parent_(parent), error_(error), auto_mode_(auto_mode) {}

  void setup() override {
    if (this->auto_mode_) {
      this->parent_->set_auto_sensor(this);
    } else if (this->error_) {
      this->parent_->set_error_sensor(this);
    } else {
      this->parent_->set_filter_sensor(this);
    }
  }
  void dump_config() override {
    ESP_LOGCONFIG("comfospot", "ComfoSpot Binary Sensor '%s'", this->get_name().c_str());
  }

 protected:
  ComfoSpot *parent_;
  bool error_;
  bool auto_mode_;
};

}  // namespace esphome::comfospot
