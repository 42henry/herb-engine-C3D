#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>

//TODO: improve collisions

//TODO: don't draw if !every! z of square is < 0

//TODO: make movement velocity based

//TODO: highlight the cube under cursor

//TODO: terrain generation

//TODO: clean the code, especially rotate_and_project()

//TODO: make it work on windows - need to get keys/keycodes correct, and use timespec to set frame rate similarly, and get mouse.x and y and left click and right click etc

//TODO: don't draw squares that are behind other squares

#define WIDTH  3500
#define HEIGHT 1500

#define CM_TO_PIXELS 10

#define FOV 2
#define WIDTH_IN_CM ((WIDTH) / (CM_TO_PIXELS))

#define CUBE_WIDTH (10 * CM_TO_PIXELS)
#define MAX_SQUARES 100000

#define TEXTURE_WIDTH 5

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
	unsigned char r;
	unsigned char g;
	unsigned char b;
} Colour_t;

typedef struct {
    int width, height;
    Colour_t *data;
} Texture_t;

typedef struct {
	Vec3 coords[4];	
	uint32_t colour;
	int r; // distance from camera
} Square_t;

typedef struct {
	Square_t *items;
	int count;
} Squares_t;

static uint32_t *pixels = NULL;

static void init_stuff();
static void cleanup();
static void debug_log(char *str);

static int get_line_intercept(Line_t line);
static float get_line_gradient(Line_t line);
static int line_goes_right(Line_t line);
static int line_goes_down(Line_t line);

static void update();
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
static void remove_cube(int index);
static void place_cube(int index);

static void rotate_and_project_squares();
static void draw_all_squares();

static void handle_input();

int compare_squares(const void *one, const void *two);

void writePPM(const char *filename, Texture_t *img);
Texture_t* readPPM(const char *filename);

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
static int mouse_left_click = 0;
static int mouse_was_left_clicked = 0;
static int mouse_right_click = 0;
static int mouse_was_right_clicked = 0;
static int mouse_defined = 0;
static int keys[256] = {0};
static unsigned char w, a, s, d, shift, space, control, escape;

static Colour_t red = {255, 0, 0};
static Colour_t green = {0, 255, 0};
static Colour_t blue = {0, 0, 255};

static Colour_t texture[TEXTURE_WIDTH * TEXTURE_WIDTH] = {0};

Texture_t *grass_texture = NULL;
static int using_texture = 0;

static int hold_mouse = 1;
static float mouse_sensitivity = 0.005f;

struct timespec last, now;

static int central_cube_index;

static int cube_highlighted = 0;

static int dlog = 0;

void init_stuff() {
	srand(time(NULL));
	cube_highlighted = -1;
	mouse_sensitivity = 0.005f;
	// create grass texture
	// * 3 for top bottom side
    int w = TEXTURE_WIDTH * 3, h = TEXTURE_WIDTH;
    Texture_t myImg = {w, h, malloc(w * h * 3)};

    Colour_t dark_green = {10, 180, 20};
    Colour_t brown = {150, 75, 10};

	// top
	int i = 0;
	for (i; i < TEXTURE_WIDTH * TEXTURE_WIDTH; i++) {
		myImg.data[i] = (Colour_t){10 + (rand() % 50), 180 + (rand() % 50), 20 + (rand() % 50), };
	}
	// bottom
	for (i; i < 2 * TEXTURE_WIDTH * TEXTURE_WIDTH; i++) {
		myImg.data[i] = (Colour_t){150 + (rand() % 50), 75 + (rand() % 50), 10 + (rand() % 50), };
	}
	// side
	for (i; i < 3 * TEXTURE_WIDTH * TEXTURE_WIDTH; i++) {
		if (i < 2.5 * TEXTURE_WIDTH * TEXTURE_WIDTH) {
			myImg.data[i] = (Colour_t){10 + (rand() % 50), 180 + (rand() % 50), 20 + (rand() % 50), };
		}
		else {
			myImg.data[i] = (Colour_t){150 + (rand() % 50), 75 + (rand() % 50), 10 + (rand() % 50), };
		}
	}

    writePPM("grass.ppm", &myImg);

    free(myImg.data);

	grass_texture = readPPM("grass.ppm");
	assert(grass_texture != NULL);

	using_texture = 1;

	hold_mouse = 1;

	camera_pos.x = 0;
	camera_pos.y = 300;
	camera_pos.z = 100;
	speed = 10;
	walk_speed = speed;
	sprint_speed = 20;

	red.r = 255;
	green.g = 255;
	blue.b = 255;
	assert(test_fill_square());

	world_squares.items = malloc(MAX_SQUARES * sizeof(Square_t));
	draw_squares.items = malloc(MAX_SQUARES * sizeof(Square_t));

	add_cube((Vec3){50, 150, 10}, red);
	add_cube((Vec3){100 + 50, 150, 10}, red);
	add_cube((Vec3){200 + 50, 150, 10}, red);
	add_cube((Vec3){300 + 50, 150, 10}, red);

	Colour_t c = blue;
	//for (int i = -5; i < 20; i++) {
		//for (int j = 0; j < 20; j++) {
			//add_cube((Vec3){50 + (i * CUBE_WIDTH), 50, 10 + (j * CUBE_WIDTH)}, c);
		//}
	//}

	return;
}

void cleanup() {
	free(world_squares.items);
	free(draw_squares.items);
    free(pixels);
}

void update() {
	frame++;

	handle_input();

	update_pixels();
}

void update_pixels() {
	if (paused) {
		return;
	}
    clear_screen((Colour_t){50, 180, 255});

	rotate_and_project_squares();

	qsort(draw_squares.items, draw_squares.count, sizeof(Square_t), compare_squares);

	draw_all_squares();

	Square_t centre = {0};
	centre.coords[0].x = WIDTH / 2 - 3;
	centre.coords[0].y = HEIGHT / 2 - 3;
	centre.coords[0].z = 1;
	centre.coords[1].x = WIDTH / 2 + 3;
	centre.coords[1].y = HEIGHT / 2 - 3;
	centre.coords[1].z = 1;
	centre.coords[2].x = WIDTH / 2 + 3;
	centre.coords[2].y = HEIGHT / 2 + 3;
	centre.coords[2].z = 1;
	centre.coords[3].x = WIDTH / 2 - 3;
	centre.coords[3].y = HEIGHT / 2 + 3;
	centre.coords[3].z = 1;

	centre.colour = pack_colour_to_uint32(1, (Colour_t){255, 255, 255});

	fill_square(&centre);

	return;
}

void clear_screen(Colour_t colour) {
	uint32_t c = pack_colour_to_uint32(1, colour);
	for (int x = 0; x < WIDTH; x++) {
		for (int y = 0; y < HEIGHT; y++) {
			pixels[y * WIDTH + x] = c;
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

void set_pixel(Vec2 coord, uint32_t colour) {
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

	// TODO: could we use y+=2 or smthn instead of y++ here to make it faster?
	for (int y = smallest_y; y < largest_y; y++) {
		uint32_t* row = &pixels[y * WIDTH];

		// get x coords y using y = mx + c
// 		int y1 = square->coords[left_start_index].y;
// 		int y2 = square->coords[left_end_index].y;
// 		int x1 = square->coords[left_start_index].x;
// 		int x2 = square->coords[left_end_index].x;
//		left_x = x1 + (((y - y1) * (x2 - x1)) / (y2 - y1));

		// if ((y2 - y1) == 0) the draw the line, and move on to the next left line
		if ((square->coords[left_end_index].y - square->coords[left_start_index].y) == 0) {
			int x1 = square->coords[left_start_index].x;
			int x2 = square->coords[left_end_index].x;
			if (x2 > x1) {
				if (x1 < 0) {
					x1 = 0;
				}
				if (x2 > WIDTH) {
					x2 = WIDTH;
				}
				// TODO: might need to add check here if x1 < x2
				for (int x = x1; x < x2; x++) {
					row[x] = square->colour;
				}
			}
			else {
				if (x2 < 0) {
					x2 = 0;
				}
				if (x1 > WIDTH) {
					x1 = WIDTH;
				}
				// TODO: might need to add check here if x2 < x1
				for (int x = x2; x < x1; x++) {
					row[x] = square->colour;
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
			int x1 = square->coords[right_start_index].x;
			int x2 = square->coords[right_end_index].x;
			if (x2 > x1) {
				if (x1 < 0) {
					x1 = 0;
				}
				if (x2 > WIDTH) {
					x2 = WIDTH;
				}
				// TODO: might need to add check here if x1 < x2
				for (int x = x1; x < x2; x++) {
					row[x] = square->colour;
				}
			}
			else {
				if (x2 < 0) {
					x2 = 0;
				}
				if (x1 > WIDTH) {
					x1 = WIDTH;
				}
				// TODO: might need to add check here if x2 < x1
				for (int x = x2; x < x1; x++) {
					row[x] = square->colour;
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
			// TODO: might need to recheck if left_x < right_x in case where left_x was already larger than width...
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
			// TODO: might need to recheck if right_x < left_x
			for (int x = right_x; x < left_x; x++) {
				row[x] = square->colour;
			}
		}
	}

	return;
}

int test_fill_square() {
	// TODO
	return 1;
}

void debug_log(char *str) {
	printf("\n%s", str);
}

float get_line_gradient(Line_t line) {
	int change_in_y = line.end.y - line.start.y;
	int change_in_x = line.end.x - line.start.x;

	if (change_in_x == 0){
		float m = WIDTH * 10;	
		return m;
	}
	float m = (float)change_in_y / (float)change_in_x;
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

	int texture_side = TEXTURE_WIDTH * TEXTURE_WIDTH;

	// front
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};
			square.coords[0] = (Vec3) {x + (i * len), y - (j * len), z};
			square.coords[1] = (Vec3) {x + ((i + 1) * len), y - (j * len), z};
			square.coords[2] = (Vec3) {x + ((i + 1) * len), y - ((j + 1) * len), z};
			square.coords[3] = (Vec3) {x + (i * len), y - ((j + 1) * len), z};

			if (using_texture) {
				// 2, as the side face textures are the 2nd square of the texture image
				Colour_t c = grass_texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
				square.colour = pack_colour_to_uint32(1, c);
			}
			else {
				square.colour = pack_colour_to_uint32(1, colour);
			}

			// set r to 1 to signify the start of a cube
			world_squares.items[world_squares.count] = square;
			if (i == 0 && j == 0) {
				world_squares.items[world_squares.count].r = 1;
			}
			world_squares.count++;
		}
	}
	// back
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};

			square.coords[0] = (Vec3) {x + (i * len), y - (j * len), z + CUBE_WIDTH};
			square.coords[1] = (Vec3) {x + ((i + 1) * len), y - (j * len), z + CUBE_WIDTH};
			square.coords[2] = (Vec3) {x + ((i + 1) * len), y - ((j + 1) * len), z + CUBE_WIDTH};
			square.coords[3] = (Vec3) {x + (i * len), y - ((j + 1) * len), z + CUBE_WIDTH};

			if (using_texture) {
				Colour_t c = grass_texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
				square.colour = pack_colour_to_uint32(1, c);
			}
			else {
				square.colour = pack_colour_to_uint32(1, colour);
			}

			world_squares.items[world_squares.count++] = square;
		}
	}
	// left
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};

			square.coords[0] = (Vec3) {x, y - (j * len), z + (i * len)};
			square.coords[1] = (Vec3) {x, y - (j * len), z + ((i + 1) * len)};
			square.coords[2] = (Vec3) {x, y - ((j + 1) * len), z + ((i + 1) * len)};
			square.coords[3] = (Vec3) {x, y - ((j + 1) * len), z + (i * len)};

			if (using_texture) {
				Colour_t c = grass_texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
				square.colour = pack_colour_to_uint32(1, c);
			}
			else {
				square.colour = pack_colour_to_uint32(1, colour);
			}

			world_squares.items[world_squares.count++] = square;
		}
	}
	// right
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};

			square.coords[0] = (Vec3) {x + CUBE_WIDTH, y - (j * len), z + (i * len)};
			square.coords[1] = (Vec3) {x + CUBE_WIDTH, y - (j * len), z + ((i + 1) * len)};
			square.coords[2] = (Vec3) {x + CUBE_WIDTH, y - ((j + 1) * len), z + ((i + 1) * len)};
			square.coords[3] = (Vec3) {x + CUBE_WIDTH, y - ((j + 1) * len), z + (i * len)};

			if (using_texture) {
				Colour_t c = grass_texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
				square.colour = pack_colour_to_uint32(1, c);
			}
			else {
				square.colour = pack_colour_to_uint32(1, colour);
			}

			world_squares.items[world_squares.count++] = square;
		}
	}
	// top
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};

			square.coords[0] = (Vec3) {x + (i * len), y, z + (j * len)};
			square.coords[1] = (Vec3) {x + ((i + 1) * len), y, z + (j * len)};
			square.coords[2] = (Vec3) {x + ((i + 1) * len), y, z + ((j + 1) * len)};
			square.coords[3] = (Vec3) {x + (i * len), y, z + ((j + 1) * len)};

			if (using_texture) {
				// 0, as the top face textures are the first square of the texture image
				Colour_t c = grass_texture->data[j * TEXTURE_WIDTH + i];
				square.colour = pack_colour_to_uint32(1, c);
			}
			else {
				square.colour = pack_colour_to_uint32(1, colour);
			}

			world_squares.items[world_squares.count++] = square;
		}
	}
	// bottom
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			Square_t square = {0};

			square.coords[0] = (Vec3) {x + (i * len), y - CUBE_WIDTH, z + (j * len)};
			square.coords[1] = (Vec3) {x + ((i + 1) * len), y - CUBE_WIDTH, z + (j * len)};
			square.coords[2] = (Vec3) {x + ((i + 1) * len), y - CUBE_WIDTH, z + ((j + 1) * len)};
			square.coords[3] = (Vec3) {x + (i * len), y - CUBE_WIDTH, z + ((j + 1) * len)};

			if (using_texture) {
				// 1, as the top face textures are the first square of the texture image
				Colour_t c = grass_texture->data[1 * texture_side + j * TEXTURE_WIDTH + i];
				square.colour = pack_colour_to_uint32(1, c);
			}
			else {
				square.colour = pack_colour_to_uint32(1, colour);
			}

			world_squares.items[world_squares.count++] = square;
		}
	}
	return;
}

void remove_cube(int index) {
	int num_of_squares = TEXTURE_WIDTH * TEXTURE_WIDTH * 6;

	int count = index;
	for (int i = index + num_of_squares; i < world_squares.count; i++) {
		world_squares.items[count] = world_squares.items[i];
		count++;	
	}
	world_squares.count -= num_of_squares;
	return;
}

void place_cube(int index) {
	Vec3 pos = {0};
	pos.x = world_squares.items[index].coords[0].x;
	pos.y = world_squares.items[index].coords[0].y;
	pos.z = world_squares.items[index].coords[0].z;
	switch (cube_highlighted) {
		// front
		case 0: {
			pos.z -= CUBE_WIDTH;
			break;
		}
		// back
		case 1: {
			pos.z += CUBE_WIDTH;
			break;
		}
		// left
		case 2: {
		    pos.x -= CUBE_WIDTH;
			break;
		}
		// right
		case 3: {
		    pos.x += CUBE_WIDTH;
			break;
		}
		// top
		case 4: {
		    pos.y += CUBE_WIDTH;
			break;
		}
		// bottom
		case 5: {
		    pos.y -= CUBE_WIDTH;
			break;
		}
	}
	add_cube(pos, red);
	return;
}

void rotate_and_project_squares() {
	int closest_r = 99999999;
	cube_highlighted = -1;
	for (int i = 0; i < world_squares.count; i++) {

		// distance to camera = r
		int x1 = 0;
		int y1 = 0;
		int z1 = 0;

		for (int j = 0; j < 4; j++) {
			int x = world_squares.items[i].coords[j].x;
			int y = world_squares.items[i].coords[j].y;
			int z = world_squares.items[i].coords[j].z;
			x -= camera_pos.x;
			y -= camera_pos.y;
			z -= camera_pos.z;

			if (j == 0) {
				x1 += x;
				y1 += y;
				z1 += z;
			}
			if (j == 2) {
				x1 += x;
				x1 /= 2;
				y1 += y;
				y1 /= 2;
				z1 += z;
				z1 /= 2;
			}

			int new_x = (z * sin(x_rotation) + x * cos(x_rotation));
			int z2 = (z * cos(x_rotation) - x * sin(x_rotation));

			int new_y = (z2 * sin(y_rotation) + y * cos(y_rotation));
			int new_z = (z2 * cos(y_rotation) - y * sin(y_rotation));
			if (new_z == 0) {
				new_z = 1;
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

		int r = sqrt((x1 * x1) + (y1 * y1) + (z1 * z1));
		draw_squares.items[i].r = r;

		// check if the square just drawn surrounds 0,0
		int top_left_x = WIDTH;
		int bottom_right_x = -WIDTH;

		int top_left_y = HEIGHT;
		int bottom_right_y = -HEIGHT;
		for (int j = 0; j < 4; j++) {
			if (draw_squares.items[i].coords[j].x < top_left_x) {
				top_left_x = draw_squares.items[i].coords[j].x;
			}	
			if (draw_squares.items[i].coords[j].x > bottom_right_x) {
				bottom_right_x = draw_squares.items[i].coords[j].x;
			}	
			if (draw_squares.items[i].coords[j].y < top_left_y) {
				top_left_y = draw_squares.items[i].coords[j].y;
			}	
			if (draw_squares.items[i].coords[j].y > bottom_right_y) { bottom_right_y = draw_squares.items[i].coords[j].y;
			}	
		}

		if (top_left_x <= WIDTH / 2 && bottom_right_x >= WIDTH / 2 && top_left_y <= HEIGHT / 2 && bottom_right_y >= HEIGHT / 2) {
			// if it is, check that square.r is closest r
			if (draw_squares.items[i].r < closest_r) {
				closest_r = draw_squares.items[i].r;

				// find the first square in this cube:
				int index = i;
				int min = i - TEXTURE_WIDTH * TEXTURE_WIDTH * 6;
				for (index; index--; index > min) {
					if (world_squares.items[index].r == 1) {
						break;
					}
				}
				cube_highlighted = 0;
				int face_index = i - index;
				int squares_per_face = TEXTURE_WIDTH * TEXTURE_WIDTH;
				// 0 = front
				// 1 = back
				// 2 = right
				// 3 = left
				// 4 = top
				// 5 = bottom
				cube_highlighted = face_index / squares_per_face;
				central_cube_index = index;
				draw_squares.items[i].colour = pack_colour_to_uint32(1, red);
			}
		}
	}
	draw_squares.count = world_squares.count;
}

void handle_input()
{
	if (mouse_left_click) {
		hold_mouse = 1;
		mouse_was_left_clicked = 1;
	}
	else {
		if (mouse_was_left_clicked) {
			if (cube_highlighted > -1) {
				remove_cube(central_cube_index);
			}
			dlog = (dlog ? 0 : 1);
		}
		mouse_was_left_clicked = 0;
	}
	if (mouse_right_click) {
		mouse_was_right_clicked = 1;
	}
	else {
		if (mouse_was_right_clicked) {
			if (cube_highlighted > -1) {
				place_cube(central_cube_index);
			}
		}
		mouse_was_right_clicked = 0;
	}
	if (hold_mouse) {
		int dx = mouse.x - (WIDTH / 2);
		int dy = mouse.y - (HEIGHT / 2);

		x_rotation -= dx * mouse_sensitivity;
		y_rotation += dy * mouse_sensitivity;

		// Clamp vertical rotation
		if (y_rotation > 3.141/2) y_rotation = 3.141/2;
		if (y_rotation < -3.141/2) y_rotation = -3.141/2;

	}
	int x = 0;
	int z = 0;
	int y = 0;
	if (keys[control]) {
        speed = sprint_speed;
	}
	else {
		speed = walk_speed;
	}
	// TODO: add y here too
    if (keys[w]) {
        x += - speed * sin(x_rotation);
        z += speed * cos(x_rotation);
	}
    if (keys[a]) {
        x += - speed * cos(x_rotation);
        z += - speed * sin(x_rotation);
	}
    if (keys[s]) {
        x += speed * sin(x_rotation);
        z += - speed * cos(x_rotation);
	}
    if (keys[d]) {
        x += speed * cos(x_rotation);
        z += speed * sin(x_rotation);
	}
    if (keys[space]) {
        y += 20;
	}
    if (keys[shift]) {
        y += - speed;
	}
	if (keys[escape]) {
		hold_mouse = 0;
	}

    // gravity:
	//y -= 10;

    camera_pos.x += x;
	
	// check collisions:
	// collisions:
	int n = TEXTURE_WIDTH * TEXTURE_WIDTH * 6;
	for (int i = 0; i < world_squares.count; i += n) {
		// x1 = top left front
		int x1 = world_squares.items[i].coords[0].x;
		int y1 = world_squares.items[i].coords[0].y;
		int z1 = world_squares.items[i].coords[0].z;

		// x2 = bottom right back
		int x2 = x1 + CUBE_WIDTH;	
		int y2 = y1 - CUBE_WIDTH;	
		int z2 = z1 + CUBE_WIDTH;

		// let's start by saying the player is cube
		int player_width = CUBE_WIDTH / 2;
		int player_height = CUBE_WIDTH / 2;

		// player_x1 = top left front
		int player_x1 = camera_pos.x - player_width;
		int player_y1 = camera_pos.y + player_width;
		int player_z1 = camera_pos.z - player_width;

		// player_x1 = bottom right back
		int player_x2 = camera_pos.x + player_width;
		int player_y2 = camera_pos.y - player_width;
		int player_z2 = camera_pos.z + player_width;

		// TODO: keep track of which ones were previous collisions
		int x_collision = 0;
		int y_collision = 0;
		int z_collision = 0;
		if ((player_x1 >= x1 && player_x1 <= x2) || (player_x2 >= x1 && player_x2 <= x2)) {
			// xs overlap
			x_collision = 1;
		}
		if ((player_y1 <= y1 && player_y1 >= y2) || (player_y2 <= y1 && player_y2 >= y2)) {
			// ys overlap
			y_collision = 1;
		}
		if ((player_z1 >= z1 && player_z1 <= z2) || (player_z2 >= z1 && player_z2 <= z2)) {
			// zs overlap
			z_collision = 1;
		}
		if (x_collision && y_collision && z_collision) {
			camera_pos.x -= x;
			break;
		}
	}

    camera_pos.y += y;

	// check collisions:
	// collisions:
	n = TEXTURE_WIDTH * TEXTURE_WIDTH * 6;
	for (int i = 0; i < world_squares.count; i += n) {
		// x1 = top left front
		int x1 = world_squares.items[i].coords[0].x;
		int y1 = world_squares.items[i].coords[0].y;
		int z1 = world_squares.items[i].coords[0].z;

		// x2 = bottom right back
		int x2 = x1 + CUBE_WIDTH;	
		int y2 = y1 - CUBE_WIDTH;	
		int z2 = z1 + CUBE_WIDTH;

		// let's start by saying the player is cube
		int player_width = CUBE_WIDTH / 2;
		int player_height = CUBE_WIDTH / 2;

		// player_x1 = top left front
		int player_x1 = camera_pos.x - player_width;
		int player_y1 = camera_pos.y + player_width;
		int player_z1 = camera_pos.z - player_width;

		// player_x1 = bottom right back
		int player_x2 = camera_pos.x + player_width;
		int player_y2 = camera_pos.y - player_width;
		int player_z2 = camera_pos.z + player_width;

		// TODO: keep track of which ones were previous collisions
		int x_collision = 0;
		int y_collision = 0;
		int z_collision = 0;
		if ((player_x1 >= x1 && player_x1 <= x2) || (player_x2 >= x1 && player_x2 <= x2)) {
			// xs overlap
			x_collision = 1;
		}
		if ((player_y1 <= y1 && player_y1 >= y2) || (player_y2 <= y1 && player_y2 >= y2)) {
			// ys overlap
			y_collision = 1;
		}
		if ((player_z1 >= z1 && player_z1 <= z2) || (player_z2 >= z1 && player_z2 <= z2)) {
			// zs overlap
			z_collision = 1;
		}
		if (x_collision && y_collision && z_collision) {
			camera_pos.y -= y;
			break;
		}
	}

    camera_pos.z += z;

	// check collisions:
	// collisions:
	n = TEXTURE_WIDTH * TEXTURE_WIDTH * 6;
	for (int i = 0; i < world_squares.count; i += n) {
		// x1 = top left front
		int x1 = world_squares.items[i].coords[0].x;
		int y1 = world_squares.items[i].coords[0].y;
		int z1 = world_squares.items[i].coords[0].z;

		// x2 = bottom right back
		int x2 = x1 + CUBE_WIDTH;	
		int y2 = y1 - CUBE_WIDTH;	
		int z2 = z1 + CUBE_WIDTH;

		// let's start by saying the player is cube
		int player_width = CUBE_WIDTH / 2;
		int player_height = CUBE_WIDTH / 2;

		// player_x1 = top left front
		int player_x1 = camera_pos.x - player_width;
		int player_y1 = camera_pos.y + player_width;
		int player_z1 = camera_pos.z - player_width;

		// player_x1 = bottom right back
		int player_x2 = camera_pos.x + player_width;
		int player_y2 = camera_pos.y - player_width;
		int player_z2 = camera_pos.z + player_width;

		// TODO: keep track of which ones were previous collisions
		int x_collision = 0;
		int y_collision = 0;
		int z_collision = 0;
		if ((player_x1 >= x1 && player_x1 <= x2) || (player_x2 >= x1 && player_x2 <= x2)) {
			// xs overlap
			x_collision = 1;
		}
		if ((player_y1 <= y1 && player_y1 >= y2) || (player_y2 <= y1 && player_y2 >= y2)) {
			// ys overlap
			y_collision = 1;
		}
		if ((player_z1 >= z1 && player_z1 <= z2) || (player_z2 >= z1 && player_z2 <= z2)) {
			// zs overlap
			z_collision = 1;
		}
		if (x_collision && y_collision && z_collision) {
			camera_pos.z -= z;
			break;
		}
	}

}

void draw_all_squares() {
	for (int i = 0; i < draw_squares.count; i++) {
		fill_square(&draw_squares.items[i]);
	}
}

int compare_squares(const void *one, const void *two) {
	const Square_t *square_one = one;
	const Square_t *square_two = two;

	int r1 = square_one->r;
	int r2 = square_two->r;

	if (r1 > r2) {
		return -1;
	}
	if (r1 < r2) {
		return 1;
	}

	return 0;
}

void writePPM(const char *filename, Texture_t *img) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
		assert(0);
	}

    fprintf(fp, "P6\n%d %d\n255\n", img->width, img->height);
    
    fwrite(img->data, 3, img->width * img->height, fp);
    fclose(fp);
}

Texture_t* readPPM(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    char header[3];
    int width, height, maxVal;

    if (fscanf(fp, "%2s", header) != 1 || strcmp(header, "P6") != 0) {
        fclose(fp);
        return NULL;
    }

    if (fscanf(fp, "%d %d", &width, &height) != 2) {
        fclose(fp);
        return NULL;
    }

    if (fscanf(fp, "%d", &maxVal) != 1 || maxVal != 255) {
        fclose(fp);
        return NULL;
    }

    fgetc(fp);  // consume exactly one byte

    Texture_t *img = malloc(sizeof(Texture_t));
    img->width = width;
    img->height = height;
    img->data = malloc(width * height * sizeof(Colour_t));

    if (fread(img->data,
              sizeof(Colour_t),
              width * height,
              fp) != width * height) {
        free(img->data);
        free(img);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return img;
}
