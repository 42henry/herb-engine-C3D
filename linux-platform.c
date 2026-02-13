#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>

void update_pixels();

static uint32_t *pixels = NULL;

int main() {
    // Open X display
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to open X display\n");
        return 1;
    }

    // Create a window
    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, WIDTH, HEIGHT, 1,
                                        BlackPixel(display, screen), WhitePixel(display, screen));
    XMapWindow(display, window);

    // Create a graphics context
    GC gc = XCreateGC(display, window, 0, NULL);

    // Create an XImage for pixel manipulation
    XImage *image = XCreateImage(display, DefaultVisual(display, screen), DefaultDepth(display, screen), ZPixmap,
                                 0, (char *)malloc(WIDTH * HEIGHT * 4), WIDTH, HEIGHT, 32, 0);

    // Create a pixel array
    pixels = (uint32_t *)image->data;

    // Main loop to update the window color
	init_stuff();
    w     = XKeysymToKeycode(display, XK_w);
    a     = XKeysymToKeycode(display, XK_a);
    s     = XKeysymToKeycode(display, XK_s);
    d     = XKeysymToKeycode(display, XK_d);
    shift = XKeysymToKeycode(display, XK_Shift_L);

    while (1) {
		update_pixels();

		// Get mouse coordinates
		Window root, child;
		int root_x, root_y;
		unsigned int mask;
		XQueryPointer(display, window,
              &root, &child,
              &root_x, &root_y,
              &mouse.x, &mouse.y,
              &mask);

		// get keyboad
		XQueryKeymap(display, keys);

		// Draw the updated image
		XPutImage(display, window, gc, image, 0, 0, 0, 0, WIDTH, HEIGHT);
        usleep(FRAME_LENGTH);
    }

    // Cleanup
    free(pixels);
    XDestroyImage(image);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}

void update_pixels() {
	int r = 0;
	int g = 255;
	int b = 0;
	int a = 1;
	for (int i = 0; i < WIDTH * HEIGHT; i++) {
		pixels[i] = (a << 24 | r << 16) | (g << 8) | b;
	}
}
