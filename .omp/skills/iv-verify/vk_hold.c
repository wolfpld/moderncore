/* vk_hold.c — feed stdin commands to a virtual keyboard.
 *
 * Build:
 *   wayland-scanner client-header virtual-keyboard-unstable-v1.xml zwlr_virtual_keyboard_manager_v1_wlr.h
 *   wayland-scanner private-code  virtual-keyboard-unstable-v1.xml zwlr_virtual_keyboard_manager_v1_wlr.c
 *   clang vk_hold.c zwlr_virtual_keyboard_manager_v1_wlr.c -o vk_hold -lwayland-client -lxkbcommon
 *
 * The protocol XML must be the compositor's (wlroots 0.20 / sway 1.12).
 *
 * Use:
 *   : > /tmp/iv-run/vk_cmd
 *   nohup tail -f /tmp/iv-run/vk_cmd | ./vk_hold > /tmp/iv-run/vk_hold.log 2>&1 &
 *   printf 'k 106 1\nk 106 0\n' >> /tmp/iv-run/vk_cmd   # Right (evdev 106)
 *
 * Commands:  k <code> <state>   key event (state 0/1)   ·   m <d> <l> <k> <g>   modifiers
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/syscall.h>
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 1
#endif
#ifndef SYS_memfd_create
#define SYS_memfd_create 319
#endif
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include "zwlr_virtual_keyboard_manager_v1_wlr.h"

static struct wl_display *dpy;
static struct zwp_virtual_keyboard_manager_v1 *mgr;
static struct wl_seat *seat;
static struct zwp_virtual_keyboard_v1 *kbd;
static uint32_t time_ms = 0;

static void reg_global(void *data, struct wl_registry *reg, uint32_t name,
                       const char *iface, uint32_t version) {
    (void)data;
    if (!strcmp(iface, zwp_virtual_keyboard_manager_v1_interface.name))
        mgr = wl_registry_bind(reg, name, &zwp_virtual_keyboard_manager_v1_interface, version);
    else if (!strcmp(iface, wl_seat_interface.name))
        seat = wl_registry_bind(reg, name, &wl_seat_interface, version);
}
static void reg_remove(void *data, struct wl_registry *reg, uint32_t name) {
    (void)data; (void)reg; (void)name;
}
static const struct wl_registry_listener reg_listener = { reg_global, reg_remove };

static void send_keymap(void) {
    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *km = xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!km) { fprintf(stderr, "keymap failed\n"); exit(1); }
    char *s = xkb_keymap_get_as_string(km, XKB_KEYMAP_FORMAT_TEXT_V1);
    size_t len = s ? strlen(s) : 0;
    int fd = syscall(SYS_memfd_create, "vk-keymap", MFD_CLOEXEC);
    if (fd < 0) { perror("memfd_create"); exit(1); }
    if (write(fd, s, len) != (ssize_t)len) { perror("write"); exit(1); }
    lseek(fd, 0, SEEK_SET);
    zwp_virtual_keyboard_v1_keymap(kbd, 1, fd, len);   /* 1 = XKB_V1 */
    close(fd);
    zwp_virtual_keyboard_v1_modifiers(kbd, 0, 0, 0, 0);
    xkb_keymap_unref(km); xkb_context_unref(ctx); free(s);
    wl_display_flush(dpy);
    fprintf(stderr, "keymap sent (%zu bytes)\n", len); fflush(stderr);
}

static void handle_stdin(void) {
    char buf[512];
    ssize_t n = read(STDIN_FILENO, buf, sizeof buf - 1);
    if (n <= 0) return;
    buf[n] = 0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (!strncmp(line, "k ", 2)) {
            uint32_t code, state;
            if (sscanf(line + 2, "%u %u", &code, &state) == 2)
                zwp_virtual_keyboard_v1_key(kbd, time_ms++, code, state);
        } else if (!strncmp(line, "m ", 2)) {
            uint32_t d, l, k, g;
            if (sscanf(line + 2, "%u %u %u %u", &d, &l, &k, &g) == 4)
                zwp_virtual_keyboard_v1_modifiers(kbd, d, l, k, g);
        }
    }
    wl_display_flush(dpy);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    dpy = wl_display_connect(NULL);
    if (!dpy) { fprintf(stderr, "cannot connect\n"); return 1; }
    struct wl_registry *reg = wl_display_get_registry(dpy);
    wl_registry_add_listener(reg, &reg_listener, NULL);
    wl_display_roundtrip(dpy);
    if (!mgr || !seat) { fprintf(stderr, "missing manager/seat\n"); return 2; }
    kbd = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(mgr, seat);
    wl_display_roundtrip(dpy);
    send_keymap();
    fprintf(stderr, "ready\n"); fflush(stderr);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    for (;;) {
        struct pollfd pfd[2] = { { .fd = wl_display_get_fd(dpy), .events = POLLIN },
                                 { .fd = STDIN_FILENO, .events = POLLIN } };
        if (poll(pfd, 2, -1) > 0) {
            if (pfd[0].revents & POLLIN) wl_display_dispatch(dpy);
            if (pfd[1].revents & POLLIN) handle_stdin();
        }
    }
}
