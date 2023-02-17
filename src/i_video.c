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
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include "xdg-shell.h"
#include "xdg-decoration.h"

#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_video.h"
#include "m_argv.h"
#include "v_video.h"
#include "m_menu.h"

struct wl_display *display;

int first_frame = 0;
int offset = 0;

uint32_t last_frame = UINT32_MAX;

struct wl_compositor *compositor = NULL;
struct wl_shm *shm = NULL;
struct xdg_wm_base *wm_base = NULL;
struct wl_surface *surface = NULL;
struct xdg_surface *window = NULL;
struct zxdg_decoration_manager_v1 *deco_manager = NULL;

struct wl_registry_listener reg_listen;
struct xdg_surface_listener surf_listen;
struct xdg_wm_base_listener wm_listen;
struct wl_buffer_listener buf_listen;
struct zxdg_toplevel_decoration_v1_listener deco_listen;
struct xdg_toplevel_listener top_listen;
struct wl_callback_listener frame_listen;

unsigned char *palette;

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

#define EVENTMASK (ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask | FocusChangeMask)
#define POINTERMASK (ButtonPressMask | ButtonReleaseMask | PointerMotionMask)

void draw();
void grab_mouse();
void release_mouse();
void create_empty_cursor();
int in_menu();
void screencoords(int *dx, int *dy, int *dw, int *dh);
//int xlatekey(KeySym sym);
void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name);
void buffer_handle_release(void *data, struct wl_buffer *buffer);
void surface_handle_configure(void *d, struct xdg_surface *s, uint32_t serial);
void wm_handle_ping(void *data, struct xdg_wm_base *b, uint32_t serial);
void decoration_configure(void *data, struct zxdg_toplevel_decoration_v1 *deco, uint32_t mode);
void toplevel_handle_configure(void *data, struct xdg_toplevel *t, int32_t w, int32_t h, struct wl_array *states);
void toplevel_handle_bounds(void *data, struct xdg_toplevel *t, int32_t w, int32_t h);
void toplevel_handle_wm_caps(void *data, struct xdg_toplevel *t, struct wl_array *caps);
void toplevel_handle_close(void *data, struct xdg_toplevel *t);
void surface_handle_frame(void *data, struct wl_callback *cb, uint32_t time);
int doo_display_poll(struct wl_display *display, short int events, int timeout);
int doo_display_dispatch(struct wl_display *display, int read_timeout);
void randname(char *buf);
int create_shm_file();
int allocate_shm_file(size_t size);

void I_InitGraphics() {
	struct wl_registry *registry;
	struct xdg_toplevel *toplevel;
	struct zxdg_toplevel_decoration_v1 *decoration;
	struct wl_callback *callback;

	reg_listen = (struct wl_registry_listener){.global = registry_handle_global, .global_remove = registry_handle_global_remove};
	surf_listen = (struct xdg_surface_listener){.configure = surface_handle_configure};
	wm_listen = (struct xdg_wm_base_listener){.ping = wm_handle_ping};
	buf_listen = (struct wl_buffer_listener){.release = buffer_handle_release};
	deco_listen = (struct zxdg_toplevel_decoration_v1_listener){.configure = decoration_configure};
	top_listen = (struct xdg_toplevel_listener){.configure = toplevel_handle_configure, .configure_bounds = toplevel_handle_bounds, .wm_capabilities = toplevel_handle_wm_caps, .close = toplevel_handle_close};
	frame_listen = (struct wl_callback_listener){.done = surface_handle_frame};

	scale = 1;
	if(M_CheckParm("-2")) scale = 2;
	if(M_CheckParm("-3")) scale = 3;
	if(M_CheckParm("-4")) scale = 4;

	fullscreen = M_CheckParm("-fullscreen");

	wwidth = SCREENWIDTH * scale;
	wheight = SCREENHEIGHT * scale;

	mouse_grabbed = 0;

#ifdef JOYTEST
	memset(joytest, 0, 8 * sizeof(int));
#endif

	printf("Allocating screen buffer, image buffer and palette.\n");
	screens[0] = (unsigned char *) malloc(
	    SCREENWIDTH * SCREENHEIGHT); // Color index, 8-bit per pixel
	palette = malloc(256 * 3);       // 256 entries, each of them 24-bit

	display = wl_display_connect(NULL);
	if(!display) {
		printf("Couldn't connect to display!\n");
		return;
	}
	printf("Got display!\n");

	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &reg_listen, NULL);
	wl_display_roundtrip(display);
	
	if(!compositor) {
		printf("Didn't receive compositor!\n");
		return;
	}

	if(!shm) {
		printf("Didn't receive shm!\n");
		return;
	}

	if(!wm_base) {
		printf("Didn't receive wm_base!\n");
		return;
	}

	if(!deco_manager) {
		printf("Didn't receive decoration manager! No big issue, we just won't have decorations.\n");
	}

	printf("adding listener...\n");
	xdg_wm_base_add_listener(wm_base, &wm_listen, NULL);

	surface = wl_compositor_create_surface(compositor);
	printf("surface: %p\n", surface);
	callback = wl_surface_frame(surface);
	wl_callback_add_listener(callback, &frame_listen, NULL);

	window = xdg_wm_base_get_xdg_surface(wm_base, surface);
	printf("window: %p\n", window);
	toplevel = xdg_surface_get_toplevel(window);
	printf("toplevel: %p\n", toplevel);
	xdg_surface_add_listener(window, &surf_listen, NULL);
	xdg_toplevel_set_title(toplevel, "DOOM");
	xdg_toplevel_set_app_id(toplevel, "doom");
	xdg_toplevel_add_listener(toplevel, &top_listen, NULL);
	if(deco_manager) {
		decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(deco_manager, toplevel);
		zxdg_toplevel_decoration_v1_add_listener(decoration, &deco_listen, NULL);
		zxdg_toplevel_decoration_v1_set_mode(decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	printf("committing...\n");
	wl_surface_commit(surface);

	wl_display_dispatch(display);

	printf("Finished initializing!\n");
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
	doo_display_dispatch(display, 1);
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
		wl_buffer_add_listener(buffer, &buf_listen, NULL);
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
	/*
	int x, y, dx, dy, dw, dh;
	XEvent ev;
	event_t d_event;
	char buf[256];
	KeySym sym;

	while(XPending(display)) {
		XNextEvent(display, &ev);

		if(ev.type == KeyPress) {
			d_event.type = ev_keydown;
			ev.xkey.state =
			    ev.xkey.state & (~(ControlMask | LockMask | ShiftMask));
			XLookupString(&ev.xkey, buf, 256, &sym, NULL);
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
		else if(ev.type == KeyRelease) {
			d_event.type = ev_keyup;
			ev.xkey.state =
			    ev.xkey.state & (~(ControlMask | LockMask | ShiftMask));
			XLookupString(&ev.xkey, buf, 256, &sym, NULL);
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
		else if(ev.type == ButtonPress) {
			d_event.type = ev_mouse;

			if(ev.xbutton.state & Button1Mask) d_event.data1 |= 1;
			if(ev.xbutton.state & Button2Mask) d_event.data1 |= 2;
			if(ev.xbutton.state & Button3Mask) d_event.data1 |= 4;

			if(ev.xbutton.button == Button1) d_event.data1 |= 1;
			if(ev.xbutton.button == Button2) d_event.data1 |= 2;
			if(ev.xbutton.button == Button3) d_event.data1 |= 4;

			d_event.data2 = 0;
			d_event.data3 = 0;

			D_PostEvent(&d_event);
		}
		else if(ev.type == ButtonRelease) {
			d_event.type = ev_mouse;

			d_event.data1 = ((ev.xbutton.state & Button1Mask) > 0);
			d_event.data1 |= ((ev.xbutton.state & Button2Mask) > 0) << 1;
			d_event.data1 |= ((ev.xbutton.state & Button3Mask) > 0) << 2;
			if(ev.xbutton.button == Button1) d_event.data1 &= ~(1 << 0);
			if(ev.xbutton.button == Button2) d_event.data1 &= ~(1 << 1);
			if(ev.xbutton.button == Button3) d_event.data1 &= ~(1 << 2);

			d_event.data2 = 0;
			d_event.data3 = 0;

			D_PostEvent(&d_event);
		}
		else if(ev.type == MotionNotify) {
			if(!in_menu()) {
				d_event.type = ev_mouse;

				d_event.data1 = ((ev.xmotion.state & Button1Mask) > 0);
				d_event.data1 |= ((ev.xmotion.state & Button2Mask) > 0) << 1;
				d_event.data1 |= ((ev.xmotion.state & Button3Mask) > 0) << 2;

				d_event.data2 = (ev.xmotion.x - wwidth / 2) << 1;
				d_event.data3 = (wheight / 2 - ev.xmotion.y) << 1;
				if(d_event.data2 || d_event.data3) {
					D_PostEvent(&d_event);
				}
			}
			else {
				screencoords(&dx, &dy, &dw, &dh);
				x = (int) (((float) (ev.xmotion.x - dx)) / dw * SCREENWIDTH);
				y = (int) (((float) (ev.xmotion.y - dy)) / dh * SCREENHEIGHT);
				M_SelectItemByPosition(x, y);
			}
		}
		else if(ev.type == ConfigureNotify) {
			wwidth = ev.xconfigure.width;
			wheight = ev.xconfigure.height;
			make_image();
			printf("ConfigureNotify! width: %d, height: %d\n", wwidth, wheight);
		}
		else if(ev.type == FocusIn) {
			if(ev.xfocus.window == window && ev.xfocus.mode == NotifyNormal) {
				if(!in_menu()) grab_mouse();
			}
		}
		else if(ev.type == FocusOut) {
			if(ev.xfocus.window == window && ev.xfocus.mode == NotifyNormal) {
				release_mouse();
			}
		}
	}*/
	if(mouse_grabbed) {
		if(in_menu()) {
			release_mouse();
		}
		else {
			//Reset pointer
		}
	}
	else {
		if(!in_menu()) grab_mouse();
	}
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
	else if(!strcmp(interface, "xdg_wm_base")) {
		wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
	}
	else if(!strcmp(interface, "zxdg_decoration_manager_v1")) {
		deco_manager = wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1);
	}
}

void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
}

void buffer_handle_release(void *data, struct wl_buffer *buffer) {
	wl_buffer_destroy(buffer);
}

void surface_handle_configure(void *d, struct xdg_surface *s, uint32_t serial) {
	xdg_surface_ack_configure(s, serial);

	if(!first_frame) {
		draw();
		doo_display_dispatch(display, 1);
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
	wl_callback_add_listener(cb, &frame_listen, NULL);

	if(last_frame == UINT32_MAX || time - last_frame > 10) {
		draw();
		doo_display_dispatch(display, 1);
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


//
// Copyright (C) 1993-1996 by id Software, Inc.
//
/*int xlatekey(KeySym sym) {
	int rc;

	switch(sym) {
	case XK_Left: rc = KEY_LEFTARROW; break;
	case XK_Right: rc = KEY_RIGHTARROW; break;
	case XK_Down: rc = KEY_DOWNARROW; break;
	case XK_Up: rc = KEY_UPARROW; break;
	case XK_Escape: rc = KEY_ESCAPE; break;
	case XK_Return: rc = KEY_ENTER; break;
	case XK_Tab: rc = KEY_TAB; break;
	case XK_F1: rc = KEY_F1; break;
	case XK_F2: rc = KEY_F2; break;
	case XK_F3: rc = KEY_F3; break;
	case XK_F4: rc = KEY_F4; break;
	case XK_F5: rc = KEY_F5; break;
	case XK_F6: rc = KEY_F6; break;
	case XK_F7: rc = KEY_F7; break;
	case XK_F8: rc = KEY_F8; break;
	case XK_F9: rc = KEY_F9; break;
	case XK_F10: rc = KEY_F10; break;
	case XK_F11: rc = KEY_F11; break;
	case XK_F12: rc = KEY_F12; break;

	case XK_BackSpace:
	case XK_Delete: rc = KEY_BACKSPACE; break;

	case XK_Pause: rc = KEY_PAUSE; break;

	case XK_KP_Equal:
	case XK_equal: rc = KEY_EQUALS; break;

	case XK_KP_Subtract:
	case XK_minus: rc = KEY_MINUS; break;

	case XK_Shift_L:
	case XK_Shift_R: rc = KEY_RSHIFT; break;

	case XK_Control_L:
	case XK_Control_R: rc = KEY_RCTRL; break;

	case XK_Alt_L:
	case XK_Meta_L:
	case XK_Alt_R:
	case XK_Meta_R: rc = KEY_RALT; break;

	default:
		if(sym >= XK_space && sym <= XK_asciitilde) rc = sym - XK_space + ' ';
		if(sym >= 'A' && sym <= 'Z') rc = sym - 'A' + 'a';
		break;
	}

	return rc;
}*/
