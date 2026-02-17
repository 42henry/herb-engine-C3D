#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>

#define WIDTH  3500
#define HEIGHT 1500

#define CM_TO_PIXELS 10

#define FOV 2
#define WIDTH_IN_CM ((WIDTH) / (CM_TO_PIXELS))

#define CUBE_WIDTH (10 * CM_TO_PIXELS)
#define MAX_SQUARES 5000

#define TEXTURE_WIDTH 3
#define COORDS_PER_SQUARE ((TEXTURE_WIDTH + 1) * (TEXTURE_WIDTH + 1))

#define TARGET_FPS 60
#define FRAME_TIME_NS (1000000000 / TARGET_FPS)

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
	Vec3 coords[4];	
	uint32_t colour;
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
static void set_pixel(Vec2 coord, uint32_t colour);
static Colour_t get_pixel_colour(Vec2 coord);
static void clear_screen(Colour_t colour);
static void draw_line(Vec3 start, Vec3 end, Colour_t colour);
static void fill_square(Square_t *square);
static int test_fill_square();
static void add_cube(Vec3 top_left, Colour_t colour);

static void rotate_and_project_squares();
static void draw_all_squares();

static void update_movement();

int compare_squares(const void *one, const void *two);

static Vec3 camera_pos = {0};
static int speed = 0;
static int walk_speed = 0;
static int sprint_speed = 0;

static Squares_t world_squares = {0};
static Squares_t draw_squares = {0};
static int paused = 0;
static int frame = 0;
static int toggle = 0;

static float x_rotation = 0;
static float y_rotation = 0;
static Vec2 mouse = {0};
static char keys[32];
static KeyCode w, a, s, d, shift, space, control;

static Colour_t red = {255, 0, 0};
static Colour_t green = {0, 255, 0};
static Colour_t blue = {0, 0, 255};

static int debug = 0;
static int debug2 = 0;
static int debug3 = 0;

static Colour_t texture[TEXTURE_WIDTH * TEXTURE_WIDTH] = {0};

struct timespec last, now;

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
    space = XKeysymToKeycode(display, XK_space);
    control = XKeysymToKeycode(display, XK_Control_L);

	clock_gettime(CLOCK_MONOTONIC, &last);

    while (1) {
		// --- Handle events ---
		while (XPending(display)) {
			XEvent event;
			XNextEvent(display, &event);

			if (event.type == Expose) {
				// redraw if needed
			}
			if (event.type == DestroyNotify) {
				return 0;
			}
		}

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

		// --- Update ---
		update_pixels();

		// --- Render ---
		XPutImage(display, window, gc, image, 0, 0, 0, 0, WIDTH, HEIGHT);
		XFlush(display);

		// --- Frame timing ---
		clock_gettime(CLOCK_MONOTONIC, &now);

		long elapsed =
			(now.tv_sec - last.tv_sec) * 1000000000L +
			(now.tv_nsec - last.tv_nsec);

		if (elapsed < FRAME_TIME_NS) {
			struct timespec sleep_time;
			sleep_time.tv_sec = 0;
			sleep_time.tv_nsec = FRAME_TIME_NS - elapsed;
			nanosleep(&sleep_time, NULL);
		}

		clock_gettime(CLOCK_MONOTONIC, &last);
    }

    // Cleanup
	free(world_squares.items);
	free(draw_squares.items);
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

	world_squares.items = malloc(MAX_SQUARES * sizeof(Square_t));
	draw_squares.items = malloc(MAX_SQUARES * sizeof(Square_t));

	add_cube((Vec3){-50, 50, 10}, green);
	add_cube((Vec3){50, 50, 10}, blue);
	add_cube((Vec3){50, 150, 10}, red);

	Colour_t c = blue;
	for (int i = 0; i < 10; i++) {
		add_cube((Vec3){50 + (i * CUBE_WIDTH), 50, 10}, c);
		if (i % 2 == 0) {
			add_cube((Vec3){50 + (i * CUBE_WIDTH), 50, 10 + (2 * CUBE_WIDTH)}, c);
			c = green;
		}
		if (i % 3 == 0) {
			add_cube((Vec3){50 + (i * CUBE_WIDTH), 50, 10 + (CUBE_WIDTH)}, c);
			c = red;
		}
		if (i % 4 == 0) {
			add_cube((Vec3){50 + (i * CUBE_WIDTH), 50, 10 + (CUBE_WIDTH)}, c);
			c = blue;
		}
	}

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
				texture[i] = red;
			}
		}
		else {
			if (i % 2 == 0) {
				texture[i] = red;
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

	x_rotation = -((mouse.x / (float)WIDTH) - 0.5) * 2 * 3.141;
	y_rotation = ((mouse.y / (float)HEIGHT) - 0.5) * 2 * 3.141;

    clear_screen((Colour_t){100, 100, 100});

	rotate_and_project_squares();

	qsort(draw_squares.items, draw_squares.count, sizeof(Square_t), compare_squares);

	draw_all_squares();
	return;
}

void clear_screen(Colour_t colour) {
	uint32_t c = pack_colour_to_uint32(1, colour);
	memset(pixels, c, WIDTH * HEIGHT * sizeof(uint32_t));
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

void set_pixel(Vec2 coord, uint32_t colour) {
	// TODO: remove these checks when not debugging as they slow us down
	if ((coord.y < HEIGHT) && (coord.x < WIDTH)) {
		if ((coord.y * WIDTH + coord.x) < (WIDTH * HEIGHT)) {
			pixels[coord.y * WIDTH + coord.x] = colour;
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
				set_pixel((Vec2){x, y}, pack_colour_to_uint32(1, colour));
			}
		}
		else {
			for (int y = line.start.y; y >= line.end.y; y--) {
				int x = (y - c) / m;
				set_pixel((Vec2){x, y}, pack_colour_to_uint32(1, colour));
			}
		}
	}
	else {
		if (line_goes_right(line)) {
			for (int x = line.start.x; x <= line.end.x; x++) {
				int y = (m * x) + c;
				set_pixel((Vec2){x, y}, pack_colour_to_uint32(1, colour));
			}
		}
		else {
			for (int x = line.start.x; x >= line.end.x; x--) {
				int y = (m * x) + c;
				set_pixel((Vec2){x, y}, pack_colour_to_uint32(1, colour));
			}
		}
	}
	return;
}

void fill_square(Square_t *square) {

	int smallest_y = HEIGHT + 1;
	int smallest_y_index = 0;
	int largest_y = -1;
	for (int i = 0; i < 4; i++) {
		if (square->coords[i].z == 0) {
			return;
		}
		if (square->coords[i].y < smallest_y) {
			smallest_y = square->coords[i].y;
			smallest_y_index = i;
		}
		if (square->coords[i].y > largest_y) {
			largest_y = square->coords[i].y;
		}
	}

	int left_start_index = smallest_y_index;
	int left_end_index = smallest_y_index - 1;
	if (left_end_index == -1) {
		left_end_index = 3;
	}

	int right_start_index = smallest_y_index;
	int right_end_index = (smallest_y_index + 1) % 4;

	int left_x = square->coords[left_start_index].x;
	int left_y = square->coords[left_start_index].y;

	int right_x = left_x;
	int right_y = right_x;
	if (smallest_y < 0) {
		smallest_y = 0;
	}
	if (largest_y > HEIGHT) {
		largest_y = HEIGHT;
	}
	for (int y = smallest_y; y < largest_y; y++) {
		// help avoid cache misses:
		uint32_t* row = &pixels[y * WIDTH];

		// get x coords y using y = mx + c
// 		int y1 = square->coords[left_start_index].y;
// 		int y2 = square->coords[left_end_index].y;
// 		int x1 = square->coords[left_start_index].x;
// 		int x2 = square->coords[left_end_index].x;
//		left_x = x1 + (((y - y1) * (x2 - x1)) / (y2 - y1));

		// if ((y2 - y1) == 0) the draw the line, and move on to the next left line
		if ((square->coords[left_end_index].y - square->coords[left_start_index].y) == 0) {
			if (square->coords[left_end_index].x > square->coords[left_start_index].x) {
				for (int x = square->coords[left_start_index].x; x < square->coords[left_end_index].x; x++) {
					if (x > -1 && x < WIDTH) {
						row[x] = square->colour;
					}
				}
			}
			else {
				for (int x = square->coords[left_end_index].x; x < square->coords[left_start_index].x; x++) {
					if (x > -1 && x < WIDTH) {
						row[x] = square->colour;
					}
				}
			}
			left_start_index = left_end_index;
			left_end_index = left_end_index - 1;
			if (left_end_index == -1) {
				left_end_index = 3;
			}
			continue;
		}

		left_x = square->coords[left_start_index].x + (((y - square->coords[left_start_index].y) * (square->coords[left_end_index].x - square->coords[left_start_index].x)) / (square->coords[left_end_index].y - square->coords[left_start_index].y));
			
//		y1 = square->coords[right_start_index].y;
//		y2 = square->coords[right_end_index].y;
//		x1 = square->coords[right_start_index].x;
//		x2 = square->coords[right_end_index].x;
		// if ((y2 - y1) == 0) the draw the line, and move on to the next right line
		if ((square->coords[right_end_index].y - square->coords[right_start_index].y) == 0) {
			if (square->coords[right_end_index].x > square->coords[right_start_index].x) {
				for (int x = square->coords[right_start_index].x; x < square->coords[right_end_index].x; x++) {
					if (x > -1 && x < WIDTH) {
						row[x] = square->colour;
					}
				}
			}
			else {
				for (int x = square->coords[right_end_index].x; x < square->coords[right_start_index].x; x++) {
					if (x > -1 && x < WIDTH) {
						row[x] = square->colour;
					}
				}
			}
			right_start_index = right_end_index;
			right_end_index = (right_end_index + 1) % 4;
			continue;
		}

//		right_x = x1 + (((y - y1) * (x2 - x1)) / (y2 - y1));
		right_x = square->coords[right_start_index].x + (((y - square->coords[right_start_index].y) * (square->coords[right_end_index].x - square->coords[right_start_index].x)) / (square->coords[right_end_index].y - square->coords[right_start_index].y));

		if (y >= square->coords[left_end_index].y) {

			left_start_index = left_end_index;
			left_end_index = left_end_index - 1;
			if (left_end_index == -1) {
				left_end_index = 3;
			}
		}
		if (y >= square->coords[right_end_index].y) {

			right_start_index = right_end_index;
			right_end_index = (right_end_index + 1) % 4;
		}

		// connect left x and right x
		if (right_x > left_x) {
			if (left_x <= 0) {
				left_x = 0;
			}
			if (right_x > WIDTH) {
				right_x = WIDTH;
			}
			for (int x = left_x; x < right_x; x++) {
				row[x] = square->colour;
			}
		}
		else {
			if (right_x <= 0) {
				right_x = 0;
			}
			if (left_x > WIDTH) {
				left_x = WIDTH;
			}
			for (int x = right_x; x < left_x; x++) {
				row[x] = square->colour;
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

void add_cube(Vec3 top_left, Colour_t colour) {
	int x = top_left.x;
	int y = top_left.y;
	int z = top_left.z;

	int len = CUBE_WIDTH / TEXTURE_WIDTH;

	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};
			square.coords[0] = (Vec3) {x + (i * len), y - (j * len), z};
			square.coords[1] = (Vec3) {x + ((i + 1) * len), y - (j * len), z};
			square.coords[2] = (Vec3) {x + ((i + 1) * len), y - ((j + 1) * len), z};
			square.coords[3] = (Vec3) {x + (i * len), y - ((j + 1) * len), z};

			square.colour = pack_colour_to_uint32(1, colour);

			world_squares.items[world_squares.count++] = square;
		}
	}
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};

			square.coords[0] = (Vec3) {x, y, z + CUBE_WIDTH};
			square.coords[1] = (Vec3) {x + ((i + 1) * len), y, z + CUBE_WIDTH};
			square.coords[2] = (Vec3) {x + ((i + 1) * len), y - ((j + 1) * len), z + CUBE_WIDTH};
			square.coords[3] = (Vec3) {x, y - ((j + 1) * len), z + CUBE_WIDTH};

			square.colour = pack_colour_to_uint32(1, colour);

			world_squares.items[world_squares.count++] = square;
		}
	}
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};

			square.coords[0] = (Vec3) {x, y, z};
			square.coords[1] = (Vec3) {x, y, z + ((i + 1) * len)};
			square.coords[2] = (Vec3) {x, y - ((j + 1) * len), z + ((i + 1) * len)};
			square.coords[3] = (Vec3) {x, y - ((j + 1) * len), z};

			square.colour = pack_colour_to_uint32(1, colour);

			world_squares.items[world_squares.count++] = square;
		}
	}
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};

			square.coords[0] = (Vec3) {x + CUBE_WIDTH, y, z};
			square.coords[1] = (Vec3) {x + CUBE_WIDTH, y, z + ((i + 1) * len)};
			square.coords[2] = (Vec3) {x + CUBE_WIDTH, y - ((j + 1) * len), z + ((i + 1) * len)};
			square.coords[3] = (Vec3) {x + CUBE_WIDTH, y - ((j + 1) * len), z};

			square.colour = pack_colour_to_uint32(1, colour);

			world_squares.items[world_squares.count++] = square;
		}
	}
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};

			square.coords[0] = (Vec3) {x, y, z};
			square.coords[1] = (Vec3) {x + ((i + 1) * len), y, z};
			square.coords[2] = (Vec3) {x + ((i + 1) * len), y, z + ((j + 1) * len)};
			square.coords[3] = (Vec3) {x, y, z + ((j + 1) * len)};

			square.colour = pack_colour_to_uint32(1, colour);

			world_squares.items[world_squares.count++] = square;
		}
	}
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};

			square.coords[0] = (Vec3) {x, y - CUBE_WIDTH, z};
			square.coords[1] = (Vec3) {x + ((i + 1) * len), y - CUBE_WIDTH, z};
			square.coords[2] = (Vec3) {x + ((i + 1) * len), y - CUBE_WIDTH, z + ((i + 1) * len)};
			square.coords[3] = (Vec3) {x, y - CUBE_WIDTH, z + ((i + 1) * len)};

			square.colour = pack_colour_to_uint32(1, colour);

			world_squares.items[world_squares.count++] = square;
		}
	}
	return;
}

void rotate_and_project_squares() {
	// rotate
	for (int i = 0; i < world_squares.count; i++) {
		for (int j = 0; j < 4; j++) {
			int x = world_squares.items[i].coords[j].x;
			int y = world_squares.items[i].coords[j].y;
			int z = world_squares.items[i].coords[j].z;
			x -= camera_pos.x;
			y -= camera_pos.y;
			z -= camera_pos.z;
			int new_x = (z * sin(x_rotation) + x * cos(x_rotation));
			int z2 = (z * cos(x_rotation) - x * sin(x_rotation));

			int new_y = (z2 * sin(y_rotation) + y * cos(y_rotation));
			int new_z = (z2 * cos(y_rotation) - y * sin(y_rotation));
			if (new_z == 0) {
				new_z = 0.01;
			}
			float percent_size = ((float)(WIDTH_IN_CM * FOV) / new_z);
			// project
			if (new_z > 0) {
				new_x *= percent_size;
				new_y *= percent_size;
			}
			else {
				new_z = 0;
			}
			// convert from standard grid to screen grid
			new_x = new_x + WIDTH / 2;
			new_y = -new_y + HEIGHT / 2;
			draw_squares.items[i].coords[j].x = new_x;
			draw_squares.items[i].coords[j].y = new_y;
			draw_squares.items[i].coords[j].z = new_z;
			draw_squares.items[i].colour = world_squares.items[i].colour;
		}
	}
	draw_squares.count = world_squares.count;
}

void update_movement()
{
	int x = 0;
	int z = 0;
	int y = 0;
	if (keys[control / 8] & (1 << (control % 8))) {
        speed = sprint_speed;
	}
	else {
		speed = walk_speed;
	}
    if (keys[w / 8] & (1 << (w % 8))) {
        x = - speed * sin(x_rotation);
        z = speed * cos(x_rotation);
	}
    if (keys[a / 8] & (1 << (a % 8))) {
        x = - speed * cos(x_rotation);
        z = - speed * sin(x_rotation);
	}
    if (keys[s / 8] & (1 << (s % 8))) {
        x = speed * sin(x_rotation);
        z = - speed * cos(x_rotation);
	}
    if (keys[d / 8] & (1 << (d % 8))) {
        x = speed * cos(x_rotation);
        z = speed * sin(x_rotation);
	}
    if (keys[space / 8] & (1 << (space % 8))) {
        y = speed;
	}
    if (keys[shift / 8] & (1 << (shift % 8))) {
        y = - speed;
	}
    camera_pos.x += x;
    camera_pos.y += y;
    camera_pos.z += z;
}

void draw_all_squares() {
	for (int i = 0; i < draw_squares.count; i++) {
		fill_square(&draw_squares.items[i]);
	}
}

int compare_squares(const void *one, const void *two) {
	const Square_t *square_one = one;
	const Square_t *square_two = two;

	float z1 = square_one->coords[0].z;
	for (int i = 1; i < 4; i++) {
		z1 += square_one->coords[i].z;
	}
	z1 /= 4;

	float z2 = square_two->coords[0].z;
	for (int i = 1; i < 4; i++) {
		z2 += square_two->coords[i].z;
	}
	z2 /= 4;

	if (z1 > z2) {
		return -1;
	}
	if (z1 < z2) {
		return 1;
	}

	return 0;
}
