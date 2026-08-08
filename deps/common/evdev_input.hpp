/*************************************************************************
 * This file is part of input-overlay.
 *
 * Linux evdev input backend used when a compositor does not expose global
 * keyboard and pointer events (notably Wayland).
 *************************************************************************/

#pragma once

#include <cstddef>
#include <uiohook.h>

namespace evdev_input {

struct start_result {
    std::size_t device_count{};
    std::size_t permission_denied{};

    explicit operator bool() const { return device_count != 0; }
};

start_result start(dispatcher_t dispatch);
void stop();
bool is_running();

} // namespace evdev_input
