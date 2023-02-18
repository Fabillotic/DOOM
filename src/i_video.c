//-----------------------------------------------------------------------------
//
// Copyright (C) 2022 by Fabillotic
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include "xdg-shell.h"
#include "xdg-decoration.h"
#include "pointer-constraints.h"
#include "relative-pointer.h"

#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_video.h"
#include "m_argv.h"
#include "v_video.h"
#include "m_menu.h"

#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112

struct wl_display *display;

struct wl_compositor *compositor = NULL;
struct wl_shm *shm = NULL;
struct wl_seat *seat = NULL;
struct xdg_wm_base *wm_base = NULL;
struct zxdg_decoration_manager_v1 *deco_manager = NULL;
struct zwp_relative_pointer_manager_v1 *relative_pointer_manager = NULL;
struct zwp_pointer_constraints_v1 *pointer_constraints = NULL;

struct wl_surface *surface = NULL;
struct xdg_surface *window = NULL;
struct wl_pointer *pointer;
struct wl_keyboard *keyboard;
struct zwp_locked_pointer_v1 *locked_pointer = NULL;

struct xkb_context *xkb_context;
struct xkb_keymap *xkb_keymap;
struct xkb_state *xkb_state;

struct wl_registry_listener registry_listener;
struct wl_pointer_listener pointer_listener;
struct wl_keyboard_listener keyboard_listener;
struct xdg_surface_listener window_listener;
struct xdg_wm_base_listener wm_base_listener;
struct wl_buffer_listener buffer_listener;
struct zxdg_toplevel_decoration_v1_listener decoration_listener;
struct xdg_toplevel_listener toplevel_listener;
struct zwp_relative_pointer_v1_listener relative_pointer_listener;
struct wl_callback_listener frame_listener;

unsigned char *palette;

int first_frame = 0;
int offset = 0;

uint32_t last_frame = UINT32_MAX;

int last_mouse_grabbed;
int mouse_grabbed;

int scale;
int wwidth;
int wheight;

int fullscreen;

#ifdef JOYTEST
int joytest[8];
#define JOY_FORWARD 't'
#define JOY_LEFT 'f'
#define JOY_BACK 'g'
#define JOY_RIGHT 'h'
#define JOY_FIRE 'c'
#define JOY_STRAFE 'v'
#define JOY_USE 'b'
#define JOY_SPEED 'n'
#endif

int buttons[3];

void* start_display(void *args);
void init_listeners();
void draw();
void grab_mouse();
void release_mouse();
void create_empty_cursor();
int in_menu();
void screencoords(int *dx, int *dy, int *dw, int *dh);
void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name);
void pointer_handle_enter(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y);
void pointer_handle_leave(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s);
void pointer_handle_motion(void *data, struct wl_pointer *p, uint32_t time, wl_fixed_t x, wl_fixed_t y);
void pointer_handle_button(void *data, struct wl_pointer *p, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
void pointer_handle_axis(void *data, struct wl_pointer *p, uint32_t time, uint32_t axis, wl_fixed_t value);
void pointer_handle_frame(void *data, struct wl_pointer *p);
void pointer_handle_axis_source(void *data, struct wl_pointer *p, uint32_t axis_source);
void pointer_handle_axis_stop(void *data, struct wl_pointer *p, uint32_t time, uint32_t axis);
void pointer_handle_axis_discrete(void *data, struct wl_pointer *p, uint32_t axis, int32_t discrete);
void pointer_handle_axis_value120(void *data, struct wl_pointer *p, uint32_t axis, int32_t value120);
void keyboard_handle_keymap(void *data, struct wl_keyboard *k, uint32_t format, int32_t fd, uint32_t size);
void keyboard_handle_enter(void *data, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *keys);
void keyboard_handle_leave(void *data, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s);
void keyboard_handle_key(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
void keyboard_handle_modifiers(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
void keyboard_handle_repeat_info(void *data, struct wl_keyboard *k, int32_t rate, int32_t delay);
void buffer_handle_release(void *data, struct wl_buffer *buffer);
void surface_handle_configure(void *d, struct xdg_surface *s, uint32_t serial);
void wm_handle_ping(void *data, struct xdg_wm_base *b, uint32_t serial);
void decoration_configure(void *data, struct zxdg_toplevel_decoration_v1 *deco, uint32_t mode);
void toplevel_handle_configure(void *data, struct xdg_toplevel *t, int32_t w, int32_t h, struct wl_array *states);
void toplevel_handle_bounds(void *data, struct xdg_toplevel *t, int32_t w, int32_t h);
void toplevel_handle_wm_caps(void *data, struct xdg_toplevel *t, struct wl_array *caps);
void toplevel_handle_close(void *data, struct xdg_toplevel *t);
void surface_handle_frame(void *data, struct wl_callback *cb, uint32_t time);
void relative_pointer_handle_motion(void *data, struct zwp_relative_pointer_v1 *p, uint32_t utime_hi, uint32_t utime_lo, wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel);
int doo_display_poll(struct wl_display *display, short int events, int timeout);
int doo_display_dispatch(struct wl_display *display, int read_timeout);
void randname(char *buf);
int create_shm_file();
int allocate_shm_file(size_t size);
int xlatekey(xkb_keysym_t sym);

void I_InitGraphics() {
	scale = 1;
	if(M_CheckParm("-2")) scale = 2;
	if(M_CheckParm("-3")) scale = 3;
	if(M_CheckParm("-4")) scale = 4;

	fullscreen = M_CheckParm("-fullscreen");

	wwidth = SCREENWIDTH * scale;
	wheight = SCREENHEIGHT * scale;

	mouse_grabbed = 0;
	last_mouse_grabbed = 0;

#ifdef JOYTEST
	memset(joytest, 0, 8 * sizeof(int));
#endif

	memset(buttons, 0, 3 * sizeof(int));

	printf("Allocating screen buffer, image buffer and palette.\n");
	screens[0] = (unsigned char *) malloc(
	    SCREENWIDTH * SCREENHEIGHT); // Color index, 8-bit per pixel
	palette = malloc(256 * 3);       // 256 entries, each of them 24-bit

	printf("Starting wayland thread...\n");

	pthread_t thread;
	pthread_create(&thread, NULL, start_display, NULL);

	printf("Thread started!\n");
}

void* start_display(void *args) {
	struct wl_registry *registry;
	struct xdg_toplevel *toplevel;
	struct zxdg_toplevel_decoration_v1 *decoration;
	struct zwp_relative_pointer_v1 *relative_pointer;
	struct wl_callback *callback;

	init_listeners();

	display = wl_display_connect(NULL);
	if(!display) {
		printf("Couldn't connect to display!\n");
		return NULL;
	}
	printf("Got display!\n");

	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);
	
	if(!compositor) {
		printf("Didn't receive compositor!\n");
		return NULL;
	}

	if(!seat) {
		printf("Didn't receive seat!\n");
		return NULL;
	}

	if(!shm) {
		printf("Didn't receive shm!\n");
		return NULL;
	}

	if(!wm_base) {
		printf("Didn't receive wm_base!\n");
		return NULL;
	}

	if(!relative_pointer_manager) {
		printf("Didn't receive relative_pointer_manager!\n");
		return NULL;
	}

	if(!pointer_constraints) {
		printf("Didn't received pointer_constraints!\n");
		return NULL;
	}

	if(!deco_manager) {
		printf("Didn't receive decoration manager! No big issue, we just won't have decorations.\n");
	}

	printf("adding listener...\n");
	xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);

	surface = wl_compositor_create_surface(compositor);
	printf("surface: %p\n", surface);
	callback = wl_surface_frame(surface);
	wl_callback_add_listener(callback, &frame_listener, NULL);

	pointer = wl_seat_get_pointer(seat);
	wl_pointer_add_listener(pointer, &pointer_listener, NULL);

	relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(relative_pointer_manager, pointer);
	zwp_relative_pointer_v1_add_listener(relative_pointer, &relative_pointer_listener, NULL);

	keyboard = wl_seat_get_keyboard(seat);
	wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);

	xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	window = xdg_wm_base_get_xdg_surface(wm_base, surface);
	printf("window: %p\n", window);
	toplevel = xdg_surface_get_toplevel(window);
	printf("toplevel: %p\n", toplevel);
	xdg_surface_add_listener(window, &window_listener, NULL);
	xdg_toplevel_set_title(toplevel, "DOOM");
	xdg_toplevel_set_app_id(toplevel, "doom");
	xdg_toplevel_add_listener(toplevel, &toplevel_listener, NULL);
	if(deco_manager) {
		decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(deco_manager, toplevel);
		zxdg_toplevel_decoration_v1_add_listener(decoration, &decoration_listener, NULL);
		zxdg_toplevel_decoration_v1_set_mode(decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	printf("committing...\n");
	wl_surface_commit(surface);

	printf("Finished initializing!\n");

	while(1) {
		if(mouse_grabbed && !last_mouse_grabbed) {
			locked_pointer = zwp_pointer_constraints_v1_lock_pointer(pointer_constraints, surface, pointer, NULL, 2);
		}
		if(!mouse_grabbed && last_mouse_grabbed) {
			zwp_locked_pointer_v1_destroy(locked_pointer);
		}
		last_mouse_grabbed = mouse_grabbed;
		doo_display_dispatch(display, 1);
	}

	return NULL;
}

void init_listeners() {
	registry_listener = (struct wl_registry_listener) {
		.global = registry_handle_global,
		.global_remove = registry_handle_global_remove
	};
	pointer_listener = (struct wl_pointer_listener) {
		.enter = pointer_handle_enter,
		.leave = pointer_handle_leave,
		.motion = pointer_handle_motion,
		.button = pointer_handle_button,
		.axis = pointer_handle_axis,
		.frame = pointer_handle_frame,
		.axis_source = pointer_handle_axis_source,
		.axis_stop = pointer_handle_axis_stop,
		.axis_discrete = pointer_handle_axis_discrete,
		.axis_value120 = pointer_handle_axis_value120
	};
	keyboard_listener = (struct wl_keyboard_listener) {
		.keymap = keyboard_handle_keymap, keyboard_handle_enter,
		.leave = keyboard_handle_leave,
		.key = keyboard_handle_key,
		.modifiers = keyboard_handle_modifiers,
		.repeat_info = keyboard_handle_repeat_info
	};
	window_listener = (struct xdg_surface_listener) {
		.configure = surface_handle_configure
	};
	wm_base_listener = (struct xdg_wm_base_listener) {
		.ping = wm_handle_ping
	};
	buffer_listener = (struct wl_buffer_listener) {
		.release = buffer_handle_release
	};
	decoration_listener = (struct zxdg_toplevel_decoration_v1_listener) {
		.configure = decoration_configure
	};
	toplevel_listener = (struct xdg_toplevel_listener) {
		.configure = toplevel_handle_configure,
		.configure_bounds = toplevel_handle_bounds,
		.wm_capabilities = toplevel_handle_wm_caps,
		.close = toplevel_handle_close
	};
	frame_listener = (struct wl_callback_listener) {
		.done = surface_handle_frame
	};
	relative_pointer_listener = (struct zwp_relative_pointer_v1_listener) {
		.relative_motion = relative_pointer_handle_motion
	};
}

void grab_mouse() {
	mouse_grabbed = 1;
}

void release_mouse() {
	mouse_grabbed = 0;
}

void I_ShutdownGraphics() {
}

void I_SetPalette(byte *pal) {
	int i;

	memcpy(palette, pal, 256 * 3);

	for(i = 0; i < 256 * 3; i++) {
		palette[i] = gammatable[usegamma][palette[i]];
	}
}

void I_UpdateNoBlit() {
}

void I_FinishUpdate() {
	//doo_display_dispatch(display, 1);
}

void draw() {
	int i, j, k, dw, dh, dx, dy, x, y, size, fd;
	unsigned char *data;
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;

	screencoords(&dx, &dy, &dw, &dh);

	size = wwidth * wheight * 4;

	if(size > 0) {
		fd = allocate_shm_file(size);

		data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if(data == MAP_FAILED) {
			printf("Map failed!\n");
			close(fd);
		}

		pool = wl_shm_create_pool(shm, fd, size);
		buffer = wl_shm_pool_create_buffer(pool, 0, wwidth, wheight, wwidth * 4, WL_SHM_FORMAT_ARGB8888);
		wl_shm_pool_destroy(pool);
		close(fd);

		for(i = 0; i < wwidth * wheight; i++) {
			j = i % wwidth;
			k = i / wwidth;

			data[i * 4 + 3] = 255;

			/* Black bars */
			if(j < dx || j > dx + dw || k < dy || k > dy + dh) {
				data[i * 4 + 0] = 0;
				data[i * 4 + 1] = 0;
				data[i * 4 + 2] = 0;
			}
			else {
				x = (int) (((float) (j - dx)) / dw * SCREENWIDTH);
				y = (int) (((float) (k - dy)) / dh * SCREENHEIGHT);

				if(x == SCREENWIDTH || y == SCREENHEIGHT) continue;

				data[i * 4 + 0] =
				    palette[((int) screens[0][x + SCREENWIDTH * y]) * 3 + 2];
				data[i * 4 + 1] =
				    palette[((int) screens[0][x + SCREENWIDTH * y]) * 3 + 1];
				data[i * 4 + 2] =
				    palette[((int) screens[0][x + SCREENWIDTH * y]) * 3 + 0];
			}
		}

		munmap(data, size);
		wl_buffer_add_listener(buffer, &buffer_listener, NULL);
		wl_surface_attach(surface, buffer, 0, 0);
		wl_surface_damage(surface, 0, 0, wwidth, wheight);
		wl_surface_commit(surface);
	}
}

void I_ReadScreen(byte *scr) {
	printf("Reading screen...\n");
	memcpy(scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

void I_StartTic() {
	if(mouse_grabbed && in_menu()) release_mouse();
	if(!mouse_grabbed && !in_menu()) grab_mouse();
}

void I_StartFrame() {
}

int in_menu() {
	return menuactive || gamestate != GS_LEVEL;
}

void screencoords(int *dx, int *dy, int *dw, int *dh) {
	int x, y, w, h, vert;

	vert = (float) wwidth / wheight < (float) SCREENWIDTH / SCREENHEIGHT;

	if(!vert) {
		h = wheight;
		w = (int) (((float) h / SCREENHEIGHT) * SCREENWIDTH);
		y = 0;
		x = (wwidth - w) / 2;
	}
	else {
		w = wwidth;
		h = (int) (((float) w / SCREENWIDTH) * SCREENHEIGHT);
		x = 0;
		y = (wheight - h) / 2;
	}

	*dx = x;
	*dy = y;
	*dw = w;
	*dh = h;
}

void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	printf("Interface: '%s', version: %d, name: %d\n", interface, version, name);

	if(!strcmp(interface, "wl_compositor")) {
		compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 5);
	}
	else if(!strcmp(interface, "wl_shm")) {
		shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	}
	else if(!strcmp(interface, "wl_seat")) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface, 8);
	}
	else if(!strcmp(interface, "xdg_wm_base")) {
		wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
	}
	else if(!strcmp(interface, "zxdg_decoration_manager_v1")) {
		deco_manager = wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1);
	}
	else if(!strcmp(interface, "zwp_relative_pointer_manager_v1")) {
		relative_pointer_manager = wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, 1);
	}
	else if(!strcmp(interface, "zwp_pointer_constraints_v1")) {
		pointer_constraints = wl_registry_bind(registry, name, &zwp_pointer_constraints_v1_interface, 1);
	}
}

void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
}

void pointer_handle_enter(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y) {
}

void pointer_handle_leave(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s) {
}

void pointer_handle_motion(void *data, struct wl_pointer *p, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
	int dx, dy, dw, dh, x2, y2;

	if(in_menu()) {
		screencoords(&dx, &dy, &dw, &dh);
		x2 = (int) (((float) (wl_fixed_to_int(x) - dx)) / dw * SCREENWIDTH);
		y2 = (int) (((float) (wl_fixed_to_int(y) - dy)) / dh * SCREENHEIGHT);
		M_SelectItemByPosition(x2, y2);
	}
}

void relative_pointer_handle_motion(void *data, struct zwp_relative_pointer_v1 *p, uint32_t utime_hi, uint32_t utime_lo, wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel) {
	event_t d_event;

	if(!in_menu()) {
		d_event.type = ev_mouse;

		d_event.data1 = buttons[0];
		d_event.data1 |= buttons[1] << 1;
		d_event.data1 |= buttons[2] << 2;
		//d_event.data2 = (wl_fixed_to_int(x) - wwidth / 2) << 1;
		//d_event.data3 = (wl_fixed_to_int(y) - wheight / 2) << 1;
		d_event.data2 = (int) (wl_fixed_to_double(dx) * 20.0);
		d_event.data3 = (int) (wl_fixed_to_double(dy) * 20.0);

		if(d_event.data2 || d_event.data3) {
			D_PostEvent(&d_event);
		}
	}
}

void pointer_handle_button(void *data, struct wl_pointer *p, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	event_t d_event;

	d_event.type = ev_mouse;

	if(button == BTN_LEFT) {
		buttons[0] = state;
	}
	else if(button == BTN_RIGHT) {
		buttons[1] = state;
	}
	else if(button == BTN_MIDDLE) {
		buttons[2] = state;
	}

	d_event.data1 = buttons[0];
	d_event.data1 |= buttons[1] << 1;
	d_event.data1 |= buttons[2] << 2;
	d_event.data2 = 0;
	d_event.data3 = 0;

	D_PostEvent(&d_event);
}

void pointer_handle_axis(void *data, struct wl_pointer *p, uint32_t time, uint32_t axis, wl_fixed_t value) {
}

void pointer_handle_frame(void *data, struct wl_pointer *p) {
}

void pointer_handle_axis_source(void *data, struct wl_pointer *p, uint32_t axis_source) {
}

void pointer_handle_axis_stop(void *data, struct wl_pointer *p, uint32_t time, uint32_t axis) {
}

void pointer_handle_axis_discrete(void *data, struct wl_pointer *p, uint32_t axis, int32_t discrete) {
}

void pointer_handle_axis_value120(void *data, struct wl_pointer *p, uint32_t axis, int32_t value120) {
}

void keyboard_handle_keymap(void *data, struct wl_keyboard *k, uint32_t format, int32_t fd, uint32_t size) {
	char *map_data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

	printf("keymap!\n");
	xkb_keymap = xkb_keymap_new_from_string(xkb_context, map_data, format, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map_data, size);
	close(fd);

	if(xkb_state) xkb_state_unref(xkb_state);
	xkb_state = xkb_state_new(xkb_keymap);
}

void keyboard_handle_enter(void *data, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *keys) {
}

void keyboard_handle_leave(void *data, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s) {
}

void keyboard_handle_key(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
	int code = key + 8;
	char name[64];
	event_t d_event;
	xkb_keysym_t sym = xkb_state_key_get_one_sym(xkb_state, code);
	sym = xkb_keysym_to_lower(sym);

	xkb_keysym_get_name(sym, name, sizeof(name));

	printf("key! code: %d, state: %d, sym: %d, name: '%s'\n", code, state, sym, name);
	
	if(state) {
		d_event.type = ev_keydown;
		d_event.data1 = xlatekey(sym);
		D_PostEvent(&d_event);

#ifdef JOYTEST
		if(d_event.data1 == JOY_FORWARD) joytest[0] = 1;
		else if(d_event.data1 == JOY_LEFT) joytest[1] = 1;
		else if(d_event.data1 == JOY_BACK) joytest[2] = 1;
		else if(d_event.data1 == JOY_RIGHT) joytest[3] = 1;
		else if(d_event.data1 == JOY_FIRE) joytest[4] = 1;
		else if(d_event.data1 == JOY_STRAFE) joytest[5] = 1;
		else if(d_event.data1 == JOY_USE) joytest[6] = 1;
		else if(d_event.data1 == JOY_SPEED) joytest[7] = 1;

		d_event.type = ev_joystick;
		d_event.data1 = joytest[4] | (joytest[5] << 1) | (joytest[6] << 2) |
				(joytest[7] << 3);
		d_event.data2 =
		    (joytest[1] ^ joytest[3]) ? (joytest[3] * 2 - 1) : 0;
		d_event.data3 =
		    (joytest[0] ^ joytest[2]) ? (joytest[2] * 2 - 1) : 0;
		printf("joystick! buttons: %d, x-axis: %d, y-axis: %d\n",
		    d_event.data1, d_event.data2, d_event.data3);
		D_PostEvent(&d_event);
#endif
	}
	else {
		d_event.type = ev_keyup;
		d_event.data1 = xlatekey(sym);
		D_PostEvent(&d_event);

#ifdef JOYTEST
		if(d_event.data1 == JOY_FORWARD) joytest[0] = 0;
		else if(d_event.data1 == JOY_LEFT) joytest[1] = 0;
		else if(d_event.data1 == JOY_BACK) joytest[2] = 0;
		else if(d_event.data1 == JOY_RIGHT) joytest[3] = 0;
		else if(d_event.data1 == JOY_FIRE) joytest[4] = 0;
		else if(d_event.data1 == JOY_STRAFE) joytest[5] = 0;
		else if(d_event.data1 == JOY_USE) joytest[6] = 0;
		else if(d_event.data1 == JOY_SPEED) joytest[7] = 0;

		d_event.type = ev_joystick;
		d_event.data1 = joytest[4] | (joytest[5] << 1) | (joytest[6] << 2) |
				(joytest[7] << 3);
		d_event.data2 =
		    (joytest[1] ^ joytest[3]) ? (joytest[3] * 2 - 1) : 0;
		d_event.data3 =
		    (joytest[0] ^ joytest[2]) ? (joytest[2] * 2 - 1) : 0;
		printf("joystick! buttons: %d, x-axis: %d, y-axis: %d\n",
		    d_event.data1, d_event.data2, d_event.data3);
		D_PostEvent(&d_event);
#endif
	}
}

void keyboard_handle_modifiers(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
	printf("modifiers!\n");
	xkb_state_update_mask(xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

void keyboard_handle_repeat_info(void *data, struct wl_keyboard *k, int32_t rate, int32_t delay) {
}

void buffer_handle_release(void *data, struct wl_buffer *buffer) {
	wl_buffer_destroy(buffer);
}

void surface_handle_configure(void *d, struct xdg_surface *s, uint32_t serial) {
	xdg_surface_ack_configure(s, serial);

	if(!first_frame) {
		draw();
		//doo_display_dispatch(display, 1);
	}
	first_frame = 1;
}

void wm_handle_ping(void *data, struct xdg_wm_base *b, uint32_t serial) {
	printf("PING!\n");
	xdg_wm_base_pong(b, serial);
}

void decoration_configure(void *data, struct zxdg_toplevel_decoration_v1 *deco, uint32_t mode) {
	printf("Decoration configure! Mode: %d\n", mode);
}

void toplevel_handle_configure(void *data, struct xdg_toplevel *t, int32_t w, int32_t h, struct wl_array *states) {
	if(w > 0 && h > 0) {
		wwidth = w;
		wheight = h;
	}
}

void toplevel_handle_bounds(void *data, struct xdg_toplevel *t, int32_t w, int32_t h) {
	printf("Bounds!\n");
}

void toplevel_handle_wm_caps(void *data, struct xdg_toplevel *t, struct wl_array *caps) {
	printf("Capabilities!\n");
}

void toplevel_handle_close(void *data, struct xdg_toplevel *t) {
	printf("Close!\n");
	exit(0);
}

void surface_handle_frame(void *data, struct wl_callback *cb, uint32_t time) {
	wl_callback_destroy(cb);

	cb = wl_surface_frame(surface);
	wl_callback_add_listener(cb, &frame_listener, NULL);

	if(last_frame == UINT32_MAX || time - last_frame > 10) {
		draw();
		//doo_display_dispatch(display, 1);
		last_frame = time;
	}
}

int doo_display_poll(struct wl_display *display, short int events, int timeout) {
	int ret;
	struct pollfd pfd[1];

	pfd[0].fd = wl_display_get_fd(display);
	pfd[0].events = events;
	do {
		ret = poll(pfd, 1, timeout);
	} while (ret == -1 && errno == EINTR);

	return ret;
}

int doo_display_dispatch(struct wl_display *display, int read_timeout) {
	int ret;

	if(wl_display_prepare_read(display) == -1)
		return wl_display_dispatch_pending(display);

	while(1) {
		ret = wl_display_flush(display);

		if(ret != -1 || errno != EAGAIN)
			break;

		if(doo_display_poll(display, POLLOUT, -1) == -1) {
			wl_display_cancel_read(display);
			return -1;
		}
	}

	/* Don't stop if flushing hits an EPIPE; continue so we can read any
	 * protocol error that may have triggered it. */
	if(ret < 0 && errno != EPIPE) {
		wl_display_cancel_read(display);
		return -1;
	}

	if(doo_display_poll(display, POLLIN, read_timeout) == -1) {
		wl_display_cancel_read(display);
		return -1;
	}

	if(wl_display_read_events(display) == -1)
		return -1;

	return wl_display_dispatch_pending(display);
}

void randname(char *buf) {
	struct timespec ts;
	int i;
	long r;

	clock_gettime(CLOCK_REALTIME, &ts);
	r = ts.tv_nsec;
	for(i = 0; i < 6; i++) {
		buf[i] = 'A' + (r & 15) + (r & 16) * 2;
		r >>= 5;
	}
}

int create_shm_file() {
	int fd, retries;

	retries = 100;
	do {
		char name[] = "/wl_shm-XXXXXX";
		randname(name + sizeof(name) - 7);
		--retries;
		fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if(fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while(retries > 0 && errno == EEXIST);
	return -1;
}

int allocate_shm_file(size_t size) {
	int fd, ret;

	fd = create_shm_file();
	if(fd < 0) return -1;

	do {
		ret = ftruncate(fd, size);
	} while(ret < 0 && errno == EINTR);
	if(ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}


int xlatekey(xkb_keysym_t sym) {
	int rc;

	switch(sym) {
	case XKB_KEY_Left: rc = KEY_LEFTARROW; break;
	case XKB_KEY_Right: rc = KEY_RIGHTARROW; break;
	case XKB_KEY_Down: rc = KEY_DOWNARROW; break;
	case XKB_KEY_Up: rc = KEY_UPARROW; break;
	case XKB_KEY_Escape: rc = KEY_ESCAPE; break;
	case XKB_KEY_Return: rc = KEY_ENTER; break;
	case XKB_KEY_Tab: rc = KEY_TAB; break;
	case XKB_KEY_F1: rc = KEY_F1; break;
	case XKB_KEY_F2: rc = KEY_F2; break;
	case XKB_KEY_F3: rc = KEY_F3; break;
	case XKB_KEY_F4: rc = KEY_F4; break;
	case XKB_KEY_F5: rc = KEY_F5; break;
	case XKB_KEY_F6: rc = KEY_F6; break;
	case XKB_KEY_F7: rc = KEY_F7; break;
	case XKB_KEY_F8: rc = KEY_F8; break;
	case XKB_KEY_F9: rc = KEY_F9; break;
	case XKB_KEY_F10: rc = KEY_F10; break;
	case XKB_KEY_F11: rc = KEY_F11; break;
	case XKB_KEY_F12: rc = KEY_F12; break;

	case XKB_KEY_BackSpace:
	case XKB_KEY_Delete: rc = KEY_BACKSPACE; break;

	case XKB_KEY_Pause: rc = KEY_PAUSE; break;

	case XKB_KEY_KP_Equal:
	case XKB_KEY_equal: rc = KEY_EQUALS; break;

	case XKB_KEY_KP_Subtract:
	case XKB_KEY_minus: rc = KEY_MINUS; break;

	case XKB_KEY_Shift_L:
	case XKB_KEY_Shift_R: rc = KEY_RSHIFT; break;

	case XKB_KEY_Control_L:
	case XKB_KEY_Control_R: rc = KEY_RCTRL; break;

	case XKB_KEY_Alt_L:
	case XKB_KEY_Meta_L:
	case XKB_KEY_Alt_R:
	case XKB_KEY_Meta_R: rc = KEY_RALT; break;
	
	default: rc = sym; break;
	}

	return rc;
}
