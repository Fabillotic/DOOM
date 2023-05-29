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
#include <unistd.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#ifdef JOYSTICK
#include <linux/input.h>
#include <linux/joystick.h>
#endif
#ifdef OPENGL
#include <GL/glew.h>
#include <GL/glxew.h>
#include "shader.h"
#endif
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_video.h"
#include "m_argv.h"
#include "v_video.h"
#include "m_menu.h"

Display *display;
Window root;
Window window;
XVisualInfo visual;
Colormap colormap;

#ifndef OPENGL
GC context;
XImage *image;
#else
GLXContext context;
int shader;
int vertexArray;
int vertexBuffer;
int texture;
#endif

unsigned char *image_data;
unsigned char *palette;

Cursor empty_cursor;
int mouse_grabbed;

int scale;
int wwidth;
int wheight;

int fullscreen;

#ifdef JOYTEST
int joytest[8];
#define JOYTEST_FORWARD 't'
#define JOYTEST_LEFT 'f'
#define JOYTEST_BACK 'g'
#define JOYTEST_RIGHT 'h'
#define JOYTEST_FIRE 'c'
#define JOYTEST_STRAFE 'v'
#define JOYTEST_USE 'b'
#define JOYTEST_SPEED 'n'
#endif

#ifdef JOYSTICK
struct JS_DATA_TYPE joystick;
int joystick_fd;
#endif

#define EVENTMASK (ExposureMask | KeyPressMask | KeyReleaseMask \
		| StructureNotifyMask | FocusChangeMask)
#define POINTERMASK (ButtonPressMask | ButtonReleaseMask | PointerMotionMask)

void make_image();
void grab_mouse();
void release_mouse();
void create_empty_cursor();
int in_menu();
void screencoords(int *dx, int *dy, int *dw, int *dh);
int xlatekey(KeySym sym);

void I_InitGraphics() {
	XSetWindowAttributes atts;
	XEvent ev;
	XGCValues vals;
	Atom wm_state, wm_fullscreen;
	int screen, p;

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

	fullscreen = M_CheckParm("-fullscreen");

	wwidth = SCREENWIDTH * scale;
	wheight = SCREENHEIGHT * scale;

	screen = DefaultScreen(display);
	root = RootWindow(display, screen);

#ifndef OPENGL
	if(!XMatchVisualInfo(display, screen, 24, TrueColor, &visual)) {
		printf("Only screens with 24-bit TrueColor are supported!\n");
		return;
	}

	colormap = XCreateColormap(display, root, visual.visual, AllocNone);
	atts = (XSetWindowAttributes){
	    .event_mask = EVENTMASK,
	    .colormap = colormap,
	    .override_redirect = False};
	window = XCreateWindow(display, root, 0, 0, wwidth, wheight, 0, 24,
	    InputOutput, visual.visual,
	    CWEventMask | CWColormap | CWOverrideRedirect, &atts);
	context = XCreateGC(display, window, 0, &vals);
#else
	Screen *scr = XDefaultScreenOfDisplay(display);
	XVisualInfo *visual_temp = malloc(sizeof(XVisualInfo));
	visual_temp->visual = DefaultVisualOfScreen(scr);
	visual_temp->visualid = XVisualIDFromVisual(visual_temp->visual);
	visual_temp->screen = screen;
	visual_temp->depth = DefaultDepthOfScreen(scr);

	XWindowAttributes gwa;
	XGetWindowAttributes(display, root, &gwa);

	atts = (XSetWindowAttributes){
	    .event_mask = EVENTMASK,
	    .override_redirect = False};
	window = XCreateWindow(display, root, 0, 0, wwidth, wheight, 0,
		CopyFromParent, InputOutput, CopyFromParent,
		CWEventMask | CWOverrideRedirect, &atts
	);
	XSync(display, False);
#endif

	mouse_grabbed = 0;

	if(fullscreen) {
		wm_state = XInternAtom(display, "_NET_WM_STATE", True);
		wm_fullscreen = XInternAtom(display,
			"_NET_WM_STATE_FULLSCREEN", True
		);
		XChangeProperty(display, window, wm_state, XA_ATOM, 32,
			PropModeReplace, (unsigned char*) &wm_fullscreen, 1
		);
	}

	XClassHint *class_hint;
	class_hint = XAllocClassHint();
	class_hint->res_name = "DOOM";
	class_hint->res_class = "doom";
	XSetClassHint(display, window, class_hint);

	XStoreName(display, window, "DOOM");

	printf("Mapping window...\n");
	XMapWindow(display, window);
	XSync(display, False);

#ifdef OPENGL
	int r;
	XVisualInfo *visual_info = XGetVisualInfo(display, VisualIDMask \
		| VisualScreenMask | VisualDepthMask, visual_temp, &r
	);
	printf("visual_info: %p\nr: %d\n", visual_info, r);
	free(visual_temp);

	context = glXCreateContext(display, visual_info, NULL, 1);
	printf("context: %p\n", context);
	glXMakeCurrent(display, window, context);
	printf("make current\n");

	glewExperimental=GL_TRUE;
	int err = glewInit();
	if(err != GLEW_OK) {
		printf("%s\n", glewGetErrorString(err));
		printf("glewInit() failed!\n");
		return;
	}

#ifndef GL2
	shader = create_shader(
		"#version 330 core\n\
		layout(location = 0) in vec2 pos;\n\
		layout(location = 1) in vec2 tex;\n\
		uniform vec2 aspect_scale;\n\
		out vec2 uv;\n\
		void main() {\n\
			gl_Position = vec4(pos * aspect_scale, 0.0, 1.0);\n\
			uv = tex;\n\
		}\n\
		",
		NULL,
		"#version 330 core\n\
		in vec2 uv;\n\
		out vec3 color;\n\
		uniform sampler2D tex;\n\
		void main() {color = texture(tex, uv).rgb;}\n\
		"
	);

	printf("shader: %d\n", shader);

	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);

	printf("vertexArray: %d\n", vertexArray);

	float data[] = {
		//-1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 1.0f

		//Positions
		-1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f,

		//UVs
		0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f
	};

	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
#endif

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#endif

	while(1) {
		XNextEvent(display, &ev);
		if(ev.type == Expose && !ev.xexpose.count) break;
		if(ev.type == ConfigureNotify) {
			wwidth = ev.xconfigure.width;
			wheight = ev.xconfigure.height;
		}
	}
	XSelectInput(display, window, EVENTMASK);

	create_empty_cursor();

#ifdef JOYTEST
	memset(joytest, 0, 8 * sizeof(int));
#endif

#ifdef JOYSTICK
	memset(&joystick, 0, sizeof(struct JS_DATA_TYPE));

	joystick_fd = -1;
	if((p = M_CheckParm("-joystick"))) {
		if(p < myargc - 1) {
			joystick_fd = open(myargv[p + 1], O_RDONLY);
		}
	}
	if(joystick_fd < 0) {
		printf("Failed to open joystick!\n");
	}

#endif

	printf("Allocating screen buffer, image buffer and palette.\n");
	screens[0] = (unsigned char *) malloc(
	    SCREENWIDTH * SCREENHEIGHT); // Color index, 8-bit per pixel
	palette = malloc(256 * 3);       // 256 entries, each of them 24-bit

	image_data = NULL;

#ifndef OPENGL
	image = NULL;
#else
	image_data = malloc(SCREENWIDTH * SCREENHEIGHT * 3);
	glViewport(0, 0, wwidth, wheight);
#endif

	make_image();
	printf("Finished initializing!\n");
}

void make_image() {
#ifndef OPENGL
	if(image) {
		XDestroyImage(image);
	}

	// RGB, 8-bit each, actually 32-bit per pixel, cause X11 weirdness
	image_data = malloc(wwidth * wheight * 4);
	image = XCreateImage(display, visual.visual, 24, ZPixmap, 0,
	    (char *) image_data, wwidth, wheight, 8, 4 * wwidth);
#endif
}

void grab_mouse() {
	if(!mouse_grabbed)
		XGrabPointer(display, window, True,
		    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		    GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
	XDefineCursor(display, window, empty_cursor);
	XSelectInput(display, window, EVENTMASK);
	mouse_grabbed = 1;
}

void release_mouse() {
	if(mouse_grabbed) XUngrabPointer(display, CurrentTime);
	XUndefineCursor(display, window);
	XSelectInput(display, window, EVENTMASK | POINTERMASK);
	mouse_grabbed = 0;
}

void create_empty_cursor() {
	XColor col;
	Pixmap pix;

	col.pixel = 0;
	col.red = 0;
	col.flags = DoRed;

	pix = XCreatePixmap(display, root, 1, 1, 1);
	empty_cursor = XCreatePixmapCursor(display, pix, pix, &col, &col, 0, 0);
	XFreePixmap(display, pix);
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
	int i, j, k, dw, dh, dx, dy, x, y;
	float aspect_scale_x, aspect_scale_y;

#ifdef OPENGL
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
#endif

	screencoords(&dx, &dy, &dw, &dh);

#ifndef OPENGL
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

			image_data[i * 4 + 0] = palette[
				((int) screens[0][x + SCREENWIDTH * y]) * 3 + 2
			];
			image_data[i * 4 + 1] = palette[
				((int) screens[0][x + SCREENWIDTH * y]) * 3 + 1
			];
			image_data[i * 4 + 2] = palette[
				((int) screens[0][x + SCREENWIDTH * y]) * 3 + 0
			];
		}
	}
#else
	for(i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++) {
		x = i % SCREENWIDTH;
		y = i / SCREENWIDTH;
		image_data[i * 3 + 0] = palette[
			((int) screens[0][x + SCREENWIDTH * y]) * 3 + 2
		];
		image_data[i * 3 + 1] = palette[
			((int) screens[0][x + SCREENWIDTH * y]) * 3 + 1
		];
		image_data[i * 3 + 2] = palette[
			((int) screens[0][x + SCREENWIDTH * y]) * 3 + 0
		];
	}
#endif

#ifndef OPENGL
	XPutImage(display, window, context, image, 0, 0, 0, 0, wwidth, wheight);
	XSync(display, False);
#else
	aspect_scale_x = (float) SCREENWIDTH / (float) SCREENHEIGHT;
	aspect_scale_x /= (float) wwidth / (float) wheight;
	aspect_scale_y = 1.0f;
	if(aspect_scale_x > 1.0f) {
		aspect_scale_y = (float) SCREENHEIGHT / (float) SCREENWIDTH;
		aspect_scale_y /= (float) wheight / (float) wwidth;
		aspect_scale_x = 1.0f;
	}

#ifndef GL2
	glUseProgram(shader);
	glUniform2f(glGetUniformLocation(shader, "aspect_scale"),
		aspect_scale_x, aspect_scale_y
	);
#else
	glEnable(GL_TEXTURE_2D);
#endif

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCREENWIDTH, SCREENHEIGHT, 0,
		GL_BGR, GL_UNSIGNED_BYTE, image_data
	);

#ifndef GL2
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*) 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, sizeof(float) * 12);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
#else
	glBegin(GL_TRIANGLES);
	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(-1.0f * aspect_scale_x, -1.0f * aspect_scale_y);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(1.0f * aspect_scale_x, 1.0f * aspect_scale_y);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f * aspect_scale_x, 1.0f * aspect_scale_y);

	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(-1.0f * aspect_scale_x, -1.0f * aspect_scale_y);

	glTexCoord2f(1.0f, 1.0f);
	glVertex2f(1.0f * aspect_scale_x, -1.0f * aspect_scale_y);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(1.0f * aspect_scale_x, 1.0f * aspect_scale_y);
	glEnd();

	glDisable(GL_TEXTURE_2D);
#endif

	glXSwapBuffers(display, window);
#endif
}

void I_ReadScreen(byte *scr) {
	printf("Reading screen...\n");
	memcpy(scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

void I_StartTic() {
	int x, y, dx, dy, dw, dh;
	XEvent ev;
	event_t d_event;
	char buf[256];
	KeySym sym;

#ifdef JOYSTICK
	struct JS_DATA_TYPE js_data;
#endif

	while(XPending(display)) {
		XNextEvent(display, &ev);

		if(ev.type == KeyPress) {
			d_event.type = ev_keydown;
			ev.xkey.state = ev.xkey.state &
				(~(ControlMask | LockMask | ShiftMask));
			XLookupString(&ev.xkey, buf, 256, &sym, NULL);
			d_event.data1 = xlatekey(sym);
			D_PostEvent(&d_event);

#ifdef JOYTEST
			if(d_event.data1 == JOYTEST_FORWARD) joytest[0] = 1;
			else if(d_event.data1 == JOYTEST_LEFT) joytest[1] = 1;
			else if(d_event.data1 == JOYTEST_BACK) joytest[2] = 1;
			else if(d_event.data1 == JOYTEST_RIGHT) joytest[3] = 1;
			else if(d_event.data1 == JOYTEST_FIRE) joytest[4] = 1;
			else if(d_event.data1 == JOYTEST_STRAFE) joytest[5] = 1;
			else if(d_event.data1 == JOYTEST_USE) joytest[6] = 1;
			else if(d_event.data1 == JOYTEST_SPEED) joytest[7] = 1;

			d_event.type = ev_joystick;
			d_event.data1 = joytest[4] | (joytest[5] << 1) |
				(joytest[6] << 2) | (joytest[7] << 3);
			d_event.data2 = (joytest[1] ^ joytest[3]) ?
				(joytest[3] * 2 - 1) : 0;
			d_event.data3 = (joytest[0] ^ joytest[2]) ?
				(joytest[2] * 2 - 1) : 0;
			D_PostEvent(&d_event);
#endif
		}
		else if(ev.type == KeyRelease) {
			d_event.type = ev_keyup;
			ev.xkey.state = ev.xkey.state &
				(~(ControlMask | LockMask | ShiftMask));
			XLookupString(&ev.xkey, buf, 256, &sym, NULL);
			d_event.data1 = xlatekey(sym);
			D_PostEvent(&d_event);

#ifdef JOYTEST
			if(d_event.data1 == JOYTEST_FORWARD) joytest[0] = 0;
			else if(d_event.data1 == JOYTEST_LEFT) joytest[1] = 0;
			else if(d_event.data1 == JOYTEST_BACK) joytest[2] = 0;
			else if(d_event.data1 == JOYTEST_RIGHT) joytest[3] = 0;
			else if(d_event.data1 == JOYTEST_FIRE) joytest[4] = 0;
			else if(d_event.data1 == JOYTEST_STRAFE) joytest[5] = 0;
			else if(d_event.data1 == JOYTEST_USE) joytest[6] = 0;
			else if(d_event.data1 == JOYTEST_SPEED) joytest[7] = 0;

			d_event.type = ev_joystick;
			d_event.data1 = joytest[4] | (joytest[5] << 1) |
				(joytest[6] << 2) | (joytest[7] << 3);
			d_event.data2 = (joytest[1] ^ joytest[3]) ?
				(joytest[3] * 2 - 1) : 0;
			d_event.data3 = (joytest[0] ^ joytest[2]) ?
				(joytest[2] * 2 - 1) : 0;
			D_PostEvent(&d_event);
#endif
		}
		else if(ev.type == ButtonPress) {
			d_event.type = ev_mouse;

			d_event.data1 = 0;
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
			d_event.data1 |=
				((ev.xbutton.state & Button2Mask) > 0) << 1;
			d_event.data1 |=
				((ev.xbutton.state & Button3Mask) > 0) << 2;
			if(ev.xbutton.button == Button1)
				d_event.data1 &= ~(1 << 0);
			if(ev.xbutton.button == Button2)
				d_event.data1 &= ~(1 << 1);
			if(ev.xbutton.button == Button3)
				d_event.data1 &= ~(1 << 2);

			d_event.data2 = 0;
			d_event.data3 = 0;

			D_PostEvent(&d_event);
		}
		else if(ev.type == MotionNotify) {
			if(!in_menu()) {
				d_event.type = ev_mouse;

				d_event.data1 =
					((ev.xmotion.state & Button1Mask) > 0);
				d_event.data1 |= (
					(ev.xmotion.state & Button2Mask) > 0)
					<< 1;
				d_event.data1 |=
					((ev.xmotion.state & Button3Mask) > 0)
					<< 2;

				d_event.data2 =
					(ev.xmotion.x - wwidth / 2) << 1;
				d_event.data3 =
					(wheight / 2 - ev.xmotion.y) << 1;
				if(d_event.data2 || d_event.data3) {
					D_PostEvent(&d_event);
				}
			}
			else {
				screencoords(&dx, &dy, &dw, &dh);
				x = (int) (((float) (ev.xmotion.x - dx))
					/ dw * SCREENWIDTH);
				y = (int) (((float) (ev.xmotion.y - dy))
					/ dh * SCREENHEIGHT);
				M_SelectItemByPosition(x, y);
			}
		}
		else if(ev.type == ConfigureNotify) {
			wwidth = ev.xconfigure.width;
			wheight = ev.xconfigure.height;

			make_image();
#ifdef OPENGL
			glViewport(0, 0, wwidth, wheight);
#endif
		}
		else if(ev.type == FocusIn && ev.xfocus.mode == NotifyNormal) {
			if(ev.xfocus.window == window) {
				if(!in_menu()) grab_mouse();
			}
		}
		else if(ev.type == FocusOut && ev.xfocus.mode == NotifyNormal) {
			if(ev.xfocus.window == window) {
				release_mouse();
			}
		}
	}
	if(mouse_grabbed) {
		if(in_menu()) {
			release_mouse();
		}
		else {
			XWarpPointer(display, None, window, 0, 0, 0, 0,
				wwidth / 2, wheight / 2
			);
			XSync(display, False);
		}
	}
	else {
		if(!in_menu()) grab_mouse();
	}

#ifdef JOYSTICK
	if(joystick_fd > 0) read(joystick_fd, &js_data, JS_RETURN);

	if(joystick_fd > 0 && (
		js_data.x != joystick.x ||
		js_data.y != joystick.y ||
		js_data.buttons != joystick.buttons
	)) {
		d_event.type = ev_joystick;
		d_event.data1 = js_data.buttons;
		d_event.data2 = js_data.x < 128 ? -1 : (js_data.x > 128);
		d_event.data3 = js_data.y < 128 ? -1 : (js_data.y > 128);
		d_event.data3 = -d_event.data3;
		/*printf("joystick! buttons: %d, x-axis: %d, y-axis: %d\n",
		    d_event.data1, d_event.data2, d_event.data3);*/
		D_PostEvent(&d_event);

		joystick.x = js_data.x;
		joystick.y = js_data.y;
		joystick.buttons = js_data.buttons;
	}
#endif
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
		if(sym >= XK_space && sym <= XK_asciitilde)
			rc = sym - XK_space + ' ';
		if(sym >= 'A' && sym <= 'Z') rc = sym - 'A' + 'a';
		break;
	}

	return rc;
}
