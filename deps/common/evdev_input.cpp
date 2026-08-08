/*************************************************************************
 * This file is part of input-overlay.
 *
 * Linux evdev input backend used when a compositor does not expose global
 * keyboard and pointer events (notably Wayland).
 *************************************************************************/

#include "evdev_input.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <linux/input.h>
#include <mutex>
#include <poll.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <sys/ioctl.h>
#include <unistd.h>

namespace evdev_input {
namespace {

constexpr std::size_t bits_per_word = sizeof(unsigned long) * 8;

struct device {
    int fd{-1};
    std::string path;
    std::unordered_set<uint16_t> pressed_keys;
    std::unordered_set<uint16_t> pressed_buttons;
    int relative_x{};
    int relative_y{};
    int wheel{};
    int horizontal_wheel{};
    int high_resolution_wheel{};
    int high_resolution_horizontal_wheel{};
    bool dropped{};
};

std::atomic<bool> running{};
std::thread input_thread;
std::mutex lifecycle_mutex;
dispatcher_t dispatch_event{};

uint16_t linux_to_vcode(uint16_t code)
{
    switch (code) {
    case KEY_ESC:
        return VC_ESCAPE;
    case KEY_F1:
        return VC_F1;
    case KEY_F2:
        return VC_F2;
    case KEY_F3:
        return VC_F3;
    case KEY_F4:
        return VC_F4;
    case KEY_F5:
        return VC_F5;
    case KEY_F6:
        return VC_F6;
    case KEY_F7:
        return VC_F7;
    case KEY_F8:
        return VC_F8;
    case KEY_F9:
        return VC_F9;
    case KEY_F10:
        return VC_F10;
    case KEY_F11:
        return VC_F11;
    case KEY_F12:
        return VC_F12;
    case KEY_F13:
        return VC_F13;
    case KEY_F14:
        return VC_F14;
    case KEY_F15:
        return VC_F15;
    case KEY_F16:
        return VC_F16;
    case KEY_F17:
        return VC_F17;
    case KEY_F18:
        return VC_F18;
    case KEY_F19:
        return VC_F19;
    case KEY_F20:
        return VC_F20;
    case KEY_F21:
        return VC_F21;
    case KEY_F22:
        return VC_F22;
    case KEY_F23:
        return VC_F23;
    case KEY_F24:
        return VC_F24;
    case KEY_GRAVE:
        return VC_BACK_QUOTE;
    case KEY_1:
        return VC_1;
    case KEY_2:
        return VC_2;
    case KEY_3:
        return VC_3;
    case KEY_4:
        return VC_4;
    case KEY_5:
        return VC_5;
    case KEY_6:
        return VC_6;
    case KEY_7:
        return VC_7;
    case KEY_8:
        return VC_8;
    case KEY_9:
        return VC_9;
    case KEY_0:
        return VC_0;
    case KEY_MINUS:
        return VC_MINUS;
    case KEY_EQUAL:
        return VC_EQUALS;
    case KEY_BACKSPACE:
        return VC_BACKSPACE;
    case KEY_TAB:
        return VC_TAB;
    case KEY_Q:
        return VC_Q;
    case KEY_W:
        return VC_W;
    case KEY_E:
        return VC_E;
    case KEY_R:
        return VC_R;
    case KEY_T:
        return VC_T;
    case KEY_Y:
        return VC_Y;
    case KEY_U:
        return VC_U;
    case KEY_I:
        return VC_I;
    case KEY_O:
        return VC_O;
    case KEY_P:
        return VC_P;
    case KEY_LEFTBRACE:
        return VC_OPEN_BRACKET;
    case KEY_RIGHTBRACE:
        return VC_CLOSE_BRACKET;
    case KEY_ENTER:
        return VC_ENTER;
    case KEY_CAPSLOCK:
        return VC_CAPS_LOCK;
    case KEY_A:
        return VC_A;
    case KEY_S:
        return VC_S;
    case KEY_D:
        return VC_D;
    case KEY_F:
        return VC_F;
    case KEY_G:
        return VC_G;
    case KEY_H:
        return VC_H;
    case KEY_J:
        return VC_J;
    case KEY_K:
        return VC_K;
    case KEY_L:
        return VC_L;
    case KEY_SEMICOLON:
        return VC_SEMICOLON;
    case KEY_APOSTROPHE:
        return VC_QUOTE;
    case KEY_BACKSLASH:
        return VC_BACK_SLASH;
    case KEY_LEFTSHIFT:
        return VC_SHIFT_L;
    case KEY_Z:
        return VC_Z;
    case KEY_X:
        return VC_X;
    case KEY_C:
        return VC_C;
    case KEY_V:
        return VC_V;
    case KEY_B:
        return VC_B;
    case KEY_N:
        return VC_N;
    case KEY_M:
        return VC_M;
    case KEY_COMMA:
        return VC_COMMA;
    case KEY_DOT:
        return VC_PERIOD;
    case KEY_SLASH:
        return VC_SLASH;
    case KEY_RIGHTSHIFT:
        return VC_SHIFT_R;
    case KEY_102ND:
        return VC_102;
    case KEY_LEFTALT:
        return VC_ALT_L;
    case KEY_LEFTCTRL:
        return VC_CONTROL_L;
    case KEY_LEFTMETA:
        return VC_META_L;
    case KEY_SPACE:
        return VC_SPACE;
    case KEY_RIGHTMETA:
        return VC_META_R;
    case KEY_RIGHTCTRL:
        return VC_CONTROL_R;
    case KEY_RIGHTALT:
        return VC_ALT_R;
    case KEY_COMPOSE:
        return VC_CONTEXT_MENU;
    case KEY_SYSRQ:
        return VC_PRINT_SCREEN;
    case KEY_SCROLLLOCK:
        return VC_SCROLL_LOCK;
    case KEY_PAUSE:
        return VC_PAUSE;
    case KEY_INSERT:
        return VC_INSERT;
    case KEY_HOME:
        return VC_HOME;
    case KEY_PAGEUP:
        return VC_PAGE_UP;
    case KEY_DELETE:
        return VC_DELETE;
    case KEY_END:
        return VC_END;
    case KEY_PAGEDOWN:
        return VC_PAGE_DOWN;
    case KEY_UP:
        return VC_UP;
    case KEY_LEFT:
        return VC_LEFT;
    case KEY_DOWN:
        return VC_DOWN;
    case KEY_RIGHT:
        return VC_RIGHT;
    case KEY_NUMLOCK:
        return VC_NUM_LOCK;
    case KEY_KPSLASH:
        return VC_KP_DIVIDE;
    case KEY_KPASTERISK:
        return VC_KP_MULTIPLY;
    case KEY_KPMINUS:
        return VC_KP_SUBTRACT;
    case KEY_KP7:
        return VC_KP_7;
    case KEY_KP8:
        return VC_KP_8;
    case KEY_KP9:
        return VC_KP_9;
    case KEY_KPPLUS:
        return VC_KP_ADD;
    case KEY_KP4:
        return VC_KP_4;
    case KEY_KP5:
        return VC_KP_5;
    case KEY_KP6:
        return VC_KP_6;
    case KEY_KP1:
        return VC_KP_1;
    case KEY_KP2:
        return VC_KP_2;
    case KEY_KP3:
        return VC_KP_3;
    case KEY_KPENTER:
        return VC_KP_ENTER;
    case KEY_KP0:
        return VC_KP_0;
    case KEY_KPDOT:
        return VC_KP_DECIMAL;
    case KEY_KPEQUAL:
        return VC_KP_EQUALS;
    case KEY_KPCOMMA:
        return VC_KP_SEPARATOR;
    case KEY_KPLEFTPAREN:
        return VC_KP_OPEN_PARENTHESIS;
    case KEY_KPRIGHTPAREN:
        return VC_KP_CLOSE_PARENTHESIS;
    case KEY_KATAKANAHIRAGANA:
        return VC_KATAKANA_HIRAGANA;
    case KEY_HENKAN:
        return VC_CONVERT;
    case KEY_MUHENKAN:
        return VC_NONCONVERT;
    case KEY_YEN:
        return VC_YEN;
    case KEY_KATAKANA:
        return VC_KATAKANA;
    case KEY_HIRAGANA:
        return VC_HIRAGANA;
    case KEY_HANGEUL:
        return VC_HANGUL;
    case KEY_HANJA:
        return VC_HANJA;
    case KEY_MUTE:
        return VC_VOLUME_MUTE;
    case KEY_VOLUMEDOWN:
        return VC_VOLUME_DOWN;
    case KEY_VOLUMEUP:
        return VC_VOLUME_UP;
    case KEY_POWER:
        return VC_POWER;
    case KEY_SLEEP:
        return VC_SLEEP;
    case KEY_WAKEUP:
        return VC_WAKE;
    case KEY_STOP:
        return VC_STOP;
    case KEY_AGAIN:
        return VC_AGAIN;
    case KEY_PROPS:
        return VC_PROPS;
    case KEY_UNDO:
        return VC_UNDO;
    case KEY_FRONT:
        return VC_FRONT;
    case KEY_COPY:
        return VC_COPY;
    case KEY_OPEN:
        return VC_OPEN;
    case KEY_PASTE:
        return VC_PASTE;
    case KEY_FIND:
        return VC_FIND;
    case KEY_CUT:
        return VC_CUT;
    case KEY_HELP:
        return VC_HELP;
    case KEY_CALC:
        return VC_APP_CALCULATOR;
    case KEY_MAIL:
        return VC_APP_MAIL;
    case KEY_WWW:
        return VC_APP_BROWSER;
    case KEY_HOMEPAGE:
        return VC_BROWSER_HOME;
    case KEY_BACK:
        return VC_BROWSER_BACK;
    case KEY_FORWARD:
        return VC_BROWSER_FORWARD;
    case KEY_REFRESH:
        return VC_BROWSER_REFRESH;
    case KEY_BOOKMARKS:
        return VC_BROWSER_FAVORITES;
    case KEY_SEARCH:
        return VC_BROWSER_SEARCH;
    case KEY_NEXTSONG:
        return VC_MEDIA_NEXT;
    case KEY_PLAYPAUSE:
        return VC_MEDIA_PLAY;
    case KEY_PREVIOUSSONG:
        return VC_MEDIA_PREVIOUS;
    case KEY_STOPCD:
        return VC_MEDIA_STOP;
    case KEY_RECORD:
        return VC_MEDIA_RECORD;
    case KEY_REWIND:
        return VC_MEDIA_REWIND;
    case KEY_FASTFORWARD:
        return VC_FAST_FORWARD;
    case KEY_EJECTCD:
        return VC_MEDIA_EJECT;
    case KEY_BRIGHTNESSDOWN:
        return VC_BRIGTNESS_DOWN;
    case KEY_BRIGHTNESSUP:
        return VC_BRIGTNESS_UP;
    case KEY_BRIGHTNESS_CYCLE:
        return VC_BRIGTNESS_CYCLE;
    case KEY_BRIGHTNESS_AUTO:
        return VC_BRIGTNESS_AUTO;
    case KEY_DISPLAY_OFF:
        return VC_DISPLAY_OFF;
    case KEY_SWITCHVIDEOMODE:
        return VC_SWITCH_VIDEO_MODE;
    case KEY_KBDILLUMTOGGLE:
        return VC_KEYBOARD_LIGHT_TOGGLE;
    case KEY_KBDILLUMDOWN:
        return VC_KEYBOARD_LIGHT_DOWN;
    case KEY_KBDILLUMUP:
        return VC_KEYBOARD_LIGHT_UP;
    default:
        return VC_UNDEFINED;
    }
}

template<std::size_t N> bool bit_is_set(const std::array<unsigned long, N> &bits, unsigned int bit)
{
    return bit / bits_per_word < N && (bits[bit / bits_per_word] & (1UL << (bit % bits_per_word))) != 0;
}

uint16_t linux_to_mouse_button(uint16_t code)
{
    switch (code) {
    case BTN_LEFT:
        return MOUSE_BUTTON1;
    case BTN_RIGHT:
        return MOUSE_BUTTON2;
    case BTN_MIDDLE:
        return MOUSE_BUTTON3;
    case BTN_SIDE:
    case BTN_BACK:
        return MOUSE_BUTTON4;
    case BTN_EXTRA:
    case BTN_FORWARD:
        return MOUSE_BUTTON5;
    default:
        return MOUSE_NOBUTTON;
    }
}

bool supports_input(int fd)
{
    std::array<unsigned long, (EV_MAX / bits_per_word) + 1> event_types{};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(event_types)), event_types.data()) < 0)
        return false;

    std::array<unsigned long, (KEY_MAX / bits_per_word) + 1> keys{};
    if (bit_is_set(event_types, EV_KEY) && ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys.data()) >= 0) {
        for (unsigned int code = 0; code <= KEY_MAX; ++code) {
            if (!bit_is_set(keys, code))
                continue;
            if (linux_to_vcode(static_cast<uint16_t>(code)) != VC_UNDEFINED ||
                linux_to_mouse_button(static_cast<uint16_t>(code)) != MOUSE_NOBUTTON)
                return true;
        }
    }

    std::array<unsigned long, (REL_MAX / bits_per_word) + 1> relative_axes{};
    if (bit_is_set(event_types, EV_REL) &&
        ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relative_axes)), relative_axes.data()) >= 0) {
        for (const unsigned int code : {REL_X, REL_Y, REL_WHEEL, REL_HWHEEL, REL_WHEEL_HI_RES, REL_HWHEEL_HI_RES}) {
            if (bit_is_set(relative_axes, code))
                return true;
        }
    }

    return false;
}

void clear_relative_frame(device &source)
{
    source.relative_x = 0;
    source.relative_y = 0;
    source.wheel = 0;
    source.horizontal_wheel = 0;
    source.high_resolution_wheel = 0;
    source.high_resolution_horizontal_wheel = 0;
}

bool is_relative_event(uint16_t code)
{
    switch (code) {
    case REL_X:
    case REL_Y:
    case REL_WHEEL:
    case REL_HWHEEL:
    case REL_WHEEL_HI_RES:
    case REL_HWHEEL_HI_RES:
        return true;
    default:
        return false;
    }
}

std::vector<std::string> event_paths()
{
    std::vector<std::string> paths;
    std::error_code error;
    std::filesystem::directory_iterator iterator("/dev/input", error);
    const std::filesystem::directory_iterator end;
    for (; !error && iterator != end; iterator.increment(error)) {
        const auto name = iterator->path().filename().string();
        if (name.rfind("event", 0) != 0 || name.size() == 5 ||
            !std::all_of(name.begin() + 5, name.end(), [](unsigned char c) { return c >= '0' && c <= '9'; }))
            continue;
        paths.emplace_back(iterator->path().string());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

bool already_open(const std::vector<device> &devices, const std::string &path)
{
    return std::any_of(devices.begin(), devices.end(), [&](const device &item) { return item.path == path; });
}

void scan_devices(std::vector<device> &devices, std::size_t &permission_denied)
{
    for (const auto &path : event_paths()) {
        if (already_open(devices, path))
            continue;

        const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            if (errno == EACCES || errno == EPERM)
                ++permission_denied;
            continue;
        }
        if (!supports_input(fd)) {
            close(fd);
            continue;
        }

        int clock_id = CLOCK_MONOTONIC;
        ioctl(fd, EVIOCSCLOCKID, &clock_id);
        device item;
        item.fd = fd;
        item.path = path;
        devices.emplace_back(std::move(item));
    }
}

uint64_t event_time()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

uint16_t modifier_for(uint16_t vcode)
{
    switch (vcode) {
    case VC_SHIFT_L:
        return MASK_SHIFT_L;
    case VC_SHIFT_R:
        return MASK_SHIFT_R;
    case VC_CONTROL_L:
        return MASK_CTRL_L;
    case VC_CONTROL_R:
        return MASK_CTRL_R;
    case VC_META_L:
        return MASK_META_L;
    case VC_META_R:
        return MASK_META_R;
    case VC_ALT_L:
        return MASK_ALT_L;
    case VC_ALT_R:
        return MASK_ALT_R;
    default:
        return 0;
    }
}

uint16_t modifier_for_mouse_button(uint16_t button)
{
    switch (button) {
    case MOUSE_BUTTON1:
        return MASK_BUTTON1;
    case MOUSE_BUTTON2:
        return MASK_BUTTON2;
    case MOUSE_BUTTON3:
        return MASK_BUTTON3;
    case MOUSE_BUTTON4:
        return MASK_BUTTON4;
    case MOUSE_BUTTON5:
        return MASK_BUTTON5;
    default:
        return 0;
    }
}

void send_hook_event(event_type type)
{
    uiohook_event event{};
    event.type = type;
    event.time = event_time();
    dispatch_event(&event, nullptr);
}

void send_key_event(event_type type, uint16_t code, uint16_t modifiers)
{
    const uint16_t vcode = linux_to_vcode(code);
    if (vcode == VC_UNDEFINED)
        return;

    uiohook_event event{};
    event.type = type;
    event.time = event_time();
    event.mask = modifiers;
    event.data.keyboard.keycode = vcode;
    event.data.keyboard.rawcode = code;
    event.data.keyboard.keychar = CHAR_UNDEFINED;
    dispatch_event(&event, nullptr);
}

void send_mouse_button_event(event_type type, uint16_t button, uint16_t modifiers)
{
    uiohook_event event{};
    event.type = type;
    event.time = event_time();
    event.mask = modifiers;
    event.data.mouse.button = button;
    event.data.mouse.clicks = 1;
    dispatch_event(&event, nullptr);
}

int16_t clamp_to_int16(int value)
{
    return static_cast<int16_t>(std::clamp(value, static_cast<int>(INT16_MIN), static_cast<int>(INT16_MAX)));
}

void send_mouse_motion_event(int x, int y, uint16_t modifiers)
{
    if (x == 0 && y == 0)
        return;

    uiohook_event event{};
    event.type = EVENT_MOUSE_MOVED_RELATIVE_TO_CURSOR;
    event.time = event_time();
    event.mask = modifiers;
    event.data.mouse.button = MOUSE_NOBUTTON;
    event.data.mouse.x = clamp_to_int16(x);
    event.data.mouse.y = clamp_to_int16(y);
    dispatch_event(&event, nullptr);
}

void send_mouse_wheel_event(int rotation, uint8_t direction, uint16_t modifiers)
{
    if (rotation == 0)
        return;

    uiohook_event event{};
    event.type = EVENT_MOUSE_WHEEL;
    event.time = event_time();
    event.mask = modifiers;
    event.data.wheel.type = WHEEL_UNIT_SCROLL;
    event.data.wheel.rotation = clamp_to_int16(rotation);
    event.data.wheel.delta = 120;
    event.data.wheel.direction = direction;
    dispatch_event(&event, nullptr);
}

void update_key(device &source, uint16_t code, bool pressed, bool repeat,
                std::unordered_map<uint16_t, unsigned int> &press_count, uint16_t &modifiers)
{
    const uint16_t vcode = linux_to_vcode(code);
    if (vcode == VC_UNDEFINED)
        return;

    if (repeat) {
        if (source.pressed_keys.count(code) != 0)
            send_key_event(EVENT_KEY_PRESSED, code, modifiers);
        return;
    }

    const bool was_pressed = source.pressed_keys.count(code) != 0;
    if (pressed == was_pressed)
        return;

    auto &count = press_count[code];
    if (pressed) {
        source.pressed_keys.insert(code);
        ++count;
        if (count != 1)
            return;
        modifiers |= modifier_for(vcode);
        send_key_event(EVENT_KEY_PRESSED, code, modifiers);
    } else {
        source.pressed_keys.erase(code);
        if (count > 0)
            --count;
        if (count != 0)
            return;
        press_count.erase(code);
        modifiers &= static_cast<uint16_t>(~modifier_for(vcode));
        send_key_event(EVENT_KEY_RELEASED, code, modifiers);
    }
}

void update_mouse_button(device &source, uint16_t code, bool pressed,
                         std::unordered_map<uint16_t, unsigned int> &press_count, uint16_t &modifiers)
{
    const uint16_t button = linux_to_mouse_button(code);
    if (button == MOUSE_NOBUTTON)
        return;

    const bool was_pressed = source.pressed_buttons.count(code) != 0;
    if (pressed == was_pressed)
        return;

    auto &count = press_count[button];
    if (pressed) {
        source.pressed_buttons.insert(code);
        ++count;
        if (count != 1)
            return;
        modifiers |= modifier_for_mouse_button(button);
        send_mouse_button_event(EVENT_MOUSE_PRESSED, button, modifiers);
    } else {
        source.pressed_buttons.erase(code);
        if (count > 0)
            --count;
        if (count != 0)
            return;
        press_count.erase(button);
        modifiers &= static_cast<uint16_t>(~modifier_for_mouse_button(button));
        send_mouse_button_event(EVENT_MOUSE_RELEASED, button, modifiers);
        send_mouse_button_event(EVENT_MOUSE_CLICKED, button, modifiers);
    }
}

void release_device(device &source, std::unordered_map<uint16_t, unsigned int> &key_press_count,
                    std::unordered_map<uint16_t, unsigned int> &button_press_count, uint16_t &modifiers)
{
    const auto pressed_keys = source.pressed_keys;
    for (const uint16_t code : pressed_keys)
        update_key(source, code, false, false, key_press_count, modifiers);

    const auto pressed_buttons = source.pressed_buttons;
    for (const uint16_t code : pressed_buttons)
        update_mouse_button(source, code, false, button_press_count, modifiers);
}

void resync_device(device &source, std::unordered_map<uint16_t, unsigned int> &key_press_count,
                   std::unordered_map<uint16_t, unsigned int> &button_press_count, uint16_t &modifiers)
{
    std::array<unsigned long, (KEY_MAX / bits_per_word) + 1> keys{};
    if (ioctl(source.fd, EVIOCGKEY(sizeof(keys)), keys.data()) < 0)
        return;

    for (unsigned int code = 0; code <= KEY_MAX; ++code) {
        const auto input_code = static_cast<uint16_t>(code);
        if (linux_to_vcode(input_code) == VC_UNDEFINED && linux_to_mouse_button(input_code) == MOUSE_NOBUTTON)
            continue;
        const bool pressed = bit_is_set(keys, code);
        update_key(source, input_code, pressed, false, key_press_count, modifiers);
        update_mouse_button(source, input_code, pressed, button_press_count, modifiers);
    }
}

void dispatch_relative_frame(device &source, uint16_t modifiers)
{
    send_mouse_motion_event(source.relative_x, source.relative_y, modifiers);

    const int vertical_rotation = source.high_resolution_wheel != 0 ? source.high_resolution_wheel : source.wheel * 120;
    const int horizontal_rotation = source.high_resolution_horizontal_wheel != 0
                                        ? source.high_resolution_horizontal_wheel
                                        : source.horizontal_wheel * 120;
    send_mouse_wheel_event(vertical_rotation, WHEEL_VERTICAL_DIRECTION, modifiers);
    send_mouse_wheel_event(horizontal_rotation, WHEEL_HORIZONTAL_DIRECTION, modifiers);
    clear_relative_frame(source);
}

void update_relative_axis(device &source, uint16_t code, int value)
{
    switch (code) {
    case REL_X:
        source.relative_x += value;
        break;
    case REL_Y:
        source.relative_y += value;
        break;
    case REL_WHEEL:
        source.wheel += value;
        break;
    case REL_HWHEEL:
        source.horizontal_wheel += value;
        break;
    case REL_WHEEL_HI_RES:
        source.high_resolution_wheel += value;
        break;
    case REL_HWHEEL_HI_RES:
        source.high_resolution_horizontal_wheel += value;
        break;
    default:;
    }
}

bool read_device(device &source, std::unordered_map<uint16_t, unsigned int> &key_press_count,
                 std::unordered_map<uint16_t, unsigned int> &button_press_count, uint16_t &modifiers)
{
    input_event events[32];
    for (;;) {
        const ssize_t bytes = read(source.fd, events, sizeof(events));
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EINTR)
                return true;
            return false;
        }
        if (bytes == 0)
            return false;

        const std::size_t count = static_cast<std::size_t>(bytes) / sizeof(input_event);
        for (std::size_t i = 0; i < count; ++i) {
            const auto &event = events[i];
            if (event.type == EV_SYN && event.code == SYN_DROPPED) {
                source.dropped = true;
                clear_relative_frame(source);
                continue;
            }
            if (source.dropped) {
                if (event.type == EV_SYN && event.code == SYN_REPORT) {
                    source.dropped = false;
                    resync_device(source, key_press_count, button_press_count, modifiers);
                }
                continue;
            }
            if (event.type == EV_KEY) {
                update_key(source, event.code, event.value != 0, event.value == 2, key_press_count, modifiers);
                if (event.value != 2)
                    update_mouse_button(source, event.code, event.value != 0, button_press_count, modifiers);
            } else if (event.type == EV_REL && is_relative_event(event.code)) {
                update_relative_axis(source, event.code, event.value);
            } else if (event.type == EV_SYN && event.code == SYN_REPORT) {
                dispatch_relative_frame(source, modifiers);
            }
        }
    }
}

void run(std::vector<device> devices)
{
    std::unordered_map<uint16_t, unsigned int> key_press_count;
    std::unordered_map<uint16_t, unsigned int> button_press_count;
    uint16_t modifiers{};
    send_hook_event(EVENT_HOOK_ENABLED);

    auto next_scan = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (running) {
        std::vector<pollfd> poll_fds;
        poll_fds.reserve(devices.size());
        for (const auto &item : devices)
            poll_fds.push_back({item.fd, POLLIN, 0});

        const int result = poll(poll_fds.data(), poll_fds.size(), 500);
        if (result > 0) {
            for (std::size_t index = devices.size(); index-- > 0;) {
                const short events = poll_fds[index].revents;
                if (events == 0)
                    continue;
                if ((events & POLLIN) != 0 &&
                    read_device(devices[index], key_press_count, button_press_count, modifiers))
                    continue;

                release_device(devices[index], key_press_count, button_press_count, modifiers);
                close(devices[index].fd);
                devices.erase(devices.begin() + static_cast<std::ptrdiff_t>(index));
            }
        }

        if (std::chrono::steady_clock::now() >= next_scan) {
            std::size_t ignored{};
            scan_devices(devices, ignored);
            next_scan = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        }
    }

    for (auto &item : devices) {
        release_device(item, key_press_count, button_press_count, modifiers);
        close(item.fd);
    }
    send_hook_event(EVENT_HOOK_DISABLED);
}

} // namespace

start_result start(dispatcher_t dispatch)
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    if (running)
        return {};

    std::vector<device> devices;
    std::size_t permission_denied{};
    scan_devices(devices, permission_denied);
    const start_result result{devices.size(), permission_denied};
    if (devices.empty())
        return result;

    dispatch_event = dispatch;
    running = true;
    input_thread = std::thread(run, std::move(devices));
    return result;
}

void stop()
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    if (!running)
        return;
    running = false;
    if (input_thread.joinable())
        input_thread.join();
    dispatch_event = nullptr;
}

bool is_running()
{
    return running;
}

} // namespace evdev_input
