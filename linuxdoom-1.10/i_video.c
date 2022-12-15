#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "i_video.h"
#include "v_video.h"
#include "d_main.h"

Display *display;
Window root;
Window window;
XVisualInfo visual;
Colormap colormap;
GC context;

XImage *image;
unsigned char *image_data;
unsigned char *palette;

#define SCALE 5

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
	
	screen = DefaultScreen(display);
	root = RootWindow(display, screen);
	
	if(!XMatchVisualInfo(display, screen, 24, TrueColor, &visual)) {
		printf("Only screens with 24-bit TrueColor are supported!\n");
		return;
	}
	printf("Found visual info!\n");
	
	colormap = XCreateColormap(display, root, visual.visual, AllocNone);
	atts = (XSetWindowAttributes) {.event_mask = ExposureMask, .colormap = colormap, .override_redirect = False};
	window = XCreateWindow(display, root, 0, 0, SCREENWIDTH * SCALE, SCREENHEIGHT * SCALE, 0, 24, InputOutput, visual.visual, CWEventMask | CWColormap | CWOverrideRedirect, &atts);
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
	image_data = malloc(SCREENWIDTH * SCALE * SCREENHEIGHT * SCALE * 4); //RGB, 8-bit each, actually 32-bit per pixel, cause X11 weirdness
	palette = malloc(256 * 3); //256 entries, each of them 24-bit
	
	printf("Creating image...\n");
	image = XCreateImage(display, visual.visual, 24, ZPixmap, 0, (char*) image_data, SCREENWIDTH * SCALE, SCREENHEIGHT * SCALE, 8, 4 * SCREENWIDTH * SCALE);
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
	
	for(i = 0; i < SCREENWIDTH*SCALE*SCREENHEIGHT*SCALE; i++) {
		//Calculate index into unscaled screen buffer
		j = i / SCREENWIDTH / SCALE / SCALE * SCREENWIDTH + i % (SCREENWIDTH * SCALE * SCALE) % (SCREENWIDTH * SCALE) / SCALE;
		
		//ORDER: BGR (A?)
		image_data[i * 4 + 3] = 255;
		image_data[i * 4 + 2] = palette[((int) screens[0][j]) * 3 + 0];
		image_data[i * 4 + 1] = palette[((int) screens[0][j]) * 3 + 1];
		image_data[i * 4 + 0] = palette[((int) screens[0][j]) * 3 + 2];
	}
	
	XPutImage(display, window, context, image, 0, 0, 0, 0, SCREENWIDTH*SCALE, SCREENHEIGHT*SCALE);
	XSync(display, False);
}

void I_ReadScreen(byte *scr) {
	printf("Reading screen...\n");
	memcpy(scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}

void I_StartTic() {
}

void I_StartFrame() {
}
