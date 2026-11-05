/* InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "base/Log.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace inputleap {

// Per-owner statistics; callers must use the owner's usual synchronization.
class InputStatistics {
public:
    using Clock = std::chrono::steady_clock;
    bool enabled() const { return enabled_; }

    void record(const char* metric, const char* unit, double value)
    {
        if (!enabled_) {
            return;
        }
        ++count_;
        total_ += value;
        maximum_ = (std::max)(maximum_, value);
        const auto now = Clock::now();
        if (now - since_ >= std::chrono::seconds(5)) {
            LOG_INFO("input-stats %s samples=%u avg=%.3f max=%.3f %s",
                     metric, count_, total_ / count_, maximum_, unit);
            count_ = 0;
            total_ = maximum_ = 0;
            since_ = now;
        }
    }

private:
    bool enabled_ = [] {
        const auto* value = std::getenv("INPUTLEAP_INPUT_STATS");
        return value != nullptr && value[0] == '1' && value[1] == '\0';
    }();
    Clock::time_point since_ = Clock::now();
    unsigned int count_ = 0;
    double total_ = 0;
    double maximum_ = 0;
};

} // namespace inputleap
