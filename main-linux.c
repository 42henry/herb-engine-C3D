#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#define WIDTH  800
#define HEIGHT 500

#define CM_TO_PIXELS 10

#define WIDTH_IN_CM ((WIDTH) / (CM_TO_PIXELS))

#define CUBE_WIDTH (30 * CM_TO_PIXELS) // cube is 100cm big
#define MAX_SQUARES 100

#define TEXTURE_WIDTH 100
#define COORDS_PER_SQUARE ((TEXTURE_WIDTH + 1) * (TEXTURE_WIDTH + 1))

#define FRAME_LENGTH 16666

typedef struct {
	int x;
	int y;
	int z;	
} Vec3;

typedef struct {
	int x;
	int y;
} Vec2;

typedef struct {
	Vec3 start;
	Vec3 end;
} Line_t;

typedef struct {
	int r;
	int g;
	int b;
} Colour_t;

typedef struct {
	Vec3 coords[COORDS_PER_SQUARE];	
} Square_t;

typedef struct {
	Square_t *items;
	int count;
} Squares_t;

static uint32_t *pixels = NULL;

static void init_stuff();
static void debug_log(char *str);

static int get_line_intercept(Line_t line);
static float get_line_gradient(Line_t line);
static int line_goes_right(Line_t line);
static int line_goes_down(Line_t line);

static void update_pixels();
static uint32_t pack_colour_to_uint32(int a, Colour_t colour);
static Colour_t unpack_colour_from_uint32(uint32_t packed_colour);
static void set_pixel(Vec2 coord, Colour_t colour);
static Colour_t get_pixel_colour(Vec2 coord);
static void clear_screen(Colour_t colour);
static void draw_line(Vec3 start, Vec3 end, Colour_t colour);
static void fill_square(Vec3 one, Vec3 two, Vec3 three, Vec3 four, Colour_t new_colour);
static int test_fill_square();
static void add_square(Vec3 top_left);

static Vec3 rotate_and_project(Vec3 coord);

static void update_movement();

static Vec3 camera_pos = {0};
static int speed = 0;
static int walk_speed = 0;
static int sprint_speed = 0;

static Squares_t squares = {0};
static int paused = 0;
static int frame = 0;
static int toggle = 0;

static float rotation = 0;
static Vec2 mouse = {0};
static char keys[32];
static KeyCode w, a, s, d, shift;

static Colour_t red = {255, 0, 0};
static Colour_t green = {0, 255, 0};
static Colour_t blue = {0, 0, 255};

static int debug = 0;
static int debug2 = 0;
static int debug3 = 0;

static Colour_t texture[TEXTURE_WIDTH * TEXTURE_WIDTH] = {0};

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

void init_stuff() {

	camera_pos.x = 0;
	camera_pos.y = 0;
	camera_pos.z = 0;
	speed = 10;
	walk_speed = speed;
	sprint_speed = 20;

	red.r = 255;
	green.g = 255;
	blue.b = 255;
	assert(test_fill_square());

	squares.items = malloc(MAX_SQUARES * sizeof(Square_t));
	add_square((Vec3){-5, -5, 10});

	int swap = 0;
	for (int i = 0; i < TEXTURE_WIDTH * TEXTURE_WIDTH; i++) {
		if (i % TEXTURE_WIDTH == 0) {
			swap = (swap ? 0 : 1);
		}
		if (swap) {
			if (i % 2 == 0) {
				texture[i] = green;
			}
			else {
				texture[i] = blue;
			}
		}
		else {
			if (i % 2 == 0) {
				texture[i] = blue;
			}
			else {
				texture[i] = green;
			}
		}
	}

	return;
}

void update_pixels() {
	if (paused) {
		return;
	}
	frame++;

	update_movement();

	rotation = -((mouse.x / (float)WIDTH) - 0.5) * 2 * 3.141;

    clear_screen((Colour_t){100, 100, 100});

 	for (int i = 0; i < squares.count; i++) {
		int count = 0;
 		for (int j = 0; j < (TEXTURE_WIDTH * (TEXTURE_WIDTH + 1)); j++) {
 			// squares share corners...
 			if (j && ((j + 1) % (TEXTURE_WIDTH + 1)) == 0) {
 				continue;
 			}
 			fill_square(rotate_and_project(squares.items[i].coords[j]),
 						rotate_and_project(squares.items[i].coords[j + 1]),
 						rotate_and_project(squares.items[i].coords[j + TEXTURE_WIDTH + 2]),
 						rotate_and_project(squares.items[i].coords[j + TEXTURE_WIDTH + 1]),
 						texture[count++]);
 		}
 	}
//	static Vec3 one = {WIDTH / 2, HEIGHT / 2, 10};
//	static Vec3 two = {WIDTH / 2, HEIGHT / 2, 10};
//	static Vec3 three = {WIDTH / 2, HEIGHT / 2, 10};
//	static Vec3 four = {WIDTH / 2, HEIGHT / 2, 10};
//	one.x -= 2;
//	one.y -= 1;
//	two.x += 1;
//	two.y -= 2;
//	three.x += 1;
//	three.y += 2;
//	four.x -= 2;
//	four.y += 1;
//	fill_square(one, two, three, four, green);
//	// if x or y becomes negative, our fill_square function fails
//	static Vec3 oone = {WIDTH / 2, HEIGHT / 2, 10};
//	static Vec3 otwo = {WIDTH / 2, HEIGHT / 2, 10};
//	static Vec3 othree = {WIDTH / 2, HEIGHT / 2, 10};
//	static Vec3 ofour = {WIDTH / 2, HEIGHT / 2, 10};
//	if (frame > 1) {
//		oone.x -= 2;
//		oone.y -= 1;
//		otwo.x += 1;
//		otwo.y -= 2;
//		othree.x += 1;
//		othree.y += 2;
//		ofour.x -= 2;
//		ofour.y += 1;
//	}
//	fill_square(oone, otwo, othree, ofour, blue);

	Vec3 left = {0, HEIGHT / 2, 10};
	Vec3 right = {WIDTH, HEIGHT / 2, 10};
	Vec3 top = {WIDTH / 2, 0, 10};
	Vec3 bottom = {WIDTH / 2, HEIGHT, 10};

	draw_line(left, right, green);
	draw_line(top, bottom, green);

	return;
}

void clear_screen(Colour_t colour) {
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            set_pixel((Vec2){x, y}, colour);
        }
    }
	return;
}

uint32_t pack_colour_to_uint32(int a, Colour_t colour) {
    return (a << 24 | colour.r << 16) | (colour.g << 8) | colour.b;
}

Colour_t unpack_colour_from_uint32(uint32_t packed_colour) {
	Colour_t colour = {0};
	colour.r = (packed_colour >> 16) & 255;
	colour.g = (packed_colour >> 8) & 255;
	colour.b = packed_colour & 255;
    return colour;
}

void set_pixel(Vec2 coord, Colour_t colour) {
	// TODO: remove these checks when not debugging as they slow us down
	if ((coord.y < HEIGHT) && (coord.x < WIDTH)) {
		if ((coord.y * WIDTH + coord.x) < (WIDTH * HEIGHT)) {
			pixels[coord.y * WIDTH + coord.x] = pack_colour_to_uint32(1, colour);
			return;
		}
	}
	return;
}

Colour_t get_pixel_colour(Vec2 coord) {
	uint32_t packed_colour = 0;
	if ((coord.y < HEIGHT) && (coord.x < WIDTH)) {
		if ((coord.y * WIDTH + coord.x) < (WIDTH * HEIGHT)) {
			packed_colour = pixels[coord.y * WIDTH + coord.x];
			return unpack_colour_from_uint32(packed_colour);
		}
	}
	debug_log("get_pixel out of bounds");
	return unpack_colour_from_uint32(packed_colour);
}

void draw_line(Vec3 start, Vec3 end, Colour_t colour) {
	Line_t line = {start, end};

	float m = get_line_gradient(line);
	int c = get_line_intercept(line);

	//if (abs(change_in_y) > abs(change_in_x)) {
	if (abs(m) >= 1) {
		if (line_goes_down(line)) {
			for (int y = line.start.y; y <= line.end.y; y++) {
				int x = (y - c) / m;
				set_pixel((Vec2){x, y}, colour);
			}
		}
		else {
			for (int y = line.start.y; y >= line.end.y; y--) {
				int x = (y - c) / m;
				set_pixel((Vec2){x, y}, colour);
			}
		}
	}
	else {
		if (line_goes_right(line)) {
			for (int x = line.start.x; x <= line.end.x; x++) {
				int y = (m * x) + c;
				set_pixel((Vec2){x, y}, colour);
			}
		}
		else {
			for (int x = line.start.x; x >= line.end.x; x--) {
				int y = (m * x) + c;
				set_pixel((Vec2){x, y}, colour);
			}
		}
	}
	return;
}

void fill_square(Vec3 one, Vec3 two, Vec3 three, Vec3 four, Colour_t colour) {
	Vec3 coords[4] = {one, two, three, four};
	// dont draw if z is neg
	if (! one.z) {
		return;
	}

	int smallest_y = HEIGHT + 1;
	int smallest_y_index = 0;
	int largest_y = -1;
	for (int i = 0; i < 4; i++) {
		if (coords[i].y < smallest_y) {
			smallest_y = coords[i].y;
			smallest_y_index = i;
		}
		if (coords[i].y > largest_y) {
			largest_y = coords[i].y;
		}
	}

	Vec3 top = coords[smallest_y_index];
	int left_index = smallest_y_index - 1;
	if (left_index == -1) {
		left_index = 3;
	}
	int right_index = (smallest_y_index + 1) % 4;

	Line_t left_line = {top, coords[left_index]};
	Line_t right_line = {top, coords[right_index]};

	int left_x = left_line.start.x;
	int left_y = left_line.start.y;

	int right_x = right_line.start.x;
	int right_y = right_line.start.y;
	if (smallest_y < 0) {
		smallest_y = 0;
	}
	if (largest_y > HEIGHT) {
		largest_y = HEIGHT;
	}
	for (int y = smallest_y; y < largest_y; y++) {

		// draw left line to y
		left_x = (y - get_line_intercept(left_line)) / get_line_gradient(left_line);
			
		// draw right line to y
		right_x = (y - get_line_intercept(right_line)) / get_line_gradient(right_line);

		if (y >= left_line.end.y) {
			left_index = left_index - 1;
			if (left_index == -1) {
				left_index = 3;
			}

			left_line.start = left_line.end;
			left_line.end = coords[left_index];
		}
		if (y >= right_line.end.y) {
			right_index = (right_index + 1) % 4;

			right_line.start = right_line.end;
			right_line.end = coords[right_index];
		}

		if (right_x > left_x) {
			if (left_x <= 0) {
				continue;
			}
			if (right_x > WIDTH) {
				right_x = WIDTH;
			}
			for (int x = left_x; x < right_x; x++) {
				set_pixel((Vec2){x, y}, colour);
			}
		}
		else {
			if (right_x <= 0) {
				continue;
			}
			if (left_x > WIDTH) {
				left_x = WIDTH;
			}
			for (int x = right_x; x < left_x; x++) {
				set_pixel((Vec2){x, y}, colour);
			}
		}
	}

	return;
}

int test_fill_square() {
	return 1;
}

void debug_log(char *str) {
	printf("\n%s", str);
}

float get_line_gradient(Line_t line) {
	int change_in_y = line.end.y - line.start.y;
	int change_in_x = line.end.x - line.start.x;

	float m = (float)change_in_y / (float)change_in_x;
	if (change_in_x == 0){
		m = WIDTH * 10;	
	}
	return m;
}

int get_line_intercept(Line_t line) {
	int c = line.start.y - (get_line_gradient(line) * line.start.x);
	return c;
}

int line_goes_down(Line_t line) {
	int change_in_y = line.end.y - line.start.y;
	return (change_in_y > 0);
}

int line_goes_right(Line_t line) {
	int change_in_x = line.end.x - line.start.x;
	return (change_in_x > 0);
}

void add_square(Vec3 top_left) {

	// convert from cm to pixels
	int x = top_left.x * CM_TO_PIXELS;
	int y = top_left.y * CM_TO_PIXELS;
	int z = top_left.z * CM_TO_PIXELS;


	Square_t square = {0};
	int count = 0;
	for (int j = 0; j < TEXTURE_WIDTH + 1; j++) {
		for (int k = 0; k < TEXTURE_WIDTH + 1; k++) {
			square.coords[count] = (Vec3){x + (k * (CUBE_WIDTH / TEXTURE_WIDTH)), y + (j * (CUBE_WIDTH / TEXTURE_WIDTH)), z};
			count++;
		}
	}
	squares.items[squares.count] = square;
	squares.count++;

}

Vec3 rotate_and_project(Vec3 coord) {
	// rotate
	coord.x -= camera_pos.x;
	coord.z -= camera_pos.z;
	int x = (coord.z * sin(rotation) + coord.x * cos(rotation));
	int z = (coord.z * cos(rotation) - coord.x * sin(rotation));
	int y = coord.y;
	if (z == 0) {
		z = 0.01;
	}
	float percent_size = ((float)WIDTH_IN_CM / z);
	// project
	if (z > 0) {
		x *= percent_size;
		y *= percent_size;
		z = 1;
	}
	else {
		z = 0;
	}
	// convert from standard grid to screen grid
	x = x + WIDTH / 2;
	y = -y + HEIGHT / 2;
	return (Vec3){x, y, z};
}

void update_movement()
{
	int x = 0;
	int z = 0;
	if (keys[shift / 8] & (1 << (shift % 8))) {
        speed = sprint_speed;
	}
	else {
		speed = walk_speed;
	}
    if (keys[w / 8] & (1 << (w % 8))) {
        x = - speed * sin(rotation);
        z = speed * cos(rotation);
	}
    if (keys[a / 8] & (1 << (a % 8))) {
        x = - speed * cos(rotation);
        z = - speed * sin(rotation);
	}
    if (keys[s / 8] & (1 << (s % 8))) {
        x = speed * sin(rotation);
        z = - speed * cos(rotation);
	}
    if (keys[d / 8] & (1 << (d % 8))) {
        x = speed * cos(rotation);
        z = speed * sin(rotation);
	}
    camera_pos.x += x;
    camera_pos.z += z;
}
