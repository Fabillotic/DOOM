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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "d_main.h"
#include "doomdef.h"
#include "i_video.h"
#include "m_argv.h"
#include "v_video.h"

Display *display;
Window root;
Window window;
XVisualInfo visual;
Colormap colormap;
GC context;

XImage *image;
unsigned char *image_data;
unsigned char *palette;

int scale;
int wwidth;
int wheight;

void make_image();
int xlatekey(KeySym sym);

void I_InitGraphics() {
	XSetWindowAttributes atts;
	XEvent ev;
	XGCValues vals;
	int screen;

	display = XOpenDisplay(NULL);
	if(!display) {
		printf("Couldn't connect to display!\n");
		return;
	}
	printf("Got display!\n");

	scale = 1;
	if(M_CheckParm("-2")) scale = 2;
	if(M_CheckParm("-3")) scale = 3;
	if(M_CheckParm("-4")) scale = 4;

	wwidth = SCREENWIDTH * scale;
	wheight = SCREENHEIGHT * scale;

	screen = DefaultScreen(display);
	root = RootWindow(display, screen);

	if(!XMatchVisualInfo(display, screen, 24, TrueColor, &visual)) {
		printf("Only screens with 24-bit TrueColor are supported!\n");
		return;
	}
	printf("Found visual info!\n");

	colormap = XCreateColormap(display, root, visual.visual, AllocNone);
	atts = (XSetWindowAttributes){.event_mask = ExposureMask | KeyPressMask |
	                                            KeyReleaseMask |
	                                            StructureNotifyMask,
	    .colormap = colormap,
	    .override_redirect = False};
	window = XCreateWindow(display, root, 0, 0, wwidth, wheight, 0, 24,
	    InputOutput, visual.visual,
	    CWEventMask | CWColormap | CWOverrideRedirect, &atts);
	context = XCreateGC(display, window, 0, &vals);

	printf("Mapping window...\n");
	XMapWindow(display, window);
	XSync(display, False);

	while(1) {
		XNextEvent(display, &ev);
		if(ev.type == Expose && !ev.xexpose.count) break;
	}

	printf("Allocating screen buffer, image buffer and palette.\n");
	screens[0] = (unsigned char *) malloc(
	    SCREENWIDTH * SCREENHEIGHT); // Color index, 8-bit per pixel
	palette = malloc(256 * 3);       // 256 entries, each of them 24-bit

	image = NULL;
	image_data = NULL;

	make_image();
	printf("Finished initializing!\n");
}

void make_image() {
	if(image) {
		XDestroyImage(image);
	}

	// RGB, 8-bit each, actually 32-bit per pixel, cause X11 weirdness
	image_data = malloc(wwidth * wheight * 4);
	image = XCreateImage(display, visual.visual, 24, ZPixmap, 0,
	    (char *) image_data, wwidth, wheight, 8, 4 * wwidth);
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
	int i, j, k, vert, dw, dh, dx, dy, x, y;

	vert = (float) wwidth / wheight < (float) SCREENWIDTH / SCREENHEIGHT;

	if(!vert) {
		dh = wheight;
		dw = (int) (((float) dh / SCREENHEIGHT) * SCREENWIDTH);
		dy = 0;
		dx = (wwidth - dw) / 2;
	}
	else {
		dw = wwidth;
		dh = (int) (((float) dw / SCREENWIDTH) * SCREENHEIGHT);
		dx = 0;
		dy = (wheight - dh) / 2;
	}

	for(i = 0; i < wwidth * wheight; i++) {
		j = i % wwidth;
		k = i / wwidth;

		image_data[i * 4 + 3] = 255;

		/* Black bars */
		if(j < dx || j > dx + dw || k < dy || k > dy + dh) {
			image_data[i * 4 + 0] = 0;
			image_data[i * 4 + 1] = 0;
			image_data[i * 4 + 2] = 0;
		}
		else {
			x = (int) (((float) (j - dx)) / dw * SCREENWIDTH);
			y = (int) (((float) (k - dy)) / dh * SCREENHEIGHT);

			if(x == SCREENWIDTH || y == SCREENHEIGHT) continue;

			image_data[i * 4 + 0] =
			    palette[((int) screens[0][x + SCREENWIDTH * y]) * 3 + 2];
			image_data[i * 4 + 1] =
			    palette[((int) screens[0][x + SCREENWIDTH * y]) * 3 + 1];
			image_data[i * 4 + 2] =
			    palette[((int) screens[0][x + SCREENWIDTH * y]) * 3 + 0];
		}
	}

	XPutImage(display, window, context, image, 0, 0, 0, 0, wwidth, wheight);
	XSync(display, False);
}

void I_ReadScreen(byte *scr) {
	printf("Reading screen...\n");
	memcpy(scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

void I_StartTic() {
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
		}
		else if(ev.type == KeyRelease) {
			d_event.type = ev_keyup;
			ev.xkey.state =
			    ev.xkey.state & (~(ControlMask | LockMask | ShiftMask));
			XLookupString(&ev.xkey, buf, 256, &sym, NULL);
			d_event.data1 = xlatekey(sym);
			D_PostEvent(&d_event);
		}
		else if(ev.type == ConfigureNotify) {
			wwidth = ev.xconfigure.width;
			wheight = ev.xconfigure.height;
			make_image();
		}
	}
}

void I_StartFrame() {
}

//
// Copyright (C) 1993-1996 by id Software, Inc.
//
int xlatekey(KeySym sym) {
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
}
