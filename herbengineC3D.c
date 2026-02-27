#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>

//TODO:

// add fog at chunk boundaries

// place_cube is no longer checking neighbours

// fix remove cube - need to check if cube was on the boundary of a chunk for neighbours!
// improve detecting if we crossed a chunk boundary
// detecting if we crossed a chunk boundary doesn't work at 0, 0, 0 area

// if we place or remove a block, store the location and block type in the chunk data, and save
// the chunk data.
// when we load a chunk, check if that chunk is stored in chunk data, else just generate it normally

// simple terration - trees
// simple lighting

// fix fill squares to accomodate for when the ys of the square are too close

// optimise by lowering resolution of the fill square function - see the TODO note

/* ----------------------- defines --------------------- */

#define WIDTH  1920
#define HEIGHT 1080

#define FOV 2

#define MAX_CUBES 1000000

#define CUBE_WIDTH 1000

#define TEXTURE_WIDTH 2

#define SQUARES_PER_FACE (TEXTURE_WIDTH * TEXTURE_WIDTH)
#define SQUARES_PER_CUBE (TEXTURE_WIDTH * TEXTURE_WIDTH * 6)

#define TARGET_FPS 60
#define FRAME_TIME_NS (1000000000 / TARGET_FPS)

#define NUM_CHUNKS 25
#define CHUNK_WIDTH 16
#define CUBES_PER_CHUNK (CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_WIDTH)

#define HOTBAR_SLOTS 9
#define HOTBAR_SLOT_WIDTH (WIDTH / (HOTBAR_SLOTS + 2)) // add 2 so it's centred

#define REACH (4 * CUBE_WIDTH)

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
} square_t;

typedef struct {
	square_t squares[SQUARES_PER_FACE];
	float r;	
} face_t;

typedef struct {
	face_t items[6 * CUBES_PER_CHUNK * NUM_CHUNKS];
	int count;
} faces_t;

typedef enum {
	TOP, FRONT, LEFT, BACK, RIGHT, BOTTOM
} direction_t;

typedef enum {
	Z_POS, Z_NEG, X_POS, X_NEG
} chunk_dir_t;

typedef struct {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} colour_t;

typedef struct {
    int width, height;
    colour_t *data;
} texture_t;

typedef struct {
	texture_t *texture;
	// TODO: could pack this into one int
	int top_neighbour;
	int front_neighbour;
	int left_neighbour;
	int back_neighbour;
	int right_neighbour;
	int bottom_neighbour;
} cube_t;

typedef struct {
	cube_t *items;
	int count;
} cubes_t;

typedef struct {
	cube_t cubes[CUBES_PER_CHUNK];
	vec3_t pos;
} chunk_t;

typedef struct {
    unsigned char p[512];
} perlin_t;

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
static void draw_all_faces();

static void draw_cursor();
static void draw_hotbar();
static void draw_hand();

static colour_t get_pixel_colour(vec2_t coord);

// textures
void generate_textures();

// rendering
static void render_chunks();
static void render_hotbar();
static void render_hand();
static vec3_t rotate_and_project(vec3_t pos);
static void rotate_and_project_by_rot_value(vec3_t *pos, vec3_t *new_pos, float x_rot, float y_rot);
static int square_surrounds_centre(square_t *square);

// cubes/squares handling
static void add_cube_to_cubes_array(vec3_t top_left, texture_t *texture, cubes_t *array);
static void remove_cube();
static void place_cube(texture_t *texture);
static int compare_faces(const void *one, const void *two);

// input
static void handle_input();
static void handle_mouse();

static void writePPM(const char *filename, texture_t *img);
static texture_t* readPPM(const char *filename);

// physics
static int collided();

// chunks
static void update_chunks(int dir);
static void generate_chunk(chunk_t *chunk);
static void remove_chunk(vec3_t pos);

// perlin noise:
static uint32_t lcg(uint32_t *state);
static void perlin_init(perlin_t *n, uint32_t seed);
static double fade(double t);
static double lerp(double a, double b, double t);
static double grad(int h, double x, double y);
static double perlin2D(perlin_t *n, double x, double y);

/* ----------------------- local variables --------------------- */

static uint32_t *pixels = NULL;

static chunk_t chunks[NUM_CHUNKS] = {0};
static int occupied_chunk_index = 0;

static faces_t draw_faces = {0};

static int highlighted_cube_face = 0;
static int highlighted_cube_index = 0;
static int highlighted_cube_chunk_index = 0;

//static cubes_t hotbar_cubes = {0};
//static faces_t hotbar_faces = {0};
static int small_height = 0;
static int hotbar_y = 0;
static int hotbar_x = 0;
static int hotbar_width = 0; 
static int hotbar_height = 0;
static int hotbar_selection = 0;
static colour_t hotbar_colour = {0};

//static cubes_t hand_cubes = {0};
//static faces_t hand_faces = {0};

static vec3_t camera_pos = {0};

static int player_width = 0;
static int player_height = 0;

static int speed = 0;
static int walk_speed = 0;
static int sprint_speed = 0;

static int paused = 0;
static int frame = 0;
static int toggle = 0;

static vec2_t mouse = {0};
static float mouse_sensitivity = 0;
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

static int gravity = 0;
static int jump = 0;
static int jump_height = 0;

static colour_t red = {0};
static colour_t green = {0};
static colour_t blue = {0};

static colour_t texture[SQUARES_PER_FACE] = {0};
texture_t *grass_texture = NULL;
texture_t *dirt_texture = NULL;
texture_t *stone_texture = NULL;
texture_t *wood_texture = NULL;
texture_t *leaf_texture = NULL;

struct timespec last, now;

static int dlog = 0;

static perlin_t noise;

static int bruh_top = 0;
static int bruh_bottom = 0;
static int bruh_left = 0;
static int bruh_right = 0;
static int bruh_front = 0;
static int bruh_back = 0;

static int chunk_width_inv = 0;
static int chunk_width_squared_inv = 0;

void init_stuff() {

	srand(time(NULL));
    perlin_init(&noise, 69420);

	chunk_width_inv = 1 / CHUNK_WIDTH;
	chunk_width_squared_inv = 1 / (CHUNK_WIDTH * CHUNK_WIDTH);

	// tests
	assert(test_fill_square());

	generate_textures();

	// set initial values
	draw_faces.count = 0;
	highlighted_cube_face = -1;

	camera_pos.x = 0;
	camera_pos.y = 10 * CUBE_WIDTH;
	camera_pos.z = 1 * CUBE_WIDTH;

	player_width = CUBE_WIDTH / 3;
	player_height = 4 * player_width;

	hold_mouse = 1;
	mouse_sensitivity = 0.005f;

	speed = CUBE_WIDTH / 9;
	walk_speed = speed;
	sprint_speed = speed * 1.5;
	jump_height = 2.5 * CUBE_WIDTH;
	gravity = - CUBE_WIDTH / 6;

	red.r = 255;
	green.g = 255;
	blue.b = 255;

	small_height = HEIGHT * 0.01;
	hotbar_y = HEIGHT - (HEIGHT * 0.1);
	hotbar_x = HOTBAR_SLOT_WIDTH;
	hotbar_width = HOTBAR_SLOT_WIDTH * 9; 
	hotbar_height = HOTBAR_SLOT_WIDTH;
	hotbar_selection = 0;
	hotbar_colour.r = 50;
	hotbar_colour.g = 50;
	hotbar_colour.b = 50;

	// 9 hotbar slots
	//hotbar_cubes.items = malloc(HOTBAR_SLOTS * sizeof(cube_t));
	//hotbar_faces.items = malloc(HOTBAR_SLOTS * 6 * sizeof(face_t));
	//add_cube_to_cubes_array((vec3_t){0, 0, 6 * CUBE_WIDTH}, grass_texture, &hotbar_cubes);
	//add_cube_to_cubes_array((vec3_t){0, 0, 6 * CUBE_WIDTH}, stone_texture, &hotbar_cubes);
	//add_cube_to_cubes_array((vec3_t){0, 0, 6 * CUBE_WIDTH}, wood_texture, &hotbar_cubes);
	//add_cube_to_cubes_array((vec3_t){0, 0, 6 * CUBE_WIDTH}, leaf_texture, &hotbar_cubes);
	//render_hotbar();

	// hand
	//hand_cubes.items = malloc(sizeof(cube_t));
	//hand_faces.items = malloc(6 * sizeof(face_t));
	//vec3_t pos = {camera_pos.x + 2 * CUBE_WIDTH, camera_pos.y - 0.5 * CUBE_WIDTH, camera_pos.z + 0.5 * CUBE_WIDTH};
	//add_cube_to_cubes_array(pos, grass_texture, &hand_cubes);
	//render_hand();

	// setup the world:
	vec3_t chunk_pos = {-CHUNK_WIDTH * (sqrt(NUM_CHUNKS) / 2) * CUBE_WIDTH, 0, -CHUNK_WIDTH * (sqrt(NUM_CHUNKS) / 2) * CUBE_WIDTH};
	int count = 0;
	for (int i = 0; i < sqrt(NUM_CHUNKS); i++) {
		for (int j = 0; j < sqrt(NUM_CHUNKS); j++) {
			chunks[count].pos = (vec3_t){chunk_pos.x + i * CUBE_WIDTH * CHUNK_WIDTH, 0, chunk_pos.z + j * CUBE_WIDTH * CHUNK_WIDTH};
			generate_chunk(&chunks[count]);
			count++;
		}
	}
	occupied_chunk_index = NUM_CHUNKS / 2;

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

	render_chunks();

	draw_all_faces();

	draw_cursor();
	//draw_hand();
	//draw_hotbar();

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

void add_cube_to_cubes_array(vec3_t top_left, texture_t *texture, cubes_t *array) {
	//// take a top left front coord of a cube, convert it to texture mapped squares, and add it to an array
	//int x = top_left.x;
	//int y = top_left.y;
	//int z = top_left.z;
//
	//int len = CUBE_WIDTH / TEXTURE_WIDTH;
//
	//int texture_side = SQUARES_PER_FACE;
//
	//cube_t cube = {0};
//
	//// front
	//face_t front = {0};
	//int count = 0;
	//for (int i = 0; i < TEXTURE_WIDTH; i++) {
		//for (int j = 0; j < TEXTURE_WIDTH; j++) {
			//square_t square = {0};
			//square.coords[0] = (vec3_t) {x + (i * len), y - (j * len), z};
			//square.coords[1] = (vec3_t) {x + ((i + 1) * len), y - (j * len), z};
			//square.coords[2] = (vec3_t) {x + ((i + 1) * len), y - ((j + 1) * len), z};
			//square.coords[3] = (vec3_t) {x + (i * len), y - ((j + 1) * len), z};
//
			//// 2, as the side face textures are the 2nd square of the texture image
			//colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
			//square.colour = pack_colour_to_uint32(&c);
//
			//front.squares[count++] = square;
		//}
	//}
//
	//// back
	//face_t back = {0};
	//count = 0;
	//for (int i = 0; i < TEXTURE_WIDTH; i++) {
		//for (int j = 0; j < TEXTURE_WIDTH; j++) {
			//square_t square = {0};
//
			//square.coords[0] = (vec3_t) {x + (i * len), y - (j * len), z + CUBE_WIDTH};
			//square.coords[1] = (vec3_t) {x + ((i + 1) * len), y - (j * len), z + CUBE_WIDTH};
			//square.coords[2] = (vec3_t) {x + ((i + 1) * len), y - ((j + 1) * len), z + CUBE_WIDTH};
			//square.coords[3] = (vec3_t) {x + (i * len), y - ((j + 1) * len), z + CUBE_WIDTH};
//
			//colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
			//square.colour = pack_colour_to_uint32(&c);
//
			//back.squares[count++] = square;
		//}
	//}
//
	//// left
	//face_t left = {0};
	//count = 0;
	//for (int i = 0; i < TEXTURE_WIDTH; i++) {
		//for (int j = 0; j < TEXTURE_WIDTH; j++) {
			//square_t square = {0};
//
			//square.coords[0] = (vec3_t) {x, y - (j * len), z + (i * len)};
			//square.coords[1] = (vec3_t) {x, y - (j * len), z + ((i + 1) * len)};
			//square.coords[2] = (vec3_t) {x, y - ((j + 1) * len), z + ((i + 1) * len)};
			//square.coords[3] = (vec3_t) {x, y - ((j + 1) * len), z + (i * len)};
//
			//colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
			//square.colour = pack_colour_to_uint32(&c);
//
			//left.squares[count++] = square;
		//}
	//}
//
	//// right
	//face_t right = {0};
	//count = 0;
	//for (int i = 0; i < TEXTURE_WIDTH; i++) {
		//for (int j = 0; j < TEXTURE_WIDTH; j++) {
			//square_t square = {0};
//
			//square.coords[0] = (vec3_t) {x + CUBE_WIDTH, y - (j * len), z + (i * len)};
			//square.coords[1] = (vec3_t) {x + CUBE_WIDTH, y - (j * len), z + ((i + 1) * len)};
			//square.coords[2] = (vec3_t) {x + CUBE_WIDTH, y - ((j + 1) * len), z + ((i + 1) * len)};
			//square.coords[3] = (vec3_t) {x + CUBE_WIDTH, y - ((j + 1) * len), z + (i * len)};
//
			//colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
			//square.colour = pack_colour_to_uint32(&c);
//
			//right.squares[count++] = square;
		//}
	//}
//
	//// top
	//face_t top = {0};
	//count = 0;
	//for (int i = 0; i < TEXTURE_WIDTH; i++) {
		//for (int j = 0; j < TEXTURE_WIDTH; j++) {
			//square_t square = {0};
//
			//square.coords[0] = (vec3_t) {x + (i * len), y, z + (j * len)};
			//square.coords[1] = (vec3_t) {x + ((i + 1) * len), y, z + (j * len)};
			//square.coords[2] = (vec3_t) {x + ((i + 1) * len), y, z + ((j + 1) * len)};
			//square.coords[3] = (vec3_t) {x + (i * len), y, z + ((j + 1) * len)};
//
			//// 0 as that is the top texture
			//colour_t c = texture->data[j * TEXTURE_WIDTH + i];
			//square.colour = pack_colour_to_uint32(&c);
//
			//top.squares[count++] = square;
		//}
	//}
//
	//// bottom
	//face_t bottom = {0};
	//count = 0;
	//for (int i = 0; i < TEXTURE_WIDTH; i++) {
		//for (int j = 0; j < TEXTURE_WIDTH; j++) {
			//square_t square = {0};
//
			//square.coords[0] = (vec3_t) {x + (i * len), y - CUBE_WIDTH, z + (j * len)};
			//square.coords[1] = (vec3_t) {x + ((i + 1) * len), y - CUBE_WIDTH, z + (j * len)};
			//square.coords[2] = (vec3_t) {x + ((i + 1) * len), y - CUBE_WIDTH, z + ((j + 1) * len)};
			//square.coords[3] = (vec3_t) {x + (i * len), y - CUBE_WIDTH, z + ((j + 1) * len)};
//
			//// 1, as the top face textures are the first square of the texture image
			//colour_t c = texture->data[1 * texture_side + j * TEXTURE_WIDTH + i];
			//square.colour = pack_colour_to_uint32(&c);
//
			//bottom.squares[count++] = square;
		//}
	//}
//
	//cube.faces[TOP] = top;
	//cube.faces[TOP].dir = TOP;
//
	//cube.faces[FRONT] = front;
	//cube.faces[FRONT].dir = FRONT;
//
	//cube.faces[LEFT] = left;
	//cube.faces[LEFT].dir = LEFT;
//
	//cube.faces[BACK] = back;
	//cube.faces[BACK].dir = BACK;
//
	//cube.faces[RIGHT] = right;
	//cube.faces[RIGHT].dir = RIGHT;
//
	//cube.faces[BOTTOM] = bottom;
	//cube.faces[BOTTOM].dir = BOTTOM;
//
	//array->items[array->count++] = cube;
//
	//return;
}

void clear_screen(colour_t colour) {
	uint32_t c = pack_colour_to_uint32(&colour);
	for (int i = 0; i < WIDTH * HEIGHT; i++) {
		pixels[i] = c;
	}
	return;
}

void render_chunks() {

	double closest_r = 999999999;
	highlighted_cube_face = -1;
	int draw_highlight_index = -1;

	draw_faces.count = 0;

	// for each chunk
	for (int chunk_i = 0; chunk_i < NUM_CHUNKS; chunk_i++) {

		//for each cube
		for (int cube_i = 0; cube_i < CUBES_PER_CHUNK; cube_i++) {

			if (chunks[chunk_i].cubes[cube_i].texture == NULL) {
				continue;
			}

			texture_t *texture = chunks[chunk_i].cubes[cube_i].texture;

			// only draw faces closest to camera
			int top = 0;
			int bottom = 0;
			int front = 0;
			int back = 0;
			int left = 0;
			int right = 0;

			// top left coord
			int x1 = chunks[chunk_i].pos.x + ((cube_i % CHUNK_WIDTH) * CUBE_WIDTH);
			int y1 = chunks[chunk_i].pos.y + (((cube_i / CHUNK_WIDTH) % CHUNK_WIDTH) * CUBE_WIDTH);
			int z1 = chunks[chunk_i].pos.z + (((cube_i / (CHUNK_WIDTH * CHUNK_WIDTH)) % CHUNK_WIDTH) * CUBE_WIDTH);

			// bottom right coord
			int x_two = x1 + CUBE_WIDTH;
			int y_two = y1 - CUBE_WIDTH;
			int z_two = z1 + CUBE_WIDTH;

			if (abs(camera_pos.x - x_two) < abs(camera_pos.x - x1)) {
				// draw right side
				right = 1;
			}
			else {
				// draw left side
				left = 1;
			}
			if (abs(camera_pos.z - z_two) < abs(camera_pos.z - z1)) {
				// draw back side
				back = 1;
			}
			else {
				// draw front side
				front = 1;
			}
			if (abs(camera_pos.y - y_two) > abs(camera_pos.y - y1)) {
				// draw bottom side
				top = 1;
			}
			else {
				// draw top side
				bottom = 1;
			}

			for (int face_i = 0; face_i < 6; face_i++) {
				int len = CUBE_WIDTH / TEXTURE_WIDTH;

				int texture_side = SQUARES_PER_FACE;

				int pos_highlight = 0;

				face_t face = {0};

				switch (face_i) {
					case TOP: {
						if (! top) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].top_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - camera_pos.x;
						int y = y1 - camera_pos.y;
						int z = z1 - camera_pos.z;

						// central coord
						double x2 = x + (CUBE_WIDTH / 2);
						double y2 = y;
						double z2 = z + (CUBE_WIDTH / 2);

						// calc distance to camera
						double r = sqrt((x2 * x2) + (y2 * y2) + (z2 * z2));

						face.r = r;

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x + (i * len), y, z + (j * len)});
								square.coords[1] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y, z + (j * len)});
								square.coords[2] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y, z + ((j + 1) * len)});
								square.coords[3] = rotate_and_project((vec3_t) {x + (i * len), y, z + ((j + 1) * len)});

								// 0 as that is the top texture
								colour_t c = texture->data[j * TEXTURE_WIDTH + i];
								square.colour = pack_colour_to_uint32(&c);

								face.squares[count++] = square;

								if (r < REACH) {
									if (square_surrounds_centre(&square)) {
										pos_highlight = 1;
									}
								}
							}
						}
						draw_faces.items[draw_faces.count++] = face;
					    break;
					}
					case BOTTOM: {
						if (! bottom) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].bottom_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - camera_pos.x;
						int y = y1 - camera_pos.y;
						int z = z1 - camera_pos.z;

						// central coord
						double x2 = x + (CUBE_WIDTH / 2);
						double y2 = y;
						double z2 = z + (CUBE_WIDTH / 2);

						// calc distance to camera
						double r = sqrt((x2 * x2) + (y2 * y2) + (z2 * z2));

						face.r = r;

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x + (i * len), y - CUBE_WIDTH, z + (j * len)});
								square.coords[1] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y - CUBE_WIDTH, z + (j * len)});
								square.coords[2] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y - CUBE_WIDTH, z + ((j + 1) * len)});
								square.coords[3] = rotate_and_project((vec3_t) {x + (i * len), y - CUBE_WIDTH, z + ((j + 1) * len)});

								// 1, as the top face textures are the first square of the texture image
								colour_t c = texture->data[1 * texture_side + j * TEXTURE_WIDTH + i];
								square.colour = pack_colour_to_uint32(&c);

								face.squares[count++] = square;

								if (r < REACH) {
									if (square_surrounds_centre(&square)) {
										pos_highlight = 1;
									}
								}
							}
						}
						draw_faces.items[draw_faces.count++] = face;
					    break;
					}
					case FRONT: {
						if (! front) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].front_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - camera_pos.x;
						int y = y1 - camera_pos.y;
						int z = z1 - camera_pos.z;

						// central coord
						double x2 = x + (CUBE_WIDTH / 2);
						double y2 = y - (CUBE_WIDTH / 2);
						double z2 = z;

						// calc distance to camera
						double r = sqrt((x2 * x2) + (y2 * y2) + (z2 * z2));

						face.r = r;

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x + (i * len), y - (j * len), z});
								square.coords[1] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y - (j * len), z});
								square.coords[2] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y - ((j + 1) * len), z});
								square.coords[3] = rotate_and_project((vec3_t) {x + (i * len), y - ((j + 1) * len), z});

								// 2, as the side face textures are the 2nd square of the texture image
								colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
								square.colour = pack_colour_to_uint32(&c);

								face.squares[count++] = square;

								if (r < REACH) {
									if (square_surrounds_centre(&square)) {
										pos_highlight = 1;
									}
								}
							}
						}
						draw_faces.items[draw_faces.count++] = face;
					    break;
					}
					case BACK: {
						if (! back) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].back_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - camera_pos.x;
						int y = y1 - camera_pos.y;
						int z = z1 - camera_pos.z;

						// central coord
						double x2 = x + (CUBE_WIDTH / 2);
						double y2 = y - (CUBE_WIDTH / 2);
						double z2 = z;

						// calc distance to camera
						double r = sqrt((x2 * x2) + (y2 * y2) + (z2 * z2));

						face.r = r;

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x + (i * len), y - (j * len), z + CUBE_WIDTH});
								square.coords[1] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y - (j * len), z + CUBE_WIDTH});
								square.coords[2] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y - ((j + 1) * len), z + CUBE_WIDTH});
								square.coords[3] = rotate_and_project((vec3_t) {x + (i * len), y - ((j + 1) * len), z + CUBE_WIDTH});

								colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
								square.colour = pack_colour_to_uint32(&c);

								face.squares[count++] = square;

								if (r < REACH) {
									if (square_surrounds_centre(&square)) {
										pos_highlight = 1;
									}
								}
							}
						}
						draw_faces.items[draw_faces.count++] = face;
					    break;
					}
					case LEFT: {
						if (! left) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].left_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - camera_pos.x;
						int y = y1 - camera_pos.y;
						int z = z1 - camera_pos.z;

						// central coord
						double x2 = x + (CUBE_WIDTH / 2);
						double y2 = y - (CUBE_WIDTH / 2);
						double z2 = z + (CUBE_WIDTH / 2);

						// calc distance to camera
						double r = sqrt((x2 * x2) + (y2 * y2) + (z2 * z2));

						face.r = r;

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x, y - (j * len), z + (i * len)});
								square.coords[1] = rotate_and_project((vec3_t) {x, y - (j * len), z + ((i + 1) * len)});
								square.coords[2] = rotate_and_project((vec3_t) {x, y - ((j + 1) * len), z + ((i + 1) * len)});
								square.coords[3] = rotate_and_project((vec3_t) {x, y - ((j + 1) * len), z + (i * len)});

								colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
								square.colour = pack_colour_to_uint32(&c);

								face.squares[count++] = square;
								 
								if (r < REACH) {
									if (square_surrounds_centre(&square)) {
										pos_highlight = 1;
									}
								}
							}
						}
						draw_faces.items[draw_faces.count++] = face;
					    break;
					}
					case RIGHT: {
						if (! right) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].right_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - camera_pos.x;
						int y = y1 - camera_pos.y;
						int z = z1 - camera_pos.z;

						// central coord
						double x2 = x + (CUBE_WIDTH / 2);
						double y2 = y - (CUBE_WIDTH / 2);
						double z2 = z + (CUBE_WIDTH / 2);

						// calc distance to camera
						double r = sqrt((x2 * x2) + (y2 * y2) + (z2 * z2));

						face.r = r;

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x + CUBE_WIDTH, y - (j * len), z + (i * len)});
								square.coords[1] = rotate_and_project((vec3_t) {x + CUBE_WIDTH, y - (j * len), z + ((i + 1) * len)});
								square.coords[2] = rotate_and_project((vec3_t) {x + CUBE_WIDTH, y - ((j + 1) * len), z + ((i + 1) * len)});
								square.coords[3] = rotate_and_project((vec3_t) {x + CUBE_WIDTH, y - ((j + 1) * len), z + (i * len)});

								colour_t c = texture->data[2 * texture_side + j * TEXTURE_WIDTH + i];
								square.colour = pack_colour_to_uint32(&c);

								face.squares[count++] = square;

								if (r < REACH) {
									if (square_surrounds_centre(&square)) {
										pos_highlight = 1;
									}
								}
							}
						}
						draw_faces.items[draw_faces.count++] = face;
					    break;
					}
				}

				if (pos_highlight) {
					// if it is, check that square.r is closest r
					if (face.r < closest_r) {
						closest_r = face.r;

						highlighted_cube_index = cube_i;
						highlighted_cube_chunk_index = chunk_i;

						// find the face we have highlighted
						highlighted_cube_face = face_i;
						draw_highlight_index = draw_faces.count - 1;
					}
				}
			}
		}

	}

	// highlight cube
	if (draw_highlight_index > -1) {
		for (int k = 0; k < SQUARES_PER_FACE; k++) {
			colour_t colour = unpack_colour_from_uint32(draw_faces.items[draw_highlight_index].squares[k].colour);
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
			draw_faces.items[draw_highlight_index].squares[k].colour =  pack_colour_to_uint32(&colour);
		}
	}

	// sort the faces based on their distance to the camera
	qsort(draw_faces.items, draw_faces.count, sizeof(face_t), compare_faces);
}

int compare_faces(const void *one, const void *two) {
	const face_t *face_one = one;
	const face_t *face_two = two;

	double r1 = face_one->r;
	double r2 = face_two->r;

	if (r1 > r2) {
		return -1;
	}
	if (r1 < r2) {
		return 1;
	}

	return 0;
}

vec3_t rotate_and_project(vec3_t pos) {

	// rotate
	vec3_t new_pos;
	new_pos.x = (pos.z * sin(x_rotation) + pos.x * cos(x_rotation));
	int z2 = (pos.z * cos(x_rotation) - pos.x * sin(x_rotation));

	int neg = 0;
	new_pos.y = (z2 * sin(y_rotation) + pos.y * cos(y_rotation));
	new_pos.z = (z2 * cos(y_rotation) - pos.y * sin(y_rotation));
	if (new_pos.z <= 0) {
		neg = 1;
		// avoid divide by 0
		new_pos.z = 100;
	}

	// project
	float percent_size = ((float)((WIDTH / 10) * FOV) / new_pos.z);
	new_pos.x *= percent_size;
	new_pos.y *= percent_size;

	// convert from standard grid to screen grid
	new_pos.x = new_pos.x + WIDTH / 2;
	new_pos.y = -new_pos.y + HEIGHT / 2;

	if (neg) {
		new_pos.z = 0;
	}
	return new_pos;
}

void draw_all_faces() {
	for (int i = 0; i < draw_faces.count; i++) {
		for (int j = 0; j < SQUARES_PER_FACE; j++) {
			fill_square(&draw_faces.items[i].squares[j]);
		}
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
	//int x = hotbar_x;
	//int y = hotbar_y;
//
	//int w = hotbar_width;
	//int h = hotbar_height;
    //draw_rect((vec3_t){x, y, 1}, w, small_height, hotbar_colour);
	//y -= h;
    //draw_rect((vec3_t){x, y, 1}, w, small_height, hotbar_colour);
//
    //draw_rect((vec3_t){x, y, 1}, small_height, h, hotbar_colour);
	//x += w;
    //draw_rect((vec3_t){x, y, 1}, small_height, h, hotbar_colour);
//
	//switch(hotbar_selection) {
		//case 0: {
			//draw_rect((vec3_t){hotbar_x + (HOTBAR_SLOT_WIDTH * 0), hotbar_y - hotbar_height, 1}, HOTBAR_SLOT_WIDTH, hotbar_height, hotbar_colour);
			//break;
		//}
		//case 1: {
			//draw_rect((vec3_t){hotbar_x + (HOTBAR_SLOT_WIDTH * 1), hotbar_y - hotbar_height, 1}, HOTBAR_SLOT_WIDTH, hotbar_height, hotbar_colour);
			//break;
		//}
		//case 2: {
			//draw_rect((vec3_t){hotbar_x + (HOTBAR_SLOT_WIDTH * 2), hotbar_y - hotbar_height, 1}, HOTBAR_SLOT_WIDTH, hotbar_height, hotbar_colour);
			//break;
		//}
		//case 3: {
			//draw_rect((vec3_t){hotbar_x + (HOTBAR_SLOT_WIDTH * 3), hotbar_y - hotbar_height, 1}, HOTBAR_SLOT_WIDTH, hotbar_height, hotbar_colour);
			//break;
		//}
	//}
//
	//for (int i = 0; i < hotbar_faces.count; i++) {
		//for (int j = 0; j < SQUARES_PER_FACE; j++) {
			//fill_square(&hotbar_faces.items[i].squares[j]);
		//}
	//}
}

void draw_hand() {
	//for (int i = 0; i < hand_faces.count; i++) {
		//for (int j = 0; j < SQUARES_PER_FACE; j++) {
			//fill_square(&hand_faces.items[i].squares[j]);
		//}
	//}
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

	int neg_z_count = 0;

	for (int i = 0; i < 4; i++) {

		if (square->coords[i].z == 0) {
			neg_z_count++;
		}

		if (square->coords[i].y < smallest_y) {
			smallest_y = square->coords[i].y;
			smallest_y_index = i;
		}
		if (square->coords[i].y > largest_y) {
			largest_y = square->coords[i].y;
		}

	}

	if (neg_z_count == 4) {
		return;
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

	// TODO: fix this fix
	//if (largest_y - smallest_y < 5) {
		//if (smallest_y > 1) {
			//smallest_y -= 1;
		//}
		//if (largest_y < HEIGHT - 1) {
			//largest_y += 1;
		//}
	//}

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

// TODO:
void place_cube(texture_t *texture) {
	//vec3_t pos = {0};
	//pos.x = chunks[highlighted_cube_chunk_index].cubes[highlighted_cube_index].faces[0].squares[0].coords[0].x;
	//pos.y = chunks[highlighted_cube_chunk_index].cubes[highlighted_cube_index].faces[0].squares[0].coords[0].y;
	//pos.z = chunks[highlighted_cube_chunk_index].cubes[highlighted_cube_index].faces[0].squares[0].coords[0].z;
	//switch (highlighted_cube_face) {
		//case TOP: {
		    //pos.y += CUBE_WIDTH;
			//break;
		//}
		//case FRONT: {
			//pos.z -= CUBE_WIDTH;
			//break;
		//}
		//case LEFT: {
		    //pos.x -= CUBE_WIDTH;
			//break;
		//}
		//case BACK: {
			//pos.z += CUBE_WIDTH;
			//break;
		//}
		//case RIGHT: {
		    //pos.x += CUBE_WIDTH;
			//break;
		//}
		//case BOTTOM: {
		    //pos.y -= CUBE_WIDTH;
			//break;
		//}
	//}
	//int x1 = pos.x;
	//int y1 = pos.y;
	//int z1 = pos.z;
//
	//// x2 = bottom right back
	//int x2 = x1 + CUBE_WIDTH;	
	//int y2 = y1 - CUBE_WIDTH;	
	//int z2 = z1 + CUBE_WIDTH;
//
	//camera_pos.y -= CUBE_WIDTH;
	//// player_x1 = top left front
	//int player_x1 = camera_pos.x - player_width;
	//int player_y1 = camera_pos.y + player_height;
	//int player_z1 = camera_pos.z - player_width;
//
	//// player_x1 = bottom right back
	//int player_x2 = camera_pos.x + player_width;
	//int player_y2 = camera_pos.y - player_width;
	//int player_z2 = camera_pos.z + player_width;
//
	//int x_collision = 0;
	//int y_collision = 0;
	//int z_collision = 0;
	//if ((player_x1 >= x1 && player_x1 <= x2) || (player_x2 >= x1 && player_x2 <= x2)) {
		//// xs overlap
		//x_collision = 1;
	//}
	//if ((player_y1 <= y1 && player_y1 >= y2) || (player_y2 <= y1 && player_y2 >= y2)) {
		//// ys overlap
		//y_collision = 1;
	//}
	//if ((player_z1 >= z1 && player_z1 <= z2) || (player_z2 >= z1 && player_z2 <= z2)) {
		//// zs overlap
		//z_collision = 1;
	//}
	//camera_pos.y += CUBE_WIDTH;
	//if (x_collision && y_collision && z_collision) {
		//return;
	//}
//
	//return;
}

//TODO;
void remove_cube() {
	//vec3_t pos = {0};
	//int x = chunks[highlighted_cube_chunk_index].cubes[highlighted_cube_index].faces[0].squares[0].coords[0].x;
	//int y = chunks[highlighted_cube_chunk_index].cubes[highlighted_cube_index].faces[0].squares[0].coords[0].y;
	//int z = chunks[highlighted_cube_chunk_index].cubes[highlighted_cube_index].faces[0].squares[0].coords[0].z;
//
	//int count = highlighted_cube_index;
	//for (int i = highlighted_cube_index + 1; i < chunks[highlighted_cube_chunk_index].cube_count; i++) {
		//chunks[highlighted_cube_chunk_index].cubes[count] = chunks[highlighted_cube_chunk_index].cubes[i];
		//count++;	
	//}
	//chunks[highlighted_cube_chunk_index].cube_count--;
//
	//// update neighbours:
	//for (int i = 0; i < chunks[highlighted_cube_chunk_index].cube_count; i++) {
		//int x1 = chunks[highlighted_cube_chunk_index].cubes[i].faces[0].squares[0].coords[0].x;
		//int y1 = chunks[highlighted_cube_chunk_index].cubes[i].faces[0].squares[0].coords[0].y;
		//int z1 = chunks[highlighted_cube_chunk_index].cubes[i].faces[0].squares[0].coords[0].z;
//
		//if (y == y1 && z == z1) {
			//if (chunks[highlighted_cube_chunk_index].cubes[i].faces[0].squares[0].coords[0].x - CUBE_WIDTH == x) {
				//chunks[highlighted_cube_chunk_index].cubes[i].faces[LEFT].neighbour = 0;	
			//}
			//if (chunks[highlighted_cube_chunk_index].cubes[i].faces[0].squares[0].coords[0].x + CUBE_WIDTH == x) {
				//chunks[highlighted_cube_chunk_index].cubes[i].faces[RIGHT].neighbour = 0;	
			//}
		//}
//
		//if (x == x1 && z == z1) {
			//if (chunks[highlighted_cube_chunk_index].cubes[i].faces[0].squares[0].coords[0].y - CUBE_WIDTH == y) {
				//chunks[highlighted_cube_chunk_index].cubes[i].faces[BOTTOM].neighbour = 0;	
			//}
			//if (chunks[highlighted_cube_chunk_index].cubes[i].faces[0].squares[0].coords[0].y + CUBE_WIDTH == y) {
				//chunks[highlighted_cube_chunk_index].cubes[i].faces[TOP].neighbour = 0;	
			//}
		//}
//
		//if (x == x1 && y == y1) {
			//if (chunks[highlighted_cube_chunk_index].cubes[i].faces[0].squares[0].coords[0].z - CUBE_WIDTH == z) {
				//chunks[highlighted_cube_chunk_index].cubes[i].faces[FRONT].neighbour = 0;	
			//}
			//if (chunks[highlighted_cube_chunk_index].cubes[i].faces[0].squares[0].coords[0].z + CUBE_WIDTH == z) {
				//chunks[highlighted_cube_chunk_index].cubes[i].faces[BACK].neighbour = 0;	
			//}
		//}
	//}
	//return;
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
			jump = jump_height;
		}
		//y += speed;
	}
    if (keys[shift]) {
        y += - speed;
	}
	if (keys[escape]) {
		hold_mouse = 0;
	}

	y += gravity;
	y += jump / 5;
	if (jump > 0) {
		jump -= (jump_height * 0.04);
	}
	else {
		jump = 0;
	}

	// make the player 2 cubes tall:
	camera_pos.y -= CUBE_WIDTH;

	// TODO: could compute 1 / cube width and 1 / chunk width to speed this up
	int old = ((camera_pos.x / CUBE_WIDTH) / CHUNK_WIDTH);
	int new = old;
    camera_pos.x += x;
	if (collided()) {
		camera_pos.x -= x;
	}
	else {
		new = ((camera_pos.x / CUBE_WIDTH) / CHUNK_WIDTH);
		if (old != new) {
			if (camera_pos.x < camera_pos.x - x) {
				update_chunks(X_NEG);
			}
			else {
				update_chunks(X_POS);
			}
		}
	}

    camera_pos.y += y;
	if (collided()) {
		camera_pos.y -= y;
	}

	old = ((camera_pos.z / CUBE_WIDTH) / CHUNK_WIDTH);
	new = old;
    camera_pos.z += z;
	if (collided()) {
		camera_pos.z -= z;
	}
	else {
		new = ((camera_pos.z / CUBE_WIDTH) / CHUNK_WIDTH);
		if (old != new) {
			if (camera_pos.z < camera_pos.z - z) {
				update_chunks(Z_NEG);
			}
			else {
				update_chunks(Z_POS);
			}
		}
	}
	
	camera_pos.y += CUBE_WIDTH;

	//vec3_t pos = {camera_pos.x + 2 * CUBE_WIDTH, camera_pos.y - 0.5 * CUBE_WIDTH, camera_pos.z + 0.5 * CUBE_WIDTH};
	//if (keys[one]) {
		//hotbar_selection = 0;
		//hand_cubes.count = 0;
		//add_cube_to_cubes_array(pos, grass_texture, &hand_cubes);
		//render_hand();
	//}
	//if (keys[two]) {
		//hotbar_selection = 1;
		//hand_cubes.count = 0;
		//add_cube_to_cubes_array(pos, stone_texture, &hand_cubes);
		//render_hand();
	//}
	//if (keys[three]) {
		//hotbar_selection = 2;
		//hand_cubes.count = 0;
		//add_cube_to_cubes_array(pos, wood_texture, &hand_cubes);
		//render_hand();
	//}
	//if (keys[four]) {
		//hotbar_selection = 3;
		//hand_cubes.count = 0;
		//add_cube_to_cubes_array(pos, leaf_texture, &hand_cubes);
		//render_hand();
	//}
}

void handle_mouse() {
	if (mouse_left_click) {
		hold_mouse = 1;
		mouse_was_left_clicked = 1;
	}
	else {
		if (mouse_was_left_clicked) {
			if (highlighted_cube_face > -1) {
				remove_cube();
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
			if (highlighted_cube_face > -1) {
				switch (hotbar_selection) {
					case 0: {
						place_cube(grass_texture);
						break;
					}
					case 1: {
						place_cube(stone_texture);
						break;
					}
					case 2: {
						place_cube(wood_texture);
						break;
					}
					case 3: {
						place_cube(leaf_texture);
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

// TODO:
int collided() {
	for (int cube_i = 0; cube_i < CUBES_PER_CHUNK; cube_i++) {
		if (chunks[occupied_chunk_index].cubes[cube_i].texture == NULL) {
			continue;
		}
		// x1 = top left front
		int x1 = chunks[occupied_chunk_index].pos.x + ((cube_i % CHUNK_WIDTH) * CUBE_WIDTH);
		int y1 = chunks[occupied_chunk_index].pos.y + (((cube_i / CHUNK_WIDTH) % CHUNK_WIDTH) * CUBE_WIDTH);
		int z1 = chunks[occupied_chunk_index].pos.z + (((cube_i / (CHUNK_WIDTH * CHUNK_WIDTH)) % CHUNK_WIDTH) * CUBE_WIDTH);

		// x2 = bottom right back
		int x2 = x1 + CUBE_WIDTH;	
		int y2 = y1 - CUBE_WIDTH;	
		int z2 = z1 + CUBE_WIDTH;

		// player_x1 = top left front
		int player_x1 = camera_pos.x - player_width;
		int player_y1 = camera_pos.y + player_height;
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
	//free(hand_cubes.items);
	//free(hand_faces.items);
	//free(hotbar_cubes.items);
	//free(hotbar_faces.items);
    free(pixels);
}

void render_hotbar() {
	//// render the hotbar squares:
	//hotbar_faces.count = 0;
//
	//// for each cube
	//for (int i = 0; i < hotbar_cubes.count; i++) {
//
		//for (int j = 0; j < 6; j++) {
//
			//// top left coord
			//int x1 = hotbar_cubes.items[i].faces[j].squares[0].coords[0].x;
			//int y1 = hotbar_cubes.items[i].faces[j].squares[0].coords[0].y;
			//int z1 = hotbar_cubes.items[i].faces[j].squares[0].coords[0].z;
//
			//// bottom right coord
			//x1 += hotbar_cubes.items[i].faces[j].squares[SQUARES_PER_FACE - 1].coords[0].x;
			//y1 += hotbar_cubes.items[i].faces[j].squares[SQUARES_PER_FACE - 1].coords[0].y;
			//z1 += hotbar_cubes.items[i].faces[j].squares[SQUARES_PER_FACE - 1].coords[0].z;
//
			//x1 /= 2;
			//y1 /= 2;
			//z1 /= 2;
//
			//// calc distance to camera
			//int r = sqrt((x1 * x1) + (y1 * y1) + (z1 * z1));
//
			//face_t new_face = {0};
			//new_face.r = r;
//
			//// for each square of each face of the cube
			//for (int k = 0; k < SQUARES_PER_FACE; k++) {
//
			    //square_t new_square = {0};
				//for (int l = 0; l < 4; l++) {
					//vec3_t pos = {0};
					//pos.x = hotbar_cubes.items[i].faces[j].squares[k].coords[l].x;
					//pos.y = hotbar_cubes.items[i].faces[j].squares[k].coords[l].y;
					//pos.z = hotbar_cubes.items[i].faces[j].squares[k].coords[l].z;
					//pos.x -= camera_pos.x;
					//pos.y -= camera_pos.y;
					//pos.z -= camera_pos.z;
//
					//vec3_t new_pos = {0};
					//rotate_and_project_by_rot_value(&pos, &new_pos, 0, 0);
//
					//new_square.coords[l].x = new_pos.x - (WIDTH / 2) + ((i + 1.2) * HOTBAR_SLOT_WIDTH);
					//new_square.coords[l].y = new_pos.y + (HEIGHT / 2) - (HEIGHT - hotbar_y) - HOTBAR_SLOT_WIDTH;
					//new_square.coords[l].z = new_pos.z;
					//new_square.colour = hotbar_cubes.items[i].faces[j].squares[k].colour;
				//}
//
				//new_face.squares[k] = new_square;
			//}
//
			//hotbar_faces.items[hotbar_faces.count++] = new_face;
		//}
	//}
//
	//// sort the faces based on their distance to the camera
	//qsort(hotbar_faces.items, hotbar_faces.count, sizeof(face_t), compare_faces);
}

void render_hand() {
	//// render the hand squares:
	//hand_faces.count = 0;
//
	//// for each cube
	//for (int i = 0; i < hand_cubes.count; i++) {
//
		//for (int j = 0; j < 6; j++) {
//
			//// top left coord
			//int x1 = hand_cubes.items[i].faces[j].squares[0].coords[0].x - camera_pos.x;
			//int y1 = hand_cubes.items[i].faces[j].squares[0].coords[0].y - camera_pos.y;
			//int z1 = hand_cubes.items[i].faces[j].squares[0].coords[0].z - camera_pos.z;
//
			//// bottom right coord
			//x1 += hand_cubes.items[i].faces[j].squares[SQUARES_PER_FACE - 1].coords[0].x - camera_pos.x;
			//y1 += hand_cubes.items[i].faces[j].squares[SQUARES_PER_FACE - 1].coords[0].y - camera_pos.y;
			//z1 += hand_cubes.items[i].faces[j].squares[SQUARES_PER_FACE - 1].coords[0].z - camera_pos.z;
//
			//x1 /= 2;
			//y1 /= 2;
			//z1 /= 2;
//
			//// calc distance to camera
			//int r = sqrt((x1 * x1) + (y1 * y1) + (z1 * z1));
//
			//face_t new_face = {0};
			//new_face.r = r;
//
			//// for each square of each face of the cube
			//for (int k = 0; k < SQUARES_PER_FACE; k++) {
//
			    //square_t new_square = {0};
				//for (int l = 0; l < 4; l++) {
					//vec3_t pos = {0};
					//pos.x = hand_cubes.items[i].faces[j].squares[k].coords[l].x;
					//pos.y = hand_cubes.items[i].faces[j].squares[k].coords[l].y;
					//pos.z = hand_cubes.items[i].faces[j].squares[k].coords[l].z;
					//pos.x -= camera_pos.x;
					//pos.y -= camera_pos.y;
					//pos.z -= camera_pos.z;
//
					//vec3_t new_pos = {0};
					//rotate_and_project_by_rot_value(&pos, &new_pos, 0, 0);
//
					//new_square.coords[l].x = new_pos.x;
					//new_square.coords[l].y = new_pos.y;
					//new_square.coords[l].z = new_pos.z;
					//new_square.colour = hand_cubes.items[i].faces[j].squares[k].colour;
				//}
//
				//new_face.squares[k] = new_square;
			//}
//
			//hand_faces.items[hand_faces.count++] = new_face;
		//}
	//}
//
	//// sort the faces based on their distance to the camera
	//qsort(hand_faces.items, hand_faces.count, sizeof(face_t), compare_faces);
}

void rotate_and_project_by_rot_value(vec3_t *pos, vec3_t *new_pos, float x_rot, float y_rot) {

	// rotate
	new_pos->x = (pos->z * sin(x_rot) + pos->x * cos(x_rot));
	int z2 = (pos->z * cos(x_rot) - pos->x * sin(x_rot));

	new_pos->y = (z2 * sin(y_rot) + pos->y * cos(y_rot));
	new_pos->z = (z2 * cos(y_rot) - pos->y * sin(y_rot));
	if (new_pos->z == 0) {
		new_pos->z = 1;
	}

	// project
	float percent_size = ((float)((WIDTH / 10) * FOV) / new_pos->z);
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

void generate_textures() {
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
	
	// create dirt texture
	// top bottom side
	i = 0;
	for (i; i < SQUARES_PER_FACE * 3; i++) {
		myImg.data[i] = (colour_t){150 + (rand() % 50), 75 + (rand() % 50), 10 + (rand() % 50), };
	}

    writePPM("dirt.ppm", &myImg);

	dirt_texture = readPPM("dirt.ppm");
	assert(dirt_texture != NULL);

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
}

// TODO:
void update_chunks(int dir) {
	switch (dir) {
		case Z_POS: {
			if ((occupied_chunk_index + 1) % CHUNK_WIDTH == 0) {
				occupied_chunk_index -= (CHUNK_WIDTH - 1);
			}
			else {
				occupied_chunk_index += 1;
			}
			for (int i = 0; i < NUM_CHUNKS; i++) {
				if (camera_pos.z - chunks[i].pos.z > (sqrt(NUM_CHUNKS) - 1) * CHUNK_WIDTH * CUBE_WIDTH) {
					chunks[i].pos.z += sqrt(NUM_CHUNKS) * CUBE_WIDTH * CHUNK_WIDTH;
					generate_chunk(&chunks[i]);
				}
			}
			break;
		}
		case Z_NEG: {
			if (occupied_chunk_index % CHUNK_WIDTH == 0) {
				occupied_chunk_index += (CHUNK_WIDTH - 1);
			}
			else {
				occupied_chunk_index -= 1;
			}
			for (int i = 0; i < NUM_CHUNKS; i++) {
				if (chunks[i].pos.z - camera_pos.z> CHUNK_WIDTH * CUBE_WIDTH) {
					chunks[i].pos.z -= sqrt(NUM_CHUNKS) * CUBE_WIDTH * CHUNK_WIDTH;
					generate_chunk(&chunks[i]);
				}
			}
			break;
		}
		case X_POS: {
			occupied_chunk_index += 3;
			if (occupied_chunk_index > (NUM_CHUNKS - 1)) {
				occupied_chunk_index -= NUM_CHUNKS;
			}
			for (int i = 0; i < NUM_CHUNKS; i++) {
				if (camera_pos.x - chunks[i].pos.x > (sqrt(NUM_CHUNKS) - 1) * CHUNK_WIDTH * CUBE_WIDTH) {
					chunks[i].pos.x += sqrt(NUM_CHUNKS) * CUBE_WIDTH * CHUNK_WIDTH;
					generate_chunk(&chunks[i]);
				}
			}
			break;
		}
		case X_NEG: {
			occupied_chunk_index -= 3;
			if (occupied_chunk_index < 0) {
				occupied_chunk_index += NUM_CHUNKS;
			}
			for (int i = 0; i < NUM_CHUNKS; i++) {
				if (chunks[i].pos.x - camera_pos.x> CHUNK_WIDTH * CUBE_WIDTH) {
					chunks[i].pos.x -= sqrt(NUM_CHUNKS) * CUBE_WIDTH * CHUNK_WIDTH;
					generate_chunk(&chunks[i]);
				}
			}
			break;
		}
	}
}

void generate_chunk(chunk_t *chunk) {

	texture_t *texture = grass_texture;

	for (int x = 0; x < CHUNK_WIDTH; x++) {
		for (int y = 0; y < CHUNK_WIDTH; y++) {
			for (int z = 0; z < CHUNK_WIDTH; z++) {

				float val = 0.00003;
				int perlin_y = floor(((perlin2D(&noise, (chunk->pos.x + (x * CUBE_WIDTH)) * val, (chunk->pos.z + (z * CUBE_WIDTH)) * val)) * 10));
				perlin_y += 5;

				int right = floor(((perlin2D(&noise, (chunk->pos.x + (x * CUBE_WIDTH) + CUBE_WIDTH) * val, (chunk->pos.z + (z * CUBE_WIDTH)) * val)) * 10));
				right += 5;
				int left = floor(((perlin2D(&noise, (chunk->pos.x + (x * CUBE_WIDTH) - CUBE_WIDTH) * val, (chunk->pos.z + (z * CUBE_WIDTH)) * val)) * 10));
				left += 5;
				int front = floor(((perlin2D(&noise, (chunk->pos.x + (x * CUBE_WIDTH)) * val, (chunk->pos.z + (z * CUBE_WIDTH) - CUBE_WIDTH) * val)) * 10));
				front += 5;
				int back = floor(((perlin2D(&noise, (chunk->pos.x + (x * CUBE_WIDTH)) * val, (chunk->pos.z + (z * CUBE_WIDTH) + CUBE_WIDTH) * val)) * 10));
				back += 5;

				if (perlin_y < 0) {
					perlin_y = 0;
				}
				if (left < 0) {
					left = 0;
				}
				if (right < 0) {
					right = 0;
				}
				if (front < 0) {
					front = 0;
				}
				if (back < 0) {
					back = 0;
				}

				bruh_top = 0;
				bruh_left = 0;
				bruh_right = 0;
				bruh_front = 0;
				bruh_back = 0;
				bruh_bottom = 1;

				if (front >= y) {
					bruh_front = 1;
				}
				if (back >= y) {
					bruh_back = 1;
				}
				if (right >= y) {
					bruh_right = 1;
				}
				if (left >= y) {
					bruh_left = 1;
				}
				bruh_top = 0;
				
				if (y > perlin_y) {
					// air
					texture = NULL;
				}
				else if (y == perlin_y) {
					// grass
					texture = grass_texture;
				}
				else {
					// dirt
					texture = dirt_texture;
					bruh_top = 1;
				}
				chunk->cubes[x + (y * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH)].texture = texture;
				chunk->cubes[x + (y * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH)].left_neighbour = bruh_left;
				chunk->cubes[x + (y * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH)].right_neighbour = bruh_right;
				chunk->cubes[x + (y * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH)].front_neighbour = bruh_front;
				chunk->cubes[x + (y * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH)].back_neighbour = bruh_back;
				chunk->cubes[x + (y * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH)].top_neighbour = bruh_top;
				chunk->cubes[x + (y * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH)].bottom_neighbour = bruh_bottom;
			}
		}
	}
	bruh_top = 0;
	bruh_bottom = 0;
	bruh_left = 0;
	bruh_right = 0;
	bruh_front = 0;
	bruh_back = 0;
}

//gpt perlin:
// Simple LCG random
static uint32_t lcg(uint32_t *state) {
    *state = (*state * 1664525u + 1013904223u);
    return *state;
}

void perlin_init(perlin_t *n, uint32_t seed)
{
    uint32_t state = seed;

    // Fill 0..255
    for (int i = 0; i < 256; i++)
        n->p[i] = i;

    // Fisher–Yates shuffle using seed
    for (int i = 255; i > 0; i--) {
        int j = lcg(&state) % (i + 1);
        unsigned char tmp = n->p[i];
        n->p[i] = n->p[j];
        n->p[j] = tmp;
    }

    // Duplicate
    for (int i = 0; i < 256; i++)
        n->p[256 + i] = n->p[i];
}

static double fade(double t) {
    return t*t*t*(t*(t*6-15)+10);
}

static double lerp(double a, double b, double t) {
    return a + t*(b-a);
}

static double grad(int h, double x, double y) {
    h &= 3;
    return ((h&1)?-x:x) + ((h&2)?-y:y);
}

double perlin2D(perlin_t *n, double x, double y)
{
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;

    x -= floor(x);
    y -= floor(y);

    double u = fade(x);
    double v = fade(y);

    int A = n->p[X] + Y;
    int B = n->p[X + 1] + Y;

    return lerp(
        lerp(grad(n->p[A], x, y),
             grad(n->p[B], x-1, y), u),
        lerp(grad(n->p[A+1], x, y-1),
             grad(n->p[B+1], x-1, y-1), u),
        v
    );
}

int square_surrounds_centre(square_t *square) {
	int left_x = WIDTH;
	int right_x = -WIDTH;
	int top_y = HEIGHT;
	int bottom_y = -HEIGHT;
	int count = 0;
	for (int i = 0; i < 4; i++) {
		if (square->coords[i].x < left_x) {
			left_x = square->coords[i].x;
		}	
		if (square->coords[i].x > right_x) {
			right_x = square->coords[i].x;
		}	
		if (square->coords[i].y > bottom_y) {
			bottom_y = square->coords[i].y;
		}
		if (square->coords[i].y < top_y) {
			top_y = square->coords[i].y;
		}
		if (square->coords[i].z < 0) {
			count++;
		}
	}
	if (count == 4) {
		return 0;
	}
	if (left_x <= WIDTH / 2 && right_x >= WIDTH / 2 && top_y <= HEIGHT / 2 && bottom_y >= HEIGHT / 2) {
		return 1;
	}
	return 0;
}
