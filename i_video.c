#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "i_video.h"
#include "v_video.h"
#include "d_main.h"
#include "doomdef.h"
#include "m_argv.h"

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
	
	screen = DefaultScreen(display);
	root = RootWindow(display, screen);
	
	if(!XMatchVisualInfo(display, screen, 24, TrueColor, &visual)) {
		printf("Only screens with 24-bit TrueColor are supported!\n");
		return;
	}
	printf("Found visual info!\n");
	
	colormap = XCreateColormap(display, root, visual.visual, AllocNone);
	atts = (XSetWindowAttributes) {.event_mask = ExposureMask|KeyPressMask|KeyReleaseMask, .colormap = colormap, .override_redirect = False};
	window = XCreateWindow(display, root, 0, 0, SCREENWIDTH * scale, SCREENHEIGHT * scale, 0, 24, InputOutput, visual.visual, CWEventMask | CWColormap | CWOverrideRedirect, &atts);
	context = XCreateGC(display, window, 0, &vals);
	
	printf("Mapping window...\n");
	XMapWindow(display, window);
	XSync(display, False);
	
	while(1) {
		XNextEvent(display, &ev);
		if(ev.type == Expose && !ev.xexpose.count) break;
	}
	
	printf("Allocating screen buffer, image buffer and palette.\n");
	screens[0] = (unsigned char*) malloc(SCREENWIDTH * SCREENHEIGHT); //Color index, 8-bit per pixel
	image_data = malloc(SCREENWIDTH * scale * SCREENHEIGHT * scale * 4); //RGB, 8-bit each, actually 32-bit per pixel, cause X11 weirdness
	palette = malloc(256 * 3); //256 entries, each of them 24-bit
	
	printf("Creating image...\n");
	image = XCreateImage(display, visual.visual, 24, ZPixmap, 0, (char*) image_data, SCREENWIDTH * scale, SCREENHEIGHT * scale, 8, 4 * SCREENWIDTH * scale);
	printf("Image: %d\n", image);
	printf("Finished initializing!\n");
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
	int i, j;
	
	for(i = 0; i < SCREENWIDTH*scale*SCREENHEIGHT*scale; i++) {
		//Calculate index into unscaled screen buffer
		j = i / SCREENWIDTH / scale / scale * SCREENWIDTH + i % (SCREENWIDTH * scale * scale) % (SCREENWIDTH * scale) / scale;
		
		//ORDER: BGR (A?)
		image_data[i * 4 + 3] = 255;
		image_data[i * 4 + 2] = palette[((int) screens[0][j]) * 3 + 0];
		image_data[i * 4 + 1] = palette[((int) screens[0][j]) * 3 + 1];
		image_data[i * 4 + 0] = palette[((int) screens[0][j]) * 3 + 2];
	}
	
	XPutImage(display, window, context, image, 0, 0, 0, 0, SCREENWIDTH*scale, SCREENHEIGHT*scale);
	XSync(display, False);
}

void I_ReadScreen(byte *scr) {
	printf("Reading screen...\n");
	memcpy(scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}

int xlatekey(KeySym sym);
void I_StartTic() {
	XEvent ev;
	event_t d_event;
	char buf[256];
	KeySym sym;
	
	while(XPending(display)) {
		XNextEvent(display, &ev);
		
		if(ev.type == KeyPress) {
			d_event.type = ev_keydown;
			XLookupString(&ev.xkey, buf, 256, &sym, NULL);
			d_event.data1 = xlatekey(sym);
			D_PostEvent(&d_event);
		}
		else if(ev.type == KeyRelease) {
			d_event.type = ev_keyup;
			XLookupString(&ev.xkey, buf, 256, &sym, NULL);
			d_event.data1 = xlatekey(sym);
			D_PostEvent(&d_event);
		}
	}
}

void I_StartFrame() {
}

int xlatekey(KeySym sym) {
	int rc;
	
	switch(sym) {
		case XK_Left:	rc = KEY_LEFTARROW;	break;
		case XK_Right:	rc = KEY_RIGHTARROW;	break;
		case XK_Down:	rc = KEY_DOWNARROW;	break;
		case XK_Up:	rc = KEY_UPARROW;	break;
		case XK_Escape:	rc = KEY_ESCAPE;	break;
		case XK_Return:	rc = KEY_ENTER;		break;
		case XK_Tab:	rc = KEY_TAB;		break;
		case XK_F1:	rc = KEY_F1;		break;
		case XK_F2:	rc = KEY_F2;		break;
		case XK_F3:	rc = KEY_F3;		break;
		case XK_F4:	rc = KEY_F4;		break;
		case XK_F5:	rc = KEY_F5;		break;
		case XK_F6:	rc = KEY_F6;		break;
		case XK_F7:	rc = KEY_F7;		break;
		case XK_F8:	rc = KEY_F8;		break;
		case XK_F9:	rc = KEY_F9;		break;
		case XK_F10:	rc = KEY_F10;		break;
		case XK_F11:	rc = KEY_F11;		break;
		case XK_F12:	rc = KEY_F12;		break;
		
		case XK_BackSpace:
		case XK_Delete:	rc = KEY_BACKSPACE;	break;
		
		case XK_Pause:	rc = KEY_PAUSE;		break;
		
		case XK_KP_Equal:
		case XK_equal:	rc = KEY_EQUALS;	break;
		
		case XK_KP_Subtract:
		case XK_minus:	rc = KEY_MINUS;		break;
		
		case XK_Shift_L:
		case XK_Shift_R:
		rc = KEY_RSHIFT;
		break;
		
		case XK_Control_L:
		case XK_Control_R:
		rc = KEY_RCTRL;
		break;
		
		case XK_Alt_L:
		case XK_Meta_L:
		case XK_Alt_R:
		case XK_Meta_R:
		rc = KEY_RALT;
		break;
		
		default:
		if (sym >= XK_space && sym <= XK_asciitilde)
			rc = sym - XK_space + ' ';
		if (sym >= 'A' && sym <= 'Z')
			rc = sym - 'A' + 'a';
		break;
	}
	
	return rc;
}
