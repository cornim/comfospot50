#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdint>

// Minimal host-test stand-in for esphome/core/preferences.h.
//
// components/comfospot only ever stores a single uint32_t (the filter
// runtime base, in seconds) under one preference key, so this fake backs
// every preference object with one shared durable slot and one pending slot.
// Tests can point global_preferences at a FakePreferences instance (see
// tests/test_helpers.h) or leave it null to exercise the "no preferences
// available" warning path.

namespace esphome {

class ESPPreferenceObject {
 public:
  ESPPreferenceObject() = default;
  explicit ESPPreferenceObject(uint32_t *slot, bool *has_value, uint32_t *pending_slot, bool *pending_has_value,
                               bool *force_save_failure)
      : slot_(slot), has_value_(has_value), pending_slot_(pending_slot), pending_has_value_(pending_has_value),
        force_save_failure_(force_save_failure) {}

  template<typename T> bool save(const T *src) {
    static_assert(sizeof(T) == sizeof(uint32_t), "fake preference only supports a uint32_t payload");
    if (pending_slot_ == nullptr)
      return false;
    if (force_save_failure_ != nullptr && *force_save_failure_)
      return false;
    *pending_slot_ = *reinterpret_cast<const uint32_t *>(src);
    if (pending_has_value_ != nullptr)
      *pending_has_value_ = true;
    return true;
  }

  template<typename T> bool load(T *dest) {
    static_assert(sizeof(T) == sizeof(uint32_t), "fake preference only supports a uint32_t payload");
    if (slot_ == nullptr || has_value_ == nullptr || !*has_value_)
      return false;
    *reinterpret_cast<uint32_t *>(dest) = *slot_;
    return true;
  }

 private:
  uint32_t *slot_{nullptr};
  bool *has_value_{nullptr};
  uint32_t *pending_slot_{nullptr};
  bool *pending_has_value_{nullptr};
  bool *force_save_failure_{nullptr};
};

class ESPPreferences {
 public:
  template<typename T> ESPPreferenceObject make_preference(uint32_t /*type*/) {
    return ESPPreferenceObject(&this->slot_, &this->has_value_, &this->pending_slot_, &this->pending_has_value_,
                               &this->force_save_failure);
  }

  bool sync() {
    if (this->force_sync_failure)
      return false;
    if (this->pending_has_value_) {
      this->slot_ = this->pending_slot_;
      this->has_value_ = true;
      this->pending_has_value_ = false;
    }
    return true;
  }

  uint32_t slot_{0};
  bool has_value_{false};
  uint32_t pending_slot_{0};
  bool pending_has_value_{false};
  bool force_save_failure{false};
  bool force_sync_failure{false};
};

// Definition lives in the test translation unit; tests point this at a
// FakePreferences instance, or leave it null.
extern ESPPreferences *global_preferences;

}  // namespace esphome
