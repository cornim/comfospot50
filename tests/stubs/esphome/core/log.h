#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

// Minimal host-test stand-in for esphome/core/log.h.
//
// The real header provides printf-style logging macros backed by the
// platform's logger. Host tests don't need real log output, so every macro
// is a no-op that still evaluates none of its arguments (arguments in
// production call sites may reference `this` members only valid on real
// hardware paths, but for the subset used by ComfoSpot, discarding the
// entire expression list is safe and keeps builds warning-free).

#define ESP_LOGE(...) ((void) 0)
#define ESP_LOGW(...) ((void) 0)
#define ESP_LOGI(...) ((void) 0)
#define ESP_LOGCONFIG(...) ((void) 0)
#define ESP_LOGD(...) ((void) 0)
#define ESP_LOGV(...) ((void) 0)
#define ESP_LOGVV(...) ((void) 0)
