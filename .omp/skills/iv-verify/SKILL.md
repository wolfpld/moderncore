---
name: iv-verify
description: Verify the iv image viewer — headless sway + lavapipe, keyboard navigation via a virtual keyboard, grim screenshots.
---

# iv-verify

Verify the `iv` image viewer (Wayland + Vulkan client in this repo) without a
display and GPU: build it, run it on headless sway with software Vulkan
(lavapipe), drive keyboard navigation, and confirm the render with `grim`
screenshots.

## Packages

```sh
pacman -S sway grim vulkan-swrast python-pillow
```

- `sway` — headless compositor (pulls `wlroots0.20`)
- `grim` — screenshots
- `vulkan-swrast` — lavapipe, the software Vulkan driver (`lvp_icd.json`)
- `python-pillow` — screenshot comparison

## Build

Initial setup (once — creates `build/`):

```sh
cmake --preset debug
```

Then build the `iv` target:

```sh
ninja -C build iv
```

Rebuild after changes to source files. See INSTALL.md for other build
dependencies.

## Headless session

```sh
export WAYLAND_DISPLAY=wayland-1
mkdir -p /tmp/iv-run
echo 'output * mode "1280x720"' > /tmp/iv-run/sway-config
WLR_BACKENDS=headless WLR_RENDERER=pixman \
    sway --config /tmp/iv-run/sway-config > /tmp/iv-run/sway.log 2>&1 &
sleep 2
ls /run/user/$(id -u)/wayland-1    # socket must exist
```

Software Vulkan via lavapipe:

```sh
export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json
```

A wrong path makes iv die at startup with `Required Vulkan instance extension
'VK_KHR_surface' is not available`.

Run iv:

```sh
WAYLAND_DISPLAY=wayland-1 VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json \
    /home/wolf/moderncore/build/iv /home/wolf/moderncore/doc/screenshots/vv1.png \
    > /tmp/iv-run/iv.log 2>&1 &
sleep 4
pgrep -a iv
```

Success markers in `iv.log`: `Selected GPU: llvmpipe` and `Image loaded: 640x432`.

## Screenshot

```sh
WAYLAND_DISPLAY=wayland-1 grim /tmp/iv-run/shot.png
```

`vv1.png` renders as mean (46.4,43.0,43.4) / 96201 distinct colors. A changed
mean/distinct means the render changed.

## Keyboard navigation

iv navigates with Right (evdev keycode 106) and Left (105). The virtual
keyboard client is `vk_hold.c` in this directory — see its header comment
for the build. Generate the protocol code from the installed compositor's
own XML — the wlroots version `sway` is linked against
(`ldd $(which sway) | grep wlroots`), `protocol/virtual-keyboard-unstable-v1.xml`.

Run vk_hold as a **long-lived managed process** (e.g. a `hub` daemon),
started **before iv**, with `WAYLAND_DISPLAY=wayland-1`:

```
application: bash   args: ["-c", "tail -f /tmp/iv-run/vk_cmd | /tmp/iv-run/vk_hold"]
ready log: "ready"  (printed once the keymap is sent)
```

A plain `nohup tail -f … | ./vk_hold &` from a short-lived shell does NOT
survive: `nohup`/`setsid` only detach the *first* element of the pipeline,
the second dies with the shell, and every keypress is then silently dropped
— check `pgrep -a vk_hold` before trusting an injection. After a sway
restart the old daemon is still attached to the dead display: restart it.

Inject:

```sh
printf 'k 106 1\nk 106 0\n' >> /tmp/iv-run/vk_cmd   # Right
```

## Sample workflow

1. `ninja -C build iv`
2. sway up (socket check) + lavapipe env
3. start vk_hold (managed process, before iv); confirm it printed `ready`
   and is alive (`pgrep vk_hold`)
4. run iv; confirm `Selected GPU: llvmpipe`, `Image loaded`, alive (`pgrep`)
5. screenshot baseline; inject Right; screenshot again — image must change and
   `iv.log` must show the new image load
6. inject Left — must navigate back
7. confirm still alive; `iv.log` clean

## Gotchas

- **Instrumenting the compositor.** `gdb`/`strace` attach may be denied
  (`ptrace: Operation not permitted`) in sandboxed environments. Fallback:
  wrap a wlroots function with `LD_PRELOAD`, e.g. `wlr_seat_set_capabilities`
  to log the caps value on every change. Note: if the sway binary carries
  file capabilities (`getcap /usr/bin/sway`), glibc runs it in
  secure-execution mode and silently drops `LD_PRELOAD` — copy the binary
  first; the copy does not keep the fcap.
- **sway flags:** full logging is `-d` (`--debug`). The `Cannot find Xwayland
  binary` and `swaybg` errors at startup are harmless in a minimal install.
