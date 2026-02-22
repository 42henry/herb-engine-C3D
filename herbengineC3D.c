#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>

//TODO: other blocks and terrain generation

//TODO: make movement and gravity velocity based
//TODO: make jumping only possible when on ground

//TODO: make it work on windows - need to get keys/keycodes correct, and use timespec to set frame rate similarly, and get mouse.x and y and left click and right click etc

//TODO: don't draw squares that are behind other squares

//TODO: optimise by lowering resolution somehow?

/* ----------------------- defines --------------------- */

#define WIDTH  3500
#define HEIGHT 1500

#define FOV 2

#define CM_TO_PIXELS 10
#define WIDTH_IN_CM ((WIDTH) / (CM_TO_PIXELS))

#define MAX_SQUARES 100000

#define CUBE_WIDTH (10 * CM_TO_PIXELS)
#define TEXTURE_WIDTH 5

#define SQUARES_PER_FACE (TEXTURE_WIDTH * TEXTURE_WIDTH)
#define SQUARES_PER_CUBE (TEXTURE_WIDTH * TEXTURE_WIDTH * 6)

#define TARGET_FPS 60
#define FRAME_TIME_NS (1000000000 / TARGET_FPS)

/* ----------------------- structs --------------------- */

typedef struct {
	int x;
	int y;
} vec2_t;

typedef struct {
	int x;
	int y;
	int z;	
} vec3_t;

typedef struct {
	vec3_t coords[4];	
	uint32_t colour;
	int r; // distance from camera
} square_t;

typedef struct {
	square_t *items;
	int count;
} squares_t;

typedef struct {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} colour_t;

typedef struct {
    int width, height;
    colour_t *data;
} texture_t;

/* ----------------------- local functions --------------------- */

static void init_stuff();

static void update();
static void update_pixels();

static void cleanup();

static void debug_log(char *str);

// maths
static uint32_t pack_colour_to_uint32(colour_t *colour);
static colour_t unpack_colour_from_uint32(uint32_t packed_colour);

// drawing
static void clear_screen(colour_t colour);
static void draw_rect(vec3_t top_left, int width, int height, colour_t colour);
static void fill_square(square_t *square);
static int test_fill_square();
static void draw_all_squares();

static void draw_cursor();
static void draw_hotbar();
static void draw_hand();

static void set_pixel(vec2_t coord, uint32_t colour);
static colour_t get_pixel_colour(vec2_t coord);

// rendering
static void render_squares();
static void render_hotbar();
static void render_hand();
static void rotate_and_project_squares(vec3_t *pos, vec3_t *new_pos);
static void rotate_and_project_squares_by_rot_value(vec3_t *pos, vec3_t *new_pos, float x_rot, float y_rot);

// cubes/squares handling
static void add_cube_squares_to_array(vec3_t top_left, texture_t *texture, squares_t *array);
static void remove_cube(int index);
static void place_cube(int index, texture_t *texture);
static int compare_squares(const void *one, const void *two);

// input
static void handle_input();
static void handle_mouse();

static void writePPM(const char *filename, texture_t *img);
static texture_t* readPPM(const char *filename);

// physics
static int collided();

/* ----------------------- local variables --------------------- */

static uint32_t *pixels = NULL;

static squares_t world_squares = {0};
static squares_t draw_squares = {0};
static int central_cube_index;
static int cube_highlighted = 0;

static squares_t hotbar_squares = {0};
static int small_height = 0;
static int hotbar_y = 0;
static int hotbar_x = 0;
static int hotbar_width = 0; 
static int hotbar_height = 0;
static int hotbar_selection = 0;
static colour_t hotbar_colour = {0};

static squares_t hand_squares = {0};

static vec3_t camera_pos = {0};

static int player_width = CUBE_WIDTH / 2;
static int player_height = CUBE_WIDTH / 2;

static int speed = 0;
static int walk_speed = 0;
static int sprint_speed = 0;

static int paused = 0;
static int frame = 0;
static int toggle = 0;

static vec2_t mouse = {0};
static float mouse_sensitivity = 0.005f;
static float x_rotation = 0;
static float y_rotation = 0;
static int mouse_left_click = 0;
static int mouse_was_left_clicked = 0;
static int mouse_right_click = 0;
static int mouse_was_right_clicked = 0;
static int hold_mouse = 1;
static int mouse_defined = 0;

static int keys[256] = {0};
static unsigned char w, a, s, d, shift, space, control, escape;
static unsigned char one, two, three, four, five, six, seven, eight, nine;

static int gravity = -10;
static int jump = 0;

static colour_t red = {255, 0, 0};
static colour_t green = {0, 255, 0};
static colour_t blue = {0, 0, 255};

static colour_t texture[SQUARES_PER_FACE] = {0};
texture_t *grass_texture = NULL;
texture_t *stone_texture = NULL;
texture_t *wood_texture = NULL;
texture_t *leaf_texture = NULL;

struct timespec last, now;

static int dlog = 0;

void init_stuff() {

	srand(time(NULL));

	// tests
	assert(test_fill_square());

	// create grass texture
	// * 3 for top bottom side of cube
    int w = TEXTURE_WIDTH * 3, h = TEXTURE_WIDTH;
    texture_t myImg = {w, h, malloc(w * h * 3)};

	// top
	int i = 0;
	for (i; i < SQUARES_PER_FACE; i++) {
		myImg.data[i] = (colour_t){10 + (rand() % 50), 180 + (rand() % 50), 20 + (rand() % 50), };
	}
	// bottom
	for (i; i < 2 * SQUARES_PER_FACE; i++) {
		myImg.data[i] = (colour_t){150 + (rand() % 50), 75 + (rand() % 50), 10 + (rand() % 50), };
	}
	// side
	for (i; i < 3 * SQUARES_PER_FACE; i++) {
		if (i < 2.5 * SQUARES_PER_FACE) {
			myImg.data[i] = (colour_t){10 + (rand() % 50), 180 + (rand() % 50), 20 + (rand() % 50), };
		}
		else {
			myImg.data[i] = (colour_t){150 + (rand() % 50), 75 + (rand() % 50), 10 + (rand() % 50), };
		}
	}

    writePPM("grass.ppm", &myImg);

	grass_texture = readPPM("grass.ppm");
	assert(grass_texture != NULL);

	// create stone texture
	// top bottom side
	i = 0;
	for (i; i < SQUARES_PER_FACE * 3; i++) {
		myImg.data[i] = (colour_t){75 + (rand()  % 12), 75 + (rand()  % 13), 75 + (rand()  % 5), };
	}

    writePPM("stone.ppm", &myImg);

	stone_texture = readPPM("stone.ppm");
	assert(stone_texture != NULL);

	// create wood texture
	// * 3 for top bottom side of cube
	// top
	i = 0;
	for (i; i < SQUARES_PER_FACE; i++) {
		myImg.data[i] = (colour_t){200 + (rand() % 50), 180 + (rand() % 50), 150 + (rand() % 50), };
	}
	// bottom
	for (i; i < 2 * SQUARES_PER_FACE; i++) {
		myImg.data[i] = (colour_t){200 + (rand() % 50), 180 + (rand() % 50), 150 + (rand() % 50), };
	}
	// side
	for (i; i < 3 * SQUARES_PER_FACE; i++) {
		myImg.data[i] = (colour_t){90 + (rand() % 10), 60 + (rand() % 10), 50 + (rand() % 20), };
	}

    writePPM("wood.ppm", &myImg);

	wood_texture = readPPM("wood.ppm");
	assert(wood_texture != NULL);

	// create leaf texture
	// top bottom side
	i = 0;
	for (i; i < SQUARES_PER_FACE * 3; i++) {
		myImg.data[i] = (colour_t){5 - (rand()  % 5), 95 - (rand()  % 20), 7 - (rand()  % 7), };
	}

    writePPM("leaf.ppm", &myImg);

    free(myImg.data);

	leaf_texture = readPPM("leaf.ppm");
	assert(leaf_texture != NULL);

	// set initial values
	world_squares.items = malloc(MAX_SQUARES * sizeof(square_t));
	draw_squares.items = malloc(MAX_SQUARES * sizeof(square_t));
	cube_highlighted = -1;

	camera_pos.x = 0;
	camera_pos.y = 300;
	camera_pos.z = 100;

	player_width = CUBE_WIDTH / 2;
	player_height = CUBE_WIDTH / 2;

	hold_mouse = 1;
	mouse_sensitivity = 0.005f;

	speed = 10;
	walk_speed = speed;
	sprint_speed = 20;
	gravity = -10;

	red.r = 255;
	green.g = 255;
	blue.b = 255;

	small_height = HEIGHT * 0.01;
	hotbar_y = HEIGHT - (HEIGHT * 0.1);
	hotbar_x = WIDTH / 6;
	hotbar_width = WIDTH - 2 * (WIDTH / 6); 
	hotbar_height = HEIGHT * 0.2;
	hotbar_selection = 0;
	hotbar_colour.r = 50;
	hotbar_colour.g = 50;
	hotbar_colour.b = 50;

	// 9 hotbar slots
	hotbar_squares.items = malloc(9 * SQUARES_PER_CUBE * sizeof(square_t));
	add_cube_squares_to_array((vec3_t){-200, -100, 400}, grass_texture, &hotbar_squares);
	add_cube_squares_to_array((vec3_t){-200, -100, 400}, stone_texture, &hotbar_squares);
	add_cube_squares_to_array((vec3_t){-200, -100, 400}, wood_texture, &hotbar_squares);
	add_cube_squares_to_array((vec3_t){-200, -100, 400}, leaf_texture, &hotbar_squares);
	render_hotbar();

	// hand
	hand_squares.items = malloc(1 * SQUARES_PER_CUBE * sizeof(square_t));
	add_cube_squares_to_array((vec3_t){0, -50, 1}, grass_texture, &hand_squares);
	render_hand();

	// setup the world:
	colour_t c = blue;
	for (int i = -5; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			add_cube_squares_to_array((vec3_t){50 + (i * CUBE_WIDTH), 50, 10 + (j * CUBE_WIDTH)}, grass_texture, &world_squares);
		}
	}

	// tree
	add_cube_squares_to_array((vec3_t){350, 150, 310}, wood_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){350, 250, 310}, wood_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){350, 350, 310}, wood_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){350, 450, 310}, wood_texture, &world_squares);

	add_cube_squares_to_array((vec3_t){250, 450, 310}, leaf_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){350, 450, 210}, leaf_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){350, 450, 410}, leaf_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){450, 450, 310}, leaf_texture, &world_squares);

	add_cube_squares_to_array((vec3_t){250, 350, 310}, leaf_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){350, 350, 210}, leaf_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){350, 350, 410}, leaf_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){450, 350, 310}, leaf_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){450, 350, 410}, leaf_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){450, 350, 210}, leaf_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){250, 350, 410}, leaf_texture, &world_squares);
	add_cube_squares_to_array((vec3_t){250, 350, 210}, leaf_texture, &world_squares);

	return;
}

void update() {
	if (paused) {
		return;
	}
	frame++;

	handle_input();

	update_pixels();
}

void update_pixels() {

    clear_screen((colour_t){50, 180, 255});

	render_squares();

	draw_all_squares();

	draw_cursor();
	draw_hand();
	draw_hotbar();

	return;
}

uint32_t pack_colour_to_uint32(colour_t *colour) {
    return (1 << 24 | colour->r << 16) | (colour->g << 8) | colour->b;
}

colour_t unpack_colour_from_uint32(uint32_t packed_colour) {
	colour_t colour = {0};
	colour.r = (packed_colour >> 16) & 255;
	colour.g = (packed_colour >> 8) & 255;
	colour.b = packed_colour & 255;
    return colour;
}

void add_cube_squares_to_array(vec3_t top_left, texture_t *texture, squares_t *array) {
	// take a top left front coord of a cube, convert it to texture mapped squares, and add it to an array
	int x = top_left.x;
	int y = top_left.y;
	int z = top_left.z;

	int len = CUBE_WIDTH / TEXTURE_WIDTH;

	int texture_side = SQUARES_PER_FACE;

	// front
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			square_t square = {0};
			square.coords[0] = (vec3_t) {x + (i * len), y - (j * len), z};
			square.coords[1] = (vec3_t) {x + ((i + 1) * len), y - (j * len), z};
			square.coords[2] = (vec3_t) {x + ((i + 1) * len), y - ((j + 1) * len), z};
			square.coords[3] = (vec3_t) {x + (i * len), y - ((j + 1) * len), z};

			// 2, as the side face textures are the 2nd square of the texture image
			colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
			square.colour = pack_colour_to_uint32(&c);

			// set r to 1 to signify the start of a cube
			array->items[array->count] = square;
			if (i == 0 && j == 0) {
				array->items[array->count].r = 1;
			}
			array->count++;
		}
	}
	// back
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			square_t square = {0};

			square.coords[0] = (vec3_t) {x + (i * len), y - (j * len), z + CUBE_WIDTH};
			square.coords[1] = (vec3_t) {x + ((i + 1) * len), y - (j * len), z + CUBE_WIDTH};
			square.coords[2] = (vec3_t) {x + ((i + 1) * len), y - ((j + 1) * len), z + CUBE_WIDTH};
			square.coords[3] = (vec3_t) {x + (i * len), y - ((j + 1) * len), z + CUBE_WIDTH};

			colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
			square.colour = pack_colour_to_uint32(&c);

			array->items[array->count++] = square;
		}
	}
	// left
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			square_t square = {0};

			square.coords[0] = (vec3_t) {x, y - (j * len), z + (i * len)};
			square.coords[1] = (vec3_t) {x, y - (j * len), z + ((i + 1) * len)};
			square.coords[2] = (vec3_t) {x, y - ((j + 1) * len), z + ((i + 1) * len)};
			square.coords[3] = (vec3_t) {x, y - ((j + 1) * len), z + (i * len)};

			colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
			square.colour = pack_colour_to_uint32(&c);

			array->items[array->count++] = square;
		}
	}
	// right
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			square_t square = {0};

			square.coords[0] = (vec3_t) {x + CUBE_WIDTH, y - (j * len), z + (i * len)};
			square.coords[1] = (vec3_t) {x + CUBE_WIDTH, y - (j * len), z + ((i + 1) * len)};
			square.coords[2] = (vec3_t) {x + CUBE_WIDTH, y - ((j + 1) * len), z + ((i + 1) * len)};
			square.coords[3] = (vec3_t) {x + CUBE_WIDTH, y - ((j + 1) * len), z + (i * len)};

			colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
			square.colour = pack_colour_to_uint32(&c);

			array->items[array->count++] = square;
		}
	}
	// top
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			square_t square = {0};

			square.coords[0] = (vec3_t) {x + (i * len), y, z + (j * len)};
			square.coords[1] = (vec3_t) {x + ((i + 1) * len), y, z + (j * len)};
			square.coords[2] = (vec3_t) {x + ((i + 1) * len), y, z + ((j + 1) * len)};
			square.coords[3] = (vec3_t) {x + (i * len), y, z + ((j + 1) * len)};

			// 0 as that is the top texture
			colour_t c = texture->data[j * TEXTURE_WIDTH + i];
			square.colour = pack_colour_to_uint32(&c);

			array->items[array->count++] = square;
		}
	}
	// bottom
	for (int i = 0; i < TEXTURE_WIDTH; i++) {
		for (int j = 0; j < TEXTURE_WIDTH; j++) {
			square_t square = {0};

			square.coords[0] = (vec3_t) {x + (i * len), y - CUBE_WIDTH, z + (j * len)};
			square.coords[1] = (vec3_t) {x + ((i + 1) * len), y - CUBE_WIDTH, z + (j * len)};
			square.coords[2] = (vec3_t) {x + ((i + 1) * len), y - CUBE_WIDTH, z + ((j + 1) * len)};
			square.coords[3] = (vec3_t) {x + (i * len), y - CUBE_WIDTH, z + ((j + 1) * len)};

			// 1, as the top face textures are the first square of the texture image
			colour_t c = texture->data[1 * texture_side + j * TEXTURE_WIDTH + i];
			square.colour = pack_colour_to_uint32(&c);

			array->items[array->count++] = square;
		}
	}
	return;
}

void clear_screen(colour_t colour) {
	uint32_t c = pack_colour_to_uint32(&colour);
	for (int x = 0; x < WIDTH; x++) {
		for (int y = 0; y < HEIGHT; y++) {
			pixels[y * WIDTH + x] = c;
		}
	}
	return;
}

void render_squares() {

	int closest_r = 99999999;
	cube_highlighted = -1;

	for (int i = 0; i < world_squares.count; i++) {

		// used	for distance to camera
		int x1 = 0;
		int y1 = 0;
		int z1 = 0;

		for (int j = 0; j < 4; j++) {
			vec3_t pos = {0};
			pos.x = world_squares.items[i].coords[j].x;
			pos.y = world_squares.items[i].coords[j].y;
			pos.z = world_squares.items[i].coords[j].z;
			pos.x -= camera_pos.x;
			pos.y -= camera_pos.y;
			pos.z -= camera_pos.z;

			// top left front coord
			if (j == 0) {
				x1 += pos.x;
				y1 += pos.y;
				z1 += pos.z;
			}
			// bottom right back coord
			if (j == 2) {
				x1 += pos.x;
				x1 /= 2;
				y1 += pos.y;
				y1 /= 2;
				z1 += pos.z;
				z1 /= 2;
			}

			vec3_t new_pos = {0};
			rotate_and_project_squares(&pos, &new_pos);

			draw_squares.items[i].coords[j].x = new_pos.x;
			draw_squares.items[i].coords[j].y = new_pos.y;
			draw_squares.items[i].coords[j].z = new_pos.z;
			draw_squares.items[i].colour = world_squares.items[i].colour;
		}

		// calc distance to camera
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
			// also check that the z value is positive!!!
			if (draw_squares.items[i].r < closest_r && draw_squares.items[i].coords[0].z) {
				closest_r = draw_squares.items[i].r;

				// find the first square in this cube:
				int index = i;
				int min = i - SQUARES_PER_CUBE;
				for (index; index--; index > min) {
					if (world_squares.items[index].r == 1) {
						break;
					}
				}

				// find the face we have highlighted
				cube_highlighted = 0;
				int face_index = i - index;
				// 0 = front
				// 1 = back
				// 2 = right
				// 3 = left
				// 4 = top
				// 5 = bottom
				cube_highlighted = face_index / SQUARES_PER_FACE;
				central_cube_index = index;
			}
		}
	}

	if (cube_highlighted > -1) {
		// highlight square under cursor:
		for (int i = central_cube_index; i < central_cube_index + SQUARES_PER_CUBE; i++) {
			colour_t colour = unpack_colour_from_uint32(draw_squares.items[i].colour);
			colour.r = (colour.r + 100);
			if (colour.r < 100) {
				colour.r = 255;
			}
			colour.g = (colour.g + 100);
			if (colour.g < 100) {
				colour.g = 255;
			}
			colour.b = (colour.b + 100);
			if (colour.b < 100) {
				colour.b = 255;
			}
			draw_squares.items[i].colour = pack_colour_to_uint32(&colour);
		}
	}

	draw_squares.count = world_squares.count;

	// sort the squares based on their distance to the camera
	qsort(draw_squares.items, draw_squares.count, sizeof(square_t), compare_squares);
}

int compare_squares(const void *one, const void *two) {
	const square_t *square_one = one;
	const square_t *square_two = two;

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

void rotate_and_project_squares(vec3_t *pos, vec3_t *new_pos) {

	// rotate
	new_pos->x = (pos->z * sin(x_rotation) + pos->x * cos(x_rotation));
	int z2 = (pos->z * cos(x_rotation) - pos->x * sin(x_rotation));

	new_pos->y = (z2 * sin(y_rotation) + pos->y * cos(y_rotation));
	new_pos->z = (z2 * cos(y_rotation) - pos->y * sin(y_rotation));
	if (new_pos->z == 0) {
		new_pos->z = 1;
	}

	// project
	float percent_size = ((float)(WIDTH_IN_CM * FOV) / new_pos->z);
	if (new_pos->z > 0) {
		new_pos->x *= percent_size;
		new_pos->y *= percent_size;
	}
	else {
		new_pos->z = 0;
	}

	// convert from standard grid to screen grid
	new_pos->x = new_pos->x + WIDTH / 2;
	new_pos->y = -new_pos->y + HEIGHT / 2;
}

void draw_all_squares() {
	for (int i = 0; i < draw_squares.count; i++) {
		fill_square(&draw_squares.items[i]);
	}
}

void draw_cursor() {
	colour_t colour = get_pixel_colour((vec2_t){WIDTH / 2, HEIGHT / 2});
	if (colour.r > 127) {
		colour.r = 0;
	}
	else {
		colour.r = 255;
	}
	if (colour.g > 127) {
		colour.g = 0;
	}
	else {
		colour.g = 255;
	}
	if (colour.b > 127) {
		colour.b = 0;
	}
	else {
		colour.b = 255;
	}
	int h = 30;
	int w = 6;
	draw_rect((vec3_t){WIDTH / 2 - (w / 2), HEIGHT / 2 - (h / 2), 1}, w, h, colour);
	draw_rect((vec3_t){WIDTH / 2 - (h / 2), HEIGHT / 2 - (w / 2), 1}, h, w, colour);
}

void draw_hotbar() {
	int x = hotbar_x;
	int y = hotbar_y;

	int w = hotbar_width;
	int h = hotbar_height;
    draw_rect((vec3_t){x, y, 1}, w, small_height, hotbar_colour);
	y -= h;
    draw_rect((vec3_t){x, y, 1}, w, small_height, hotbar_colour);

    draw_rect((vec3_t){x, y, 1}, small_height, h, hotbar_colour);
	x += w;
    draw_rect((vec3_t){x, y, 1}, small_height, h, hotbar_colour);

	int magic_num = 300;
	switch(hotbar_selection) {
		case 0: {
			draw_rect((vec3_t){hotbar_x + 100, hotbar_y - hotbar_height, 1}, magic_num, hotbar_height, hotbar_colour);
			break;
		}
		case 1: {
			draw_rect((vec3_t){hotbar_x + magic_num + 100, hotbar_y - hotbar_height, 1}, magic_num, hotbar_height, hotbar_colour);
			break;
		}
		case 2: {
			draw_rect((vec3_t){hotbar_x + 2 * magic_num + 100, hotbar_y - hotbar_height, 1}, magic_num, hotbar_height, hotbar_colour);
			break;
		}
		case 3: {
			draw_rect((vec3_t){hotbar_x + 3 * magic_num + 100, hotbar_y - hotbar_height, 1}, magic_num, hotbar_height, hotbar_colour);
			break;
		}
	}

	for (int i = 0; i < hotbar_squares.count; i++) {
		fill_square(&hotbar_squares.items[i]);
	}
}

void draw_hand() {
	for (int i = 0; i < hand_squares.count; i++) {
		fill_square(&hand_squares.items[i]);
	}
}

colour_t get_pixel_colour(vec2_t coord) {
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

void fill_square(square_t *square) {
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

void draw_rect(vec3_t top_left, int width, int height, colour_t colour) {
	square_t square = {0};
	square.coords[0].x = top_left.x;
	square.coords[0].y = top_left.y;
	square.coords[0].z = 10;

	square.coords[1].x = top_left.x + width;
	square.coords[1].y = top_left.y;
	square.coords[1].z = 10;

	square.coords[2].x = top_left.x + width;
	square.coords[2].y = top_left.y + height;
	square.coords[2].z = 10;

	square.coords[3].x = top_left.x;
	square.coords[3].y = top_left.y + height;
	square.coords[3].z = 10;

	square.colour = pack_colour_to_uint32(&colour);

	fill_square(&square);
}

void place_cube(int index, texture_t *texture) {
	vec3_t pos = {0};
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
	int x1 = pos.x;
	int y1 = pos.y;
	int z1 = pos.z;

	// x2 = bottom right back
	int x2 = x1 + CUBE_WIDTH;	
	int y2 = y1 - CUBE_WIDTH;	
	int z2 = z1 + CUBE_WIDTH;

	camera_pos.y -= CUBE_WIDTH;
	// player_x1 = top left front
	int player_x1 = camera_pos.x - player_width;
	int player_y1 = camera_pos.y + player_width;
	int player_z1 = camera_pos.z - player_width;

	// player_x1 = bottom right back
	int player_x2 = camera_pos.x + player_width;
	int player_y2 = camera_pos.y - player_width;
	int player_z2 = camera_pos.z + player_width;

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
	camera_pos.y += CUBE_WIDTH;
	if (x_collision && y_collision && z_collision) {
		return;
	}

	add_cube_squares_to_array(pos, texture, &world_squares);
	return;
}

void remove_cube(int index) {

	int count = index;
	for (int i = index + SQUARES_PER_CUBE; i < world_squares.count; i++) {
		world_squares.items[count] = world_squares.items[i];
		count++;	
	}
	world_squares.count -= SQUARES_PER_CUBE;
	return;
}

void handle_input()
{
	handle_mouse();

	// handle keys/movement
	int x = 0;
	int z = 0;
	int y = 0;
	if (keys[control]) {
        speed = sprint_speed;
	}
	else {
		speed = walk_speed;
	}
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
		if (! jump) {
			jump = 2.5 * CUBE_WIDTH;
		}
	}
    if (keys[shift]) {
        y += - speed;
	}
	if (keys[escape]) {
		hold_mouse = 0;
	}

	y += gravity;
	y += jump / 10;
	if (jump > 0) {
		jump -= 10;
	}
	else {
		jump = 0;
	}

	// make the player 2 cubes tall:
	camera_pos.y -= CUBE_WIDTH;

    camera_pos.x += x;
	if (collided()) {
		camera_pos.x -= x;
	}

    camera_pos.y += y;
	if (collided()) {
		camera_pos.y -= y;
	}

    camera_pos.z += z;
	if (collided()) {
		camera_pos.z -= z;
	}
	
	camera_pos.y += CUBE_WIDTH;

	if (keys[one]) {
		hotbar_selection = 0;
		hand_squares.count = 0;
		add_cube_squares_to_array((vec3_t){0, -50, 1}, grass_texture, &hand_squares);
		render_hand();
	}
	if (keys[two]) {
		hotbar_selection = 1;
		hand_squares.count = 0;
		add_cube_squares_to_array((vec3_t){0, -50, 1}, stone_texture, &hand_squares);
		render_hand();
	}
	if (keys[three]) {
		hotbar_selection = 2;
		hand_squares.count = 0;
		add_cube_squares_to_array((vec3_t){0, -50, 1}, wood_texture, &hand_squares);
		render_hand();
	}
	if (keys[four]) {
		hotbar_selection = 3;
		hand_squares.count = 0;
		add_cube_squares_to_array((vec3_t){0, -50, 1}, leaf_texture, &hand_squares);
		render_hand();
	}
}

void handle_mouse() {
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
				switch (hotbar_selection) {
					case 0: {
						place_cube(central_cube_index, grass_texture);
						break;
					}
					case 1: {
						place_cube(central_cube_index, stone_texture);
						break;
					}
					case 2: {
						place_cube(central_cube_index, wood_texture);
						break;
					}
					case 3: {
						place_cube(central_cube_index, leaf_texture);
						break;
					}
				}
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
}

int collided() {
	for (int i = 0; i < world_squares.count; i += SQUARES_PER_CUBE) {
		// x1 = top left front
		int x1 = world_squares.items[i].coords[0].x;
		int y1 = world_squares.items[i].coords[0].y;
		int z1 = world_squares.items[i].coords[0].z;

		// x2 = bottom right back
		int x2 = x1 + CUBE_WIDTH;	
		int y2 = y1 - CUBE_WIDTH;	
		int z2 = z1 + CUBE_WIDTH;

		// player_x1 = top left front
		int player_x1 = camera_pos.x - player_width;
		int player_y1 = camera_pos.y + player_width;
		int player_z1 = camera_pos.z - player_width;

		// player_x1 = bottom right back
		int player_x2 = camera_pos.x + player_width;
		int player_y2 = camera_pos.y - player_width;
		int player_z2 = camera_pos.z + player_width;

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
			return 1;
		}
	}
	return 0;
}

void writePPM(const char *filename, texture_t *img) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
		assert(0);
	}

    fprintf(fp, "P6\n%d %d\n255\n", img->width, img->height);
    
    fwrite(img->data, 3, img->width * img->height, fp);
    fclose(fp);
}

texture_t* readPPM(const char *filename) {
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

    texture_t *img = malloc(sizeof(texture_t));
    img->width = width;
    img->height = height;
    img->data = malloc(width * height * sizeof(colour_t));

    if (fread(img->data,
              sizeof(colour_t),
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

void debug_log(char *str) {
	printf("\n%s", str);
}

void cleanup() {
	free(world_squares.items);
	free(draw_squares.items);
    free(pixels);
}

void render_hotbar() {
	// render the hotbar squares:
	int count = -1;
	for (int i = 0; i < hotbar_squares.count; i++) {
		if (i % SQUARES_PER_CUBE == 0) {
			count++;
		}

		// used	for distance to camera
		int x1 = 0;
		int y1 = 0;
		int z1 = 0;

		for (int j = 0; j < 4; j++) {
			vec3_t pos = {0};
			pos.x = hotbar_squares.items[i].coords[j].x;
			pos.y = hotbar_squares.items[i].coords[j].y;
			pos.z = hotbar_squares.items[i].coords[j].z;

			// top left front coord
			if (j == 0) {
				x1 += pos.x;
				y1 += pos.y;
				z1 += pos.z;
			}
			// bottom right back coord
			if (j == 2) {
				x1 += pos.x;
				x1 /= 2;
				y1 += pos.y;
				y1 /= 2;
				z1 += pos.z;
				z1 /= 2;
			}

			vec3_t new_pos = {0};
			rotate_and_project_squares(&pos, &new_pos);

			hotbar_squares.items[i].coords[j].x = new_pos.x - 700 + (300 * count);
			hotbar_squares.items[i].coords[j].y = new_pos.y + 200;
			hotbar_squares.items[i].coords[j].z = new_pos.z;
		}

		// calc distance to camera
		int r = sqrt((x1 * x1) + (y1 * y1) + (z1 * z1));
		hotbar_squares.items[i].r = r;
	}

	// sort the squares based on their distance to the camera
	qsort(hotbar_squares.items, hotbar_squares.count, sizeof(square_t), compare_squares);
}

void render_hand() {
	// render the hand squares:
	for (int i = 0; i < hand_squares.count; i++) {

		// used	for distance to camera
		int x1 = 0;
		int y1 = 0;
		int z1 = 0;

		for (int j = 0; j < 4; j++) {
			vec3_t pos = {0};
			pos.x = hand_squares.items[i].coords[j].x;
			pos.y = hand_squares.items[i].coords[j].y;
			pos.z = hand_squares.items[i].coords[j].z;

			// top left front coord
			if (j == 0) {
				x1 += pos.x;
				y1 += pos.y;
				z1 += pos.z;
			}
			// bottom right back coord
			if (j == 2) {
				x1 += pos.x;
				x1 /= 2;
				y1 += pos.y;
				y1 /= 2;
				z1 += pos.z;
				z1 /= 2;
			}

			vec3_t new_pos = {0};

			// TODO: make this function take an x and y angle in so the hand always renders correctly, same for hotbar!
			rotate_and_project_squares_by_rot_value(&pos, &new_pos, 0, 0);

			hand_squares.items[i].coords[j].x = new_pos.x;
			hand_squares.items[i].coords[j].y = new_pos.y;
			hand_squares.items[i].coords[j].z = new_pos.z;
		}

		// calc distance to camera
		int r = sqrt((x1 * x1) + (y1 * y1) + (z1 * z1));
		hand_squares.items[i].r = r;
	}

	// sort the squares based on their distance to the camera
	qsort(hand_squares.items, hand_squares.count, sizeof(square_t), compare_squares);
}

void rotate_and_project_squares_by_rot_value(vec3_t *pos, vec3_t *new_pos, float x_rot, float y_rot) {

	// rotate
	new_pos->x = (pos->z * sin(x_rot) + pos->x * cos(x_rot));
	int z2 = (pos->z * cos(x_rot) - pos->x * sin(x_rot));

	new_pos->y = (z2 * sin(y_rot) + pos->y * cos(y_rot));
	new_pos->z = (z2 * cos(y_rot) - pos->y * sin(y_rot));
	if (new_pos->z == 0) {
		new_pos->z = 1;
	}

	// project
	float percent_size = ((float)(WIDTH_IN_CM * FOV) / new_pos->z);
	if (new_pos->z > 0) {
		new_pos->x *= percent_size;
		new_pos->y *= percent_size;
	}
	else {
		new_pos->z = 0;
	}

	// convert from standard grid to screen grid
	new_pos->x = new_pos->x + WIDTH / 2;
	new_pos->y = -new_pos->y + HEIGHT / 2;
}
