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
	window = XCreateWindow(display, root, 0, 0, SCREENWIDTH, SCREENHEIGHT, 0, 24, InputOutput, visual.visual, CWEventMask | CWColormap | CWOverrideRedirect, &atts);
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
	image_data = malloc(SCREENWIDTH * SCREENHEIGHT * 3); //RGB, 8-bit each, 24-bit per pixel
	palette = malloc(256 * 3); //256 entries, each of them 24-bit
	
	printf("Creating image...\n");
	image = XCreateImage(display, visual.visual, 24, ZPixmap, 0, (char*) image_data, SCREENWIDTH, SCREENHEIGHT, 8, 4 * SCREENWIDTH);
	printf("Image: %d\n", image);
	printf("Finished initializing!\n");
}

void I_ShutdownGraphics() {
}

void I_SetPalette(byte *pal) {
	int i;
	
	printf("Copying palette...\n");
	memcpy(palette, pal, 256 * 3);
	
	printf("Looking up gamma...\n");
	for(i = 0; i < 256 * 3; i++) {
		palette[i] = gammatable[usegamma][palette[i]];
	}
	printf("Done setting palette!\n");
}

void I_UpdateNoBlit() {
}

void I_FinishUpdate() {
	int i;
	
	printf("Rendering image!\n");
	memset(image_data, 255, SCREENWIDTH*SCREENHEIGHT*4);
	for(i = 0; i < SCREENWIDTH*SCREENHEIGHT; i++) {
		//ORDER: BGR (A?)
		image_data[i * 4 + 2] = palette[((int) screens[0][i]) * 3 + 0];
		image_data[i * 4 + 1] = palette[((int) screens[0][i]) * 3 + 1];
		image_data[i * 4 + 0] = palette[((int) screens[0][i]) * 3 + 2];
	}
	printf("Image data converted!\n");
	
	printf("display: %d, window: %d, context: %d, image: %d\n", display, window, context, image);
	XPutImage(display, window, context, image, 0, 0, 0, 0, SCREENWIDTH, SCREENHEIGHT);
	XSync(display, False);
	printf("Image output!\n");
}

void I_ReadScreen(byte *scr) {
	printf("Reading screen...\n");
	memcpy(scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}

void I_StartTic() {
}

void I_StartFrame() {
}
