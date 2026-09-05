#pragma once

#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include "comfospot.h"

namespace esphome::comfospot {

class ComfoSpotSelect final : public Component, public select::Select {
 public:
  explicit ComfoSpotSelect(ComfoSpot *parent) : parent_(parent) {}

  void setup() override { this->parent_->set_direction_select(this); }
  void dump_config() override {
    ESP_LOGCONFIG("comfospot", "ComfoSpot Direction '%s'", this->get_name().c_str());
  }

 protected:
  void control(size_t index) override {
    if (index <= static_cast<size_t>(ComfoSpot::Direction::EXHAUST)) {
      this->parent_->request_direction(static_cast<ComfoSpot::Direction>(index));
    }
  }

  ComfoSpot *parent_;
};

}  // namespace esphome::comfospot
