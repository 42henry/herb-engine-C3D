#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "herbengineC3D.c"

int main() {
    // Open X display
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to open X display\n");
        return 1;
    }

    // Create a window
    int screen = DefaultScreen(display);
	Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0,
                                    WINDOW_WIDTH, WINDOW_HEIGHT, 1,
                                    BlackPixel(display, screen), BlackPixel(display, screen));
    XMapWindow(display, window);

	// hide the mouse
	XGrabPointer(display, window, True,
             ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
             GrabModeAsync, GrabModeAsync,
             window, None, CurrentTime);
	Pixmap blank;
	XColor dummy;
	char data[1] = {0};

	blank = XCreateBitmapFromData(display, window, data, 1, 1);
	Cursor cursor = XCreatePixmapCursor(display, blank, blank, &dummy, &dummy, 0, 0);
	XDefineCursor(display, window, cursor);
	mouse_defined = 1;

	// input
	XSelectInput(display, window,
		ExposureMask |
		KeyPressMask |
		KeyReleaseMask |
		ButtonPressMask |
		ButtonReleaseMask
	);
	
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
    space = XKeysymToKeycode(display, XK_space);
    control = XKeysymToKeycode(display, XK_Control_L);
    escape = XKeysymToKeycode(display, XK_Escape);
    one     = XKeysymToKeycode(display, XK_1);
    two     = XKeysymToKeycode(display, XK_2);
    three     = XKeysymToKeycode(display, XK_3);
    four     = XKeysymToKeycode(display, XK_4);
    five     = XKeysymToKeycode(display, XK_5);
    six     = XKeysymToKeycode(display, XK_6);
    seven     = XKeysymToKeycode(display, XK_7);
    eight     = XKeysymToKeycode(display, XK_8);
    nine     = XKeysymToKeycode(display, XK_9);

	clock_gettime(CLOCK_MONOTONIC, &last);

	// ENTRY POINT
	while(1) {
		while (XPending(display)) {
			// --- Handle events ---
			XEvent event;
			XNextEvent(display, &event);

			switch (event.type) {
				case KeyPress: {
					KeyCode code = event.xkey.keycode;
					keys[code] = 1;
					break;
			    }
				case KeyRelease: {
					KeyCode code = event.xkey.keycode;
					keys[code] = 0;
					break;
			    }
				case ButtonPress: {
					int button_type = event.xbutton.button;
					if (button_type == 1) {
						mouse_left_click = 1;
					}
					else if (button_type == 3) {
						mouse_right_click = 1;
					}
					break;
			    }
				case ButtonRelease: {
					mouse_left_click = 0;
					mouse_right_click = 0;
					break;
				}
				case DestroyNotify: {
					break;
				}
			}
		}

		if (hold_mouse) {
			// handle the mouse: 
			if (! mouse_defined) {
				XDefineCursor(display, window, cursor);
				mouse_defined = 1;
				XWarpPointer(display, None, window,
				 0, 0, 0, 0,
				 (WIDTH / 2), (HEIGHT / 2));
			}
			Window root, child;
			int root_x, root_y;
			unsigned int mask;

			XQueryPointer(display, window,
						  &root, &child,
						  &root_x, &root_y,
						  &mouse.x, &mouse.y,
						  &mask);

			// Warp pointer back to center
			XWarpPointer(display, None, window,
					 0, 0, 0, 0,
					 WIDTH / 2, HEIGHT / 2);
		}
		else {
			if (mouse_defined) {
				XUndefineCursor(display, window);
				mouse_defined = 0;
			}
		}

		// --- Update ---
		update();

		// --- Render ---
		// Get actual window size
		XWindowAttributes attr;
		XGetWindowAttributes(display, window, &attr);

		int win_w = attr.width;
		int win_h = attr.height;

		// Maintain aspect ratio
		float scale_x = (float)win_w / WIDTH;
		float scale_y = (float)win_h / HEIGHT;
		float scale   = scale_x < scale_y ? scale_x : scale_y;

		int draw_w = (int)(WIDTH * scale);
		int draw_h = (int)(HEIGHT * scale);

		int offset_x = (win_w - draw_w) / 2;
		int offset_y = (win_h - draw_h) / 2;

		// Clear background (black bars)
		// XSetForeground(display, gc, BlackPixel(display, screen));
		// XFillRectangle(display, window, gc, 0, 0, win_w, win_h);

		// Scale + draw
		XPutImage(display, window, gc,
				  image,
				  0, 0,
				  offset_x, offset_y,
				  draw_w, draw_h);

		XFlush(display);

		// --- Frame timing ---
		clock_gettime(CLOCK_MONOTONIC, &now);

		long elapsed = (now.tv_sec - last.tv_sec) * 1000000000L + (now.tv_nsec - last.tv_nsec);

		if (elapsed < FRAME_TIME_NS) {
			struct timespec sleep_time;
			sleep_time.tv_sec = 0;
			sleep_time.tv_nsec = FRAME_TIME_NS - elapsed;
			nanosleep(&sleep_time, NULL);
		}

		clock_gettime(CLOCK_MONOTONIC, &last);
		
	}

    // Cleanup
	cleanup();
    XDestroyImage(image);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}
