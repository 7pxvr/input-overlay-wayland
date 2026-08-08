![logo](./docs/io.png)

Show keyboard, mouse and gamepad input on stream.\
This fork adds global keyboard and mouse capture for native OBS Studio on Linux Wayland.

## Wayland input

Wayland does not expose global keyboard or pointer events to ordinary applications. On Linux Wayland sessions,
input-overlay reads physical input events from the kernel evdev interface instead. The OBS process must have read access to
`/dev/input/event*`; many distributions provide this through the `input` group. Group membership grants access to all
local input events, so only grant it to trusted users. Log out and back in after changing membership.

Sandboxed OBS packages may not expose `/dev/input` even when the host user has permission. In that case, run the
`io_client` executable on the host with `--keyboard` and forward its events to input-overlay's WebSocket server.

## Installation

The release is built for 64-bit Linux and native OBS Studio. Do not keep another input-overlay installation enabled:
OBS loads the first copy, rejects the second source registration, and keyboard state is sent to the wrong plugin copy.

### 1. Remove an existing package

On Arch Linux, identify and remove the package that owns the system plugin:

```bash
pacman -Qo /usr/lib/obs-plugins/input-overlay.so
sudo pacman -Rns obs-plugin-input-overlay-bin
```

Use the package name printed by `pacman -Qo` if it differs. Also remove any old manual copy:

```bash
rm -rf ~/.config/obs-studio/plugins/input-overlay
```

### 2. Install this release

```bash
curl -L \
  https://github.com/7pxvr/input-overlay-wayland/releases/latest/download/input-overlay-wayland-linux-x86_64.tar.gz \
  -o /tmp/input-overlay-wayland.tar.gz
mkdir -p ~/.config/obs-studio/plugins
tar -xzf /tmp/input-overlay-wayland.tar.gz -C ~/.config/obs-studio/plugins
```

### 3. Grant keyboard access

Check whether the current user can read an input event device:

```bash
test -r /dev/input/event0 && echo ready || echo permission-denied
```

Many distributions grant access through the `input` group:

```bash
sudo usermod -aG input "$USER"
```

Log out and back in after changing groups. Membership in `input` exposes all local keyboard events, so grant it only to
trusted users.

### 4. Verify

Restart OBS and check its log:

```bash
obs 2>&1 | grep -i input-overlay
```

Exactly one plugin version should load. A working Wayland hook reports:

```text
Wayland evdev input hook started with ... device(s).
```

If OBS reports `Source 'input-overlay' already exists! Duplicate library?`, another plugin copy remains in
`/usr/lib/obs-plugins`, `/usr/local/lib/obs-plugins`, or `~/.config/obs-studio/plugins`.

### Build from source

```bash
git clone --recurse-submodules https://github.com/7pxvr/input-overlay-wayland.git
cd input-overlay-wayland
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## [Wiki](https://github.com/univrsal/input-overlay/wiki)

## Credits

input-overlay depends on [libuiohook](https://github.com/kwhat/libuiohook) by [kwhat](https://github.com/kwhat) licensed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.txt), [mongoose](https://github.com/cesanta/mongoose) licensed under the [GNU General Public License v2.0](https://www.gnu.org/licenses/gpl-2.0.txt), [SDL](https://libsdl.org) licensed under the [zlib license](https://www.zlib.net/zlib_license.html).

## More Information:
- [OBS resource page](https://obsproject.com/forum/resources/input-overlay.552/)
- [Config creation tool](https://univrsal.github.io/input-overlay/cct/)
- [Convert old *.ini presets to JSON (WIP)](https://univrsal.github.io/input-overlay/converter/)
