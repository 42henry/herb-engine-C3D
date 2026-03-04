#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

/* -------------------------- TODO list -------------------------- */

// fix collision issue where you get stuck in a block at a chunk boundary - probs just have to check adjacent chunks too
// fix rendering issue when some points have -z the square draws rlly big on screen

/* -------------------------- defines -------------------------- */

#define WIDTH  1920
#define HEIGHT 1080

#define FOV 2

#define CUBE_WIDTH 1000

#define TEXTURE_WIDTH 2

#define SQUARES_PER_FACE (TEXTURE_WIDTH * TEXTURE_WIDTH)
#define SQUARES_PER_CUBE (TEXTURE_WIDTH * TEXTURE_WIDTH * 6)

// this should be odd for the best effect - 
// this way the player will be in a central chunk at all times
// as the chunks are layed out in a square
#define SQRT_NUM_CHUNKS 5

#define NUM_CHUNKS (SQRT_NUM_CHUNKS * SQRT_NUM_CHUNKS)
#define CHUNK_WIDTH 16

#define CUBES_PER_CHUNK (CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_WIDTH)
#define MAX_CUBES (CUBE_WIDTH * NUM_CHUNKS)

#define FOG_DIST ((SQRT_NUM_CHUNKS / 2) * CHUNK_WIDTH * CUBE_WIDTH * 0.8)
#define FOG_MAX (CHUNK_WIDTH * CUBE_WIDTH * 0.75)

#define HOTBAR_SLOTS 9
#define HOTBAR_SLOT_WIDTH (WIDTH / (HOTBAR_SLOTS + 2)) // add 2 so it's centred

#define TARGET_FPS 60
#define FRAME_TIME_NS (1000000000 / TARGET_FPS)

// gives x y z of cube relative to chunk its in
#define CUBE_X(cube_i) (cube_i % CHUNK_WIDTH)
#define CUBE_Y(cube_i) ((cube_i / CHUNK_WIDTH) % CHUNK_WIDTH)
#define CUBE_Z(cube_i) ((cube_i / (CHUNK_WIDTH * CHUNK_WIDTH)) % CHUNK_WIDTH)

// gives index of cube in chunk cube array based on its x y z values relative to chunk position
#define CUBE_I(x, y, z) (x + (y * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH))

// gives index of cube next to this cube in the given chunk
#define LEFT_CUBE_I(cube_i) (cube_i - 1)
#define RIGHT_CUBE_I(cube_i) (cube_i + 1)
#define ABOVE_CUBE_I(cube_i) (cube_i + CHUNK_WIDTH)
#define BELOW_CUBE_I(cube_i) (cube_i - CHUNK_WIDTH)
#define IN_FRONT_CUBE_I(cube_i) (cube_i - (CHUNK_WIDTH * CHUNK_WIDTH))
#define BEHIND_CUBE_I(cube_i)  (cube_i + (CHUNK_WIDTH * CHUNK_WIDTH))

// chunk indicies accessed like (z + (x * SQRT_NUM_CHUNKS))
// so to go forward or back you add or minus 1 to chunk index
// if you go past the end of the array, you add or minus SQRT_NUM_CHUNKS

// but to go left or right, you add or minus SQRT_NUM_CHUNKS
// then if you go past the end of the array, you add or minus NUM_CHUNKS
#define LEFT_CHUNK_I(chunk_i) ((chunk_i - SQRT_NUM_CHUNKS < 0) ? \
	                 (chunk_i - SQRT_NUM_CHUNKS + NUM_CHUNKS) : \
					 (chunk_i - SQRT_NUM_CHUNKS))

#define RIGHT_CHUNK_I(chunk_i) ((chunk_i + SQRT_NUM_CHUNKS > (NUM_CHUNKS - 1)) ? \
		              (chunk_i + SQRT_NUM_CHUNKS - NUM_CHUNKS) :                 \
					  (chunk_i + SQRT_NUM_CHUNKS))

#define IN_FRONT_CHUNK_I(chunk_i) ((chunk_i % SQRT_NUM_CHUNKS == 0) ? \
		                 (chunk_i - 1 + SQRT_NUM_CHUNKS) :            \
						 (chunk_i - 1))

#define BEHIND_CHUNK_I(chunk_i) (((chunk_i + 1) % SQRT_NUM_CHUNKS == 0) ? \
	                             (chunk_i + 1 - SQRT_NUM_CHUNKS) :        \
								 (chunk_i + 1))

/* -------------------------- structs and enums -------------------------- */

typedef enum {
	TOP, FRONT, LEFT, BACK, RIGHT, BOTTOM
} direction_t;

typedef enum {
	Z_POS, Z_NEG, X_POS, X_NEG
} chunk_dir_t;

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
	unsigned char r;
	unsigned char g;
	unsigned char b;
} colour_t;

typedef struct {
    colour_t *pixels;
    int width;
	int height;
} texture_t;

typedef struct {
	vec3_t coords[4];	
	uint32_t colour;
	int water;
} square_t;

typedef struct {
	square_t squares[SQUARES_PER_FACE];
	float r;	
} face_t;

typedef struct {
	face_t items[6 * CUBES_PER_CHUNK * NUM_CHUNKS];
	int count;
} faces_t;

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
	cube_t cubes[CUBES_PER_CHUNK];
	vec3_t pos;
} chunk_t;

typedef struct {
	texture_t *texture;
	int cube_i;	
} edit_cube_t;

typedef struct {
	vec3_t pos;
	edit_cube_t *cubes;
	int count;
	int capacity;
} chunk_edit_t;

typedef struct {
	chunk_edit_t *items;
	int count;
	int capacity;
} chunk_edits_t;

typedef struct {
    unsigned char p[512];
} perlin_t;

/* -------------------------- local functions -------------------------- */

static void init_stuff();

static void update();

static void cleanup();

// physics
static int player_inside_cube(vec3_t cube_top_left_front_pos);
static int collided();
static void update_day_cycle();

// input
static void handle_input();
static void handle_mouse();

// drawing
static void update_pixels();

static void clear_screen(colour_t colour);
static void fill_square(square_t *square);
static void draw_rect(vec3_t top_left, int width, int height, colour_t colour);
static void draw_all_faces();

static void draw_cursor();
static void draw_hand();

static uint32_t pack_colour_to_uint32(colour_t *colour);
static colour_t unpack_colour_from_uint32(uint32_t packed_colour);
static colour_t get_pixel_colour(vec2_t coord);

// rendering
static vec3_t rotate_and_project(vec3_t pos);
static vec3_t rotate_and_project_by_rot_value(vec3_t pos, float x_rot, float y_rot);

static int square_surrounds_centre_of_screen(square_t *square);
static int compare_faces(const void *one, const void *two);

static void set_fog_level(colour_t *c, float fog_r);
static void set_light_level(colour_t *c, float fog_r);

static void render_chunks();
static void render_cube_to_faces_array(texture_t *texture, vec3_t cube_top_left_front_pos, face_t *faces_array, int index);

// cubes handling
static void player_place_cube();
static void player_remove_cube();

static int place_cube(int chunk_i, int cube_i, texture_t *texture);
static void remove_cube(int chunk_i, int cube_i);

static void generate_cube(int chunk_i, int cube_i, texture_t *texture);

// chunks
static void build_world();
static void generate_chunk(int chunk_i);
static void update_chunks(int dir);
static void save_chunk_edit(int chunk_i, int cube_i, texture_t *texture);

static void generate_tree(vec3_t pos, int chunk_i);
static void generate_obelisk(vec3_t pos, int chunk_i);
static void generate_frog_shrine(vec3_t pos, int chunk_i);
static void generate_easter_island_statue(vec3_t pos, int chunk_i);

// textures
void generate_textures();

// gpt perlin noise:
static uint32_t lcg(uint32_t *state);
static void perlin_init(perlin_t *n, uint32_t seed);
static double fade(double t);
static double lerp(double a, double b, double t);
static double grad(int h, double x, double y);
static double perlin2D(perlin_t *n, double x, double y);

/* -------------------------- local variables -------------------------- */

struct timespec last, now;

static int paused;
static int frame;

// player
static vec3_t player_pos = {0};

static int player_width;
static int player_height;

static int speed;
static int walk_speed;
static int sprint_speed;

static int gravity;
static int jump_amount;
static int jump_height;
static int can_jump;

// physics
static float day_cycle;
static float max_day_cycle;
static int cycle_frame_interval;
static float night_length;
static int day;
static int night;

// input
static vec2_t mouse = {0};
static float mouse_sensitivity;
static int mouse_left_click;
static int mouse_was_left_clicked;
static int mouse_right_click;
static int mouse_was_right_clicked;
static int holding_mouse;
static int mouse_defined;

static int keys[256] = {0};
static unsigned char w, a, s, d, shift, space, control, escape;
static unsigned char one, two, three, four, five, six, seven, eight, nine;
static int space_was_pressed;
static int last_space_frame;
static int space_to_fly_frame_interval;
static int flying;

static int hotbar_selection;

// drawing
static uint32_t *pixels = NULL;

static colour_t red = {0};
static colour_t green = {0};
static colour_t blue = {0};

static colour_t sky = {0};
static colour_t max_sky = {0};

static face_t hand_grass_faces[6];
static face_t hand_stone_faces[6];
static face_t hand_wood_faces[6];
static face_t hand_leaf_faces[6];
static face_t hand_sand_faces[6];
static face_t hand_water_faces[6];
static face_t hand_black_faces[6];
static face_t hand_white_faces[6];
static face_t hand_beige_faces[6];
static vec3_t hand_pos = {0};

// rendering
static float x_rotation;
static float y_rotation;

static float sine_x_rotation;
static float sine_y_rotation;
static float cos_x_rotation;
static float cos_y_rotation;
static faces_t draw_faces = {0};

static int highlighted_cube_face;
static int highlighted_cube_index;
static int highlighted_cube_chunk_index;

// chunks
static chunk_t chunks[NUM_CHUNKS] = {0};
static int occupied_chunk_index;
static chunk_edits_t chunk_edits = {0};

// textures
texture_t *grass_texture = NULL;
texture_t *dirt_texture = NULL;
texture_t *stone_texture = NULL;
texture_t *wood_texture = NULL;
texture_t *leaf_texture = NULL;
texture_t *sand_texture = NULL;
texture_t *water_texture = NULL;
texture_t *black_texture = NULL;
texture_t *white_texture = NULL;
texture_t *beige_texture = NULL;

// gpt perlin noise
static perlin_t noise;

/* -------------------------- funciton definitions -------------------------- */

void init_stuff() {

	// SEEDS
	srand(time(NULL));
    perlin_init(&noise, 69420);

	// mallocs
	chunk_edits.items = malloc(256 * sizeof(chunk_edit_t));
	chunk_edits.capacity = 256;
	chunk_edits.count = 0;

	// set values
	// - player
	player_pos.x += (SQRT_NUM_CHUNKS * CHUNK_WIDTH * CUBE_WIDTH) / 2;
	player_pos.y = 10 * CUBE_WIDTH;
	player_pos.z += (SQRT_NUM_CHUNKS * CHUNK_WIDTH * CUBE_WIDTH) / 2;

	player_width = CUBE_WIDTH / 3;
	player_height = 4 * player_width;

	speed = CUBE_WIDTH / 9;
	walk_speed = speed;
	sprint_speed = speed * 1.5;

	jump_height = 2.5 * CUBE_WIDTH;
	can_jump = 0;
	gravity = - CUBE_WIDTH / 6;

	// - physics
	day_cycle = 0;
	night = 1;
	cycle_frame_interval = 1;
	max_day_cycle = 480;
	night_length = 480;

	// - input
	holding_mouse = 1;
	mouse_sensitivity = 0.005f;

	space_was_pressed = 0;
	last_space_frame = -100;
	space_to_fly_frame_interval = 20;
	flying = 0;
	hotbar_selection = 0;

	// - drawing
	red.r = 255;
	green.g = 255;
	blue.b = 255;

	max_sky.r = 80;
	max_sky.g = 200;
	max_sky.b = 240;

	sky.r = 0;
	sky.g = 0;
	sky.b = 0;


	// - rendering
	generate_textures();

	draw_faces.count = 0;
	highlighted_cube_face = -1;

	hand_pos.x = -2000;
	hand_pos.y = -600;
	hand_pos.z = -1000;
	render_cube_to_faces_array(grass_texture, hand_pos, hand_grass_faces, 0);
	render_cube_to_faces_array(stone_texture, hand_pos, hand_stone_faces, 0);
	render_cube_to_faces_array(wood_texture, hand_pos, hand_wood_faces, 0);
	render_cube_to_faces_array(leaf_texture, hand_pos, hand_leaf_faces, 0);
	render_cube_to_faces_array(sand_texture, hand_pos, hand_sand_faces, 0);
	render_cube_to_faces_array(water_texture, hand_pos, hand_water_faces, 0);
	render_cube_to_faces_array(black_texture, hand_pos, hand_black_faces, 0);
	render_cube_to_faces_array(white_texture, hand_pos, hand_white_faces, 0);
	render_cube_to_faces_array(beige_texture, hand_pos, hand_beige_faces, 0);

	// - chunks
	occupied_chunk_index = NUM_CHUNKS / 2;

	build_world();

	return;
}

void update() {
	if (paused) {
		return;
	}
	frame++;

	update_day_cycle();

	handle_input();

	update_pixels();
	return;
}

void cleanup() {
	for (int i = 0; i < chunk_edits.count; i++) {
		free(chunk_edits.items[i].cubes);
	}
	free(chunk_edits.items);
	free(grass_texture->pixels);
	free(dirt_texture->pixels);
	free(stone_texture->pixels);
	free(wood_texture->pixels);
	free(leaf_texture->pixels);
	free(sand_texture->pixels);
	free(water_texture->pixels);
	free(black_texture->pixels);
	free(white_texture->pixels);
	free(beige_texture->pixels);
	return;
}

/* ------------------------------- physics ------------------------------- */
void update_day_cycle() {

	if (night) {
		if (frame % cycle_frame_interval == 0 && day_cycle < max_day_cycle) {
			day_cycle += 1;
		}	
		if (day_cycle == max_day_cycle) {
			if (frame % cycle_frame_interval == 0) {
				night += 1;
			}
			if (night == night_length) {
				night = 0;
				day = 1;
			}
		}
	}
	else if (day) {
		if (frame % cycle_frame_interval == 0 && day_cycle > 0) {
			day_cycle -= 1;
		}	
		if (day_cycle == 0) {
			if (frame % cycle_frame_interval == 0) {
				day += 1;
			}
			if (day == night_length) {
				day = 0;
				night = 1;
			}
		}
	}

	sky.r = (char)((float)max_sky.r * (day_cycle / max_day_cycle));
	sky.g = (char)((float)max_sky.g * (day_cycle / max_day_cycle));
	sky.b = (char)((float)max_sky.b * (day_cycle / max_day_cycle));

	return;
}

int player_inside_cube(vec3_t cube_top_left_front_pos) {

	// make the player 2 cubes tall:
	player_pos.y -= CUBE_WIDTH;

	// x1 = top left front
	int x1 = cube_top_left_front_pos.x;
	int y1 = cube_top_left_front_pos.y;
	int z1 = cube_top_left_front_pos.z;

	// x2 = bottom right back
	int x2 = x1 + CUBE_WIDTH;	
	int y2 = y1 - CUBE_WIDTH;	
	int z2 = z1 + CUBE_WIDTH;

	// player_x1 = top left front
	int player_x1 = player_pos.x - player_width;
	int player_y1 = player_pos.y + player_height;
	int player_z1 = player_pos.z - player_width;

	// player_x1 = bottom right back
	int player_x2 = player_pos.x + player_width;
	int player_y2 = player_pos.y - player_width;
	int player_z2 = player_pos.z + player_width;

	int x_collision = 0;
	int y_collision = 0;
	int z_collision = 0;

	int possibly_hit_ground = 0;
	if ((player_x1 >= x1 && player_x1 <= x2) || (player_x2 >= x1 && player_x2 <= x2)) {
		// xs overlap
		x_collision = 1;
	}
	if (player_y1 <= y1 && player_y1 >= y2) {
		// ys overlap
		y_collision = 1;
	}
	if (player_y2 <= y1 && player_y2 >= y2) {
		y_collision = 1;
		possibly_hit_ground = 1;
	}
	if ((player_z1 >= z1 && player_z1 <= z2) || (player_z2 >= z1 && player_z2 <= z2)) {
		// zs overlap
		z_collision = 1;
	}
	if (x_collision && y_collision && z_collision) {
		if (possibly_hit_ground) {
			flying = 0;
			can_jump = 1;
		}

		// make the player 2 cubes tall:
		player_pos.y += CUBE_WIDTH;
		return 1;
	}
	// make the player 2 cubes tall:
	player_pos.y += CUBE_WIDTH;
	return 0;
}

int collided() {

	for (int cube_i = 0; cube_i < CUBES_PER_CHUNK; cube_i++) {

		if (chunks[occupied_chunk_index].cubes[cube_i].texture == NULL ||
			chunks[occupied_chunk_index].cubes[cube_i].texture == water_texture) {
			continue;
		}

		// top left front
		int x = chunks[occupied_chunk_index].pos.x + (CUBE_X(cube_i) * CUBE_WIDTH);
		int y = chunks[occupied_chunk_index].pos.y + (CUBE_Y(cube_i) * CUBE_WIDTH);
		int z = chunks[occupied_chunk_index].pos.z + (CUBE_Z(cube_i) * CUBE_WIDTH);

		if (player_inside_cube((vec3_t){x, y, z})) {
			return 1;
		}
	}
	return 0;
}

/* ------------------------------- input ------------------------------- */
void handle_input() {

	handle_mouse();

	// handle keys/movement
	int x_movement = 0;
	int y_movement = 0;
	int z_movement = 0;

	if (keys[control]) {
        speed = sprint_speed;
	}
	else {
		speed = walk_speed;
	}

    if (keys[w]) {
        x_movement += - speed * sine_x_rotation;
        z_movement += speed * cos_x_rotation;
	}
    if (keys[a]) {
        x_movement += - speed * cos_x_rotation;
        z_movement += - speed * sine_x_rotation;
	}
    if (keys[s]) {
        x_movement += speed * sine_x_rotation;
        z_movement += - speed * cos_x_rotation;
	}
    if (keys[d]) {
        x_movement += speed * cos_x_rotation;
        z_movement += speed * sine_x_rotation;
	}

    if (keys[space]) {
		if (flying) {
			y_movement += speed;
		}
	    else if ((! jump_amount) && can_jump) {
			jump_amount = jump_height;
			can_jump = 0;
		}
		space_was_pressed = 1;
	}
	else {
		if (space_was_pressed) {

			if (frame - last_space_frame < space_to_fly_frame_interval) {
				flying ? (flying = 0) : (flying = 1);	
				jump_amount = 0;
			}


			last_space_frame = frame;
			space_was_pressed = 0;
		}
	}
    if (keys[shift]) {
		if (flying) {
			y_movement += - speed;
		}
	}

	if (keys[escape]) {
		holding_mouse = 0;
	}

	if (! flying) {
		y_movement += gravity;
		y_movement += jump_amount / 5;
		if (jump_amount > 0) {
			jump_amount -= (jump_height * 0.04);
		}
		else {
			jump_amount = 0;
		}
	}

	int old = ((player_pos.x / CUBE_WIDTH) / CHUNK_WIDTH);
	// account for the fact 6 / 10 = 0 and - 6 / 10 = 0
	if (player_pos.x < 0) {
		old--;
	}
	int new = old;

    player_pos.x += x_movement;
	if (collided()) {
		player_pos.x -= x_movement;
	}

	new = ((player_pos.x / CUBE_WIDTH) / CHUNK_WIDTH);
	if (player_pos.x < 0) {
		new--;
	}
	if (old != new) {
		// means we crossed a chunk border
		if (player_pos.x < player_pos.x - x_movement) {
			update_chunks(X_NEG);
		}
		else {
			update_chunks(X_POS);
		}
	}

    player_pos.y += y_movement;
	if (collided()) {
		player_pos.y -= y_movement;
	}

	old = ((player_pos.z / CUBE_WIDTH) / CHUNK_WIDTH);
	if (player_pos.z < 0) {
		old--;
	}
	new = old;

    player_pos.z += z_movement;
	if (collided()) {
		player_pos.z -= z_movement;
	}

	new = ((player_pos.z / CUBE_WIDTH) / CHUNK_WIDTH);
	if (player_pos.z < 0) {
		new--;
	}
	if (old != new) {
		if (player_pos.z < player_pos.z - z_movement) {
			update_chunks(Z_NEG);
		}
		else {
			update_chunks(Z_POS);
		}
	}

	if (keys[one]) {
		hotbar_selection = 0;
	}
	if (keys[two]) {
		hotbar_selection = 1;
	}
	if (keys[three]) {
		hotbar_selection = 2;
	}
	if (keys[four]) {
		hotbar_selection = 3;
	}
	if (keys[five]) {
		hotbar_selection = 4;
	}
	if (keys[six]) {
		hotbar_selection = 5;
	}
	if (keys[seven]) {
		hotbar_selection = 6;
	}
	if (keys[eight]) {
		hotbar_selection = 7;
	}
	if (keys[nine]) {
		hotbar_selection = 8;
	}
	return;
}

void handle_mouse() {

	if (mouse_left_click) {
		holding_mouse = 1;
		mouse_was_left_clicked = 1;
	}
	else {
		if (mouse_was_left_clicked) {
			if (highlighted_cube_face > -1) {
				player_remove_cube();
			}
		}
		mouse_was_left_clicked = 0;
	}
	if (mouse_right_click) {
		mouse_was_right_clicked = 1;
	}
	else {
		if (mouse_was_right_clicked) {
			if (highlighted_cube_face > -1) {
				player_place_cube();
			}
		}
		mouse_was_right_clicked = 0;
	}

	if (holding_mouse) {
		int dx = mouse.x - (WIDTH / 2);
		int dy = mouse.y - (HEIGHT / 2);

		x_rotation -= dx * mouse_sensitivity;
		y_rotation += dy * mouse_sensitivity;

		if (y_rotation > 3.141 / 2) {
			y_rotation = 3.141 / 2;
		}
		if (y_rotation < -3.141 / 2) {
			y_rotation = -3.141 / 2;
		}
	}

	sine_x_rotation = sin(x_rotation);
	sine_y_rotation = sin(y_rotation);

	cos_x_rotation = cos(x_rotation);
	cos_y_rotation = cos(y_rotation);

	return;
}

/* ------------------------------- drawing ------------------------------- */
uint32_t pack_colour_to_uint32(colour_t *colour) {
    return (colour->r << 16) | (colour->g << 8) | colour->b;
}

colour_t unpack_colour_from_uint32(uint32_t packed_colour) {
	colour_t colour = {0};
	colour.r = (packed_colour >> 16) & 255;
	colour.g = (packed_colour >> 8) & 255;
	colour.b = packed_colour & 255;
    return colour;
}

colour_t get_pixel_colour(vec2_t coord) {
	uint32_t packed_colour = pixels[coord.y * WIDTH + coord.x];

	return unpack_colour_from_uint32(packed_colour);
}

void update_pixels() {

    clear_screen(sky);

	render_chunks();

	draw_all_faces();

	draw_cursor();
	draw_hand();

	return;
}

void clear_screen(colour_t colour) {
	uint32_t c = pack_colour_to_uint32(&colour);
	for (int i = 0; i < WIDTH * HEIGHT; i++) {
		pixels[i] = c;
	}
	return;
}

void draw_all_faces() {
	for (int i = 0; i < draw_faces.count; i++) {
		for (int j = 0; j < SQUARES_PER_FACE; j++) {
			fill_square(&draw_faces.items[i].squares[j]);
		}
	}
	return;
}

void fill_square(square_t *square) {

	int smallest_y = HEIGHT + 1;
	int smallest_y_index = 0;
	int largest_y = -1;

	int neg_z_count = 0;

	for (int i = 0; i < 4; i++) {

		if (square->coords[i].z < 0) {
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

	if (smallest_y < 0) {
		smallest_y = 0;
	}
	if (largest_y > HEIGHT) {
		largest_y = HEIGHT;
	}

	int left_start_index = smallest_y_index;
	int left_end_index = smallest_y_index - 1;
	if (left_end_index == -1) {
		left_end_index = 3;
	}

	int right_start_index = smallest_y_index;
	int right_end_index = (smallest_y_index + 1) % 4;

	int left_x, right_x;
	int x1, x2, y1, y2;

	int increment = 1;
	if (square->water) {
		increment = 2;
	}
	for (int y = smallest_y; y < largest_y; y += increment) {
		uint32_t* row = &pixels[y * WIDTH];

		// get x coords y using y = mx + c

 		x1 = square->coords[left_start_index].x;
 		x2 = square->coords[left_end_index].x;

 		y1 = square->coords[left_start_index].y;
 		y2 = square->coords[left_end_index].y;

		// if ((y2 - y1) == 0) the draw the line, and move on to the next left line
		if ((y2 - y1) == 0) {
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

		left_x = x1 + (((y - y1) * (x2 - x1)) / (y2 - y1));

 		x1 = square->coords[right_start_index].x;
 		x2 = square->coords[right_end_index].x;

 		y1 = square->coords[right_start_index].y;
 		y2 = square->coords[right_end_index].y;
			
		if ((y2 - y1) == 0) {
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

		right_x = x1 + (((y - y1) * (x2 - x1)) / (y2 - y1));

		// check if either line is finished and move on to the next line
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

		// fill in between left x and right x
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
	return;
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
	return;
}

void draw_hand() {
	face_t *faces;
	switch (hotbar_selection) {
		case 0: {
			faces = hand_grass_faces;
			break;
		}
		case 1: {
			faces = hand_stone_faces;
			break;
		}
		case 2: {
			faces = hand_wood_faces;
			break;
		}
		case 3: {
			faces = hand_leaf_faces;
			break;
		}
		case 4: {
			faces = hand_sand_faces;
			break;
		}
		case 5: {
			faces = hand_water_faces;
			break;
		}
		case 6: {
			faces = hand_black_faces;
			break;
		}
		case 7: {
			faces = hand_white_faces;
			break;
		}
		case 8: {
			faces = hand_beige_faces;
			break;
		}
	}
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < SQUARES_PER_FACE; j++) {
			fill_square(&faces[i].squares[j]);
		}
	}
	return;
}

/* ------------------------------- rendering ------------------------------- */
void render_chunks() {

	highlighted_cube_face = -1;
	int draw_highlight_index = -1;
	double closest_r = 999999999;

	int len = CUBE_WIDTH / TEXTURE_WIDTH;
	int texture_side = SQUARES_PER_FACE;

	draw_faces.count = 0;

	for (int chunk_i = 0; chunk_i < NUM_CHUNKS; chunk_i++) {

		for (int cube_i = 0; cube_i < CUBES_PER_CHUNK; cube_i++) {

			if (chunks[chunk_i].cubes[cube_i].texture == NULL) {
				continue;
			}

			texture_t *texture = chunks[chunk_i].cubes[cube_i].texture;

			// only draw faces closest to camera
			// top left coord
			int x1 = chunks[chunk_i].pos.x + (CUBE_X(cube_i) * CUBE_WIDTH);
			int y1 = chunks[chunk_i].pos.y + (CUBE_Y(cube_i) * CUBE_WIDTH);
			int z1 = chunks[chunk_i].pos.z + (CUBE_Z(cube_i) * CUBE_WIDTH);

			// bottom right coord
			int x2 = x1 + CUBE_WIDTH;
			int y2 = y1 - CUBE_WIDTH;
			int z2 = z1 + CUBE_WIDTH;

			int draw_top_face = 0;
			int draw_front_face = 0;
			int draw_left_face = 0;
			int draw_back_face = 0;
			int draw_right_face = 0;
			int draw_bottom_face = 0;

			if (abs(player_pos.x - x2) < abs(player_pos.x - x1)) {
				draw_right_face = 1;
			}
			else {
				draw_left_face = 1;
			}
			if (abs(player_pos.z - z2) < abs(player_pos.z - z1)) {
				draw_back_face = 1;
			}
			else {
				draw_front_face = 1;
			}
			if (abs(player_pos.y - y2) > abs(player_pos.y - y1)) {
				draw_top_face = 1;
			}
			else {
				draw_bottom_face = 1;
			}

			for (int face_i = 0; face_i < 6; face_i++) {

				int highlight = 0;

				float centre_x;
				float centre_y;
				float centre_z;

				face_t face = {0};

				switch (face_i) {
					case TOP: {
						if (! draw_top_face) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].top_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - player_pos.x;
						int y = y1 - player_pos.y;
						int z = z1 - player_pos.z;

						centre_x = x + CUBE_WIDTH / 2;
						centre_y = y;
						centre_z = z + CUBE_WIDTH / 2;

						float fog_r = sqrt((centre_x * centre_x) + (centre_z * centre_z));

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x + (i * len), y, z + (j * len)});
								square.coords[1] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y, z + (j * len)});
								square.coords[2] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y, z + ((j + 1) * len)});
								square.coords[3] = rotate_and_project((vec3_t) {x + (i * len), y, z + ((j + 1) * len)});

								// 0 as that is the top texture
								colour_t c = texture->pixels[j * TEXTURE_WIDTH + i];

								set_light_level(&c, fog_r);
								set_fog_level(&c, fog_r);

								square.colour = pack_colour_to_uint32(&c);
								if (texture == water_texture) {
									square.water = 1;
								}

								face.squares[count++] = square;

								if (square_surrounds_centre_of_screen(&square)) {
									highlight = 1;
								}
							}
						}
					    break;
					}
					case BOTTOM: {
						if (! draw_bottom_face) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].bottom_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - player_pos.x;
						int y = y1 - player_pos.y - CUBE_WIDTH;
						int z = z1 - player_pos.z;

						centre_x = x + CUBE_WIDTH / 2;
						centre_y = y;
						centre_z = z + CUBE_WIDTH / 2;

						float fog_r = sqrt((centre_x * centre_x) + (centre_z * centre_z));

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x + (i * len), y, z + (j * len)});
								square.coords[1] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y, z + (j * len)});
								square.coords[2] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y, z + ((j + 1) * len)});
								square.coords[3] = rotate_and_project((vec3_t) {x + (i * len), y, z + ((j + 1) * len)});

								// 1, as the top face textures are the first square of the texture image
								colour_t c = texture->pixels[1 * texture_side + j * TEXTURE_WIDTH + i];

								set_light_level(&c, fog_r);
								set_fog_level(&c, fog_r);

								square.colour = pack_colour_to_uint32(&c);
								if (texture == water_texture) {
									square.water = 1;
								}

								face.squares[count++] = square;

								if (square_surrounds_centre_of_screen(&square)) {
									highlight = 1;
								}
							}
						}
					    break;
					}
					case FRONT: {
						if (! draw_front_face) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].front_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - player_pos.x;
						int y = y1 - player_pos.y;
						int z = z1 - player_pos.z;

						centre_x = x + CUBE_WIDTH / 2;
						centre_y = y - CUBE_WIDTH / 2;
						centre_z = z;

						float fog_r = sqrt((centre_x * centre_x) + (centre_z * centre_z));

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x + (i * len), y - (j * len), z});
								square.coords[1] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y - (j * len), z});
								square.coords[2] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y - ((j + 1) * len), z});
								square.coords[3] = rotate_and_project((vec3_t) {x + (i * len), y - ((j + 1) * len), z});

								// 2, as the side face textures are the 2nd square of the texture image
								colour_t c = texture->pixels[2 * texture_side + j * TEXTURE_WIDTH + i];

								set_light_level(&c, fog_r);
								set_fog_level(&c, fog_r);

								square.colour = pack_colour_to_uint32(&c);
								if (texture == water_texture) {
									square.water = 1;
								}

								face.squares[count++] = square;

								if (square_surrounds_centre_of_screen(&square)) {
									highlight = 1;
								}
							}
						}
					    break;
					}
					case BACK: {
						if (! draw_back_face) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].back_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - player_pos.x;
						int y = y1 - player_pos.y;
						int z = z1 - player_pos.z + CUBE_WIDTH;

						centre_x = x + CUBE_WIDTH / 2;
						centre_y = y - CUBE_WIDTH / 2;
						centre_z = z;

						float fog_r = sqrt((centre_x * centre_x) + (centre_z * centre_z));

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x + (i * len), y - (j * len), z});
								square.coords[1] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y - (j * len), z});
								square.coords[2] = rotate_and_project((vec3_t) {x + ((i + 1) * len), y - ((j + 1) * len), z});
								square.coords[3] = rotate_and_project((vec3_t) {x + (i * len), y - ((j + 1) * len), z});

								colour_t c = texture->pixels[2 * texture_side + j * TEXTURE_WIDTH + i];

								set_light_level(&c, fog_r);
								set_fog_level(&c, fog_r);

								square.colour = pack_colour_to_uint32(&c);
								if (texture == water_texture) {
									square.water = 1;
								}

								face.squares[count++] = square;

								if (square_surrounds_centre_of_screen(&square)) {
									highlight = 1;
								}
							}
						}
					    break;
					}
					case LEFT: {
						if (! draw_left_face) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].left_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - player_pos.x;
						int y = y1 - player_pos.y;
						int z = z1 - player_pos.z;

						centre_x = x;
						centre_y = y - CUBE_WIDTH / 2;
						centre_z = z + CUBE_WIDTH / 2;

						float fog_r = sqrt((centre_x * centre_x) + (centre_z * centre_z));

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x, y - (j * len), z + (i * len)});
								square.coords[1] = rotate_and_project((vec3_t) {x, y - (j * len), z + ((i + 1) * len)});
								square.coords[2] = rotate_and_project((vec3_t) {x, y - ((j + 1) * len), z + ((i + 1) * len)});
								square.coords[3] = rotate_and_project((vec3_t) {x, y - ((j + 1) * len), z + (i * len)});

								colour_t c = texture->pixels[2 * texture_side + j * TEXTURE_WIDTH + i];

								set_light_level(&c, fog_r);
								set_fog_level(&c, fog_r);

								square.colour = pack_colour_to_uint32(&c);
								if (texture == water_texture) {
									square.water = 1;
								}

								face.squares[count++] = square;
								 
								if (square_surrounds_centre_of_screen(&square)) {
									highlight = 1;
								}
							}
						}
					    break;
					}
					case RIGHT: {
						if (! draw_right_face) {
							continue;
						}
						if (chunks[chunk_i].cubes[cube_i].right_neighbour) {
							continue;
						}
						// top left coord
						int x = x1 - player_pos.x + CUBE_WIDTH;
						int y = y1 - player_pos.y;
						int z = z1 - player_pos.z;

						centre_x = x;
						centre_y = y - CUBE_WIDTH / 2;
						centre_z = z + CUBE_WIDTH / 2;

						float fog_r = sqrt((centre_x * centre_x) + (centre_z * centre_z));

						int count = 0;
						for (int i = 0; i < TEXTURE_WIDTH; i++) {
							for (int j = 0; j < TEXTURE_WIDTH; j++) {
								square_t square = {0};

								square.coords[0] = rotate_and_project((vec3_t) {x, y - (j * len), z + (i * len)});
								square.coords[1] = rotate_and_project((vec3_t) {x, y - (j * len), z + ((i + 1) * len)});
								square.coords[2] = rotate_and_project((vec3_t) {x, y - ((j + 1) * len), z + ((i + 1) * len)});
								square.coords[3] = rotate_and_project((vec3_t) {x, y - ((j + 1) * len), z + (i * len)});

								colour_t c = texture->pixels[2 * texture_side + j * TEXTURE_WIDTH + i];

								set_light_level(&c, fog_r);
								set_fog_level(&c, fog_r);

								square.colour = pack_colour_to_uint32(&c);
								if (texture == water_texture) {
									square.water = 1;
								}

								face.squares[count++] = square;

								if (square_surrounds_centre_of_screen(&square)) {
									highlight = 1;
								}
							}
						}
					    break;
					}
				}

				float r = sqrt((centre_x * centre_x) + (centre_y * centre_y) + (centre_z * centre_z));
				face.r = r;

				draw_faces.items[draw_faces.count++] = face;

				if (highlight) {
					// only highlight closest block
					if (face.r < closest_r) {
						closest_r = face.r;

						highlighted_cube_index = cube_i;
						highlighted_cube_chunk_index = chunk_i;
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
	return;
}

void render_cube_to_faces_array(texture_t *texture, vec3_t cube_top_left_front_pos, face_t *faces_array, int index) {

	int len = CUBE_WIDTH / TEXTURE_WIDTH;
	int texture_side = SQUARES_PER_FACE;

	// top left coord
	int x1 = cube_top_left_front_pos.x;
	int y1 = cube_top_left_front_pos.y;
	int z1 = cube_top_left_front_pos.z;

	float x_rot = 3;
	float y_rot = 0;

	for (int face_i = 0; face_i < 6; face_i++) {

		int highlight = 0;

		float centre_x;
		float centre_y;
		float centre_z;

		face_t face = {0};

		switch (face_i) {
			case TOP: {

				// top left coord
				int x = x1;
				int y = y1;
				int z = z1;

				centre_x = x + CUBE_WIDTH / 2;
				centre_y = y;
				centre_z = z + CUBE_WIDTH / 2;

				int count = 0;
				for (int i = 0; i < TEXTURE_WIDTH; i++) {
					for (int j = 0; j < TEXTURE_WIDTH; j++) {
						square_t square = {0};

						square.coords[0] = rotate_and_project_by_rot_value((vec3_t) {x + (i * len), y, z + (j * len)}, x_rot, y_rot);
						square.coords[1] = rotate_and_project_by_rot_value((vec3_t) {x + ((i + 1) * len), y, z + (j * len)}, x_rot, y_rot);
						square.coords[2] = rotate_and_project_by_rot_value((vec3_t) {x + ((i + 1) * len), y, z + ((j + 1) * len)}, x_rot, y_rot);
						square.coords[3] = rotate_and_project_by_rot_value((vec3_t) {x + (i * len), y, z + ((j + 1) * len)}, x_rot, y_rot);

						// 0 as that is the top texture
						colour_t c = texture->pixels[j * TEXTURE_WIDTH + i];

						square.colour = pack_colour_to_uint32(&c);
						if (texture == water_texture) {
							square.water = 1;
						}

						face.squares[count++] = square;
					}
				}
				break;
			}
			case BOTTOM: {
				// top left coord
				int x = x1;
				int y = y1 - CUBE_WIDTH;
				int z = z1;

				centre_x = x + CUBE_WIDTH / 2;
				centre_y = y;
				centre_z = z + CUBE_WIDTH / 2;

				int count = 0;
				for (int i = 0; i < TEXTURE_WIDTH; i++) {
					for (int j = 0; j < TEXTURE_WIDTH; j++) {
						square_t square = {0};

						square.coords[0] = rotate_and_project_by_rot_value((vec3_t) {x + (i * len), y, z + (j * len)}, x_rot, y_rot);
						square.coords[1] = rotate_and_project_by_rot_value((vec3_t) {x + ((i + 1) * len), y, z + (j * len)}, x_rot, y_rot);
						square.coords[2] = rotate_and_project_by_rot_value((vec3_t) {x + ((i + 1) * len), y, z + ((j + 1) * len)}, x_rot, y_rot);
						square.coords[3] = rotate_and_project_by_rot_value((vec3_t) {x + (i * len), y, z + ((j + 1) * len)}, x_rot, y_rot);

						// 1, as the top face textures are the first square of the texture image
						colour_t c = texture->pixels[1 * texture_side + j * TEXTURE_WIDTH + i];

						square.colour = pack_colour_to_uint32(&c);
						if (texture == water_texture) {
							square.water = 1;
						}

						face.squares[count++] = square;
					}
				}
				break;
			}
			case FRONT: {
				// top left coord
				int x = x1;
				int y = y1;
				int z = z1;

				centre_x = x + CUBE_WIDTH / 2;
				centre_y = y - CUBE_WIDTH / 2;
				centre_z = z;

				int count = 0;
				for (int i = 0; i < TEXTURE_WIDTH; i++) {
					for (int j = 0; j < TEXTURE_WIDTH; j++) {
						square_t square = {0};

						square.coords[0] = rotate_and_project_by_rot_value((vec3_t) {x + (i * len), y - (j * len), z}, x_rot, y_rot);
						square.coords[1] = rotate_and_project_by_rot_value((vec3_t) {x + ((i + 1) * len), y - (j * len), z}, x_rot, y_rot);
						square.coords[2] = rotate_and_project_by_rot_value((vec3_t) {x + ((i + 1) * len), y - ((j + 1) * len), z}, x_rot, y_rot);
						square.coords[3] = rotate_and_project_by_rot_value((vec3_t) {x + (i * len), y - ((j + 1) * len), z}, x_rot, y_rot);

						// 2, as the side face textures are the 2nd square of the texture image
						colour_t c = texture->pixels[2 * texture_side + j * TEXTURE_WIDTH + i];

						square.colour = pack_colour_to_uint32(&c);
						if (texture == water_texture) {
							square.water = 1;
						}

						face.squares[count++] = square;
					}
				}
				break;
			}
			case BACK: {
				// top left coord
				int x = x1;
				int y = y1;
				int z = z1 + CUBE_WIDTH;

				centre_x = x + CUBE_WIDTH / 2;
				centre_y = y - CUBE_WIDTH / 2;
				centre_z = z;

				int count = 0;
				for (int i = 0; i < TEXTURE_WIDTH; i++) {
					for (int j = 0; j < TEXTURE_WIDTH; j++) {
						square_t square = {0};

						square.coords[0] = rotate_and_project_by_rot_value((vec3_t) {x + (i * len), y - (j * len), z}, x_rot, y_rot);
						square.coords[1] = rotate_and_project_by_rot_value((vec3_t) {x + ((i + 1) * len), y - (j * len), z}, x_rot, y_rot);
						square.coords[2] = rotate_and_project_by_rot_value((vec3_t) {x + ((i + 1) * len), y - ((j + 1) * len), z}, x_rot, y_rot);
						square.coords[3] = rotate_and_project_by_rot_value((vec3_t) {x + (i * len), y - ((j + 1) * len), z}, x_rot, y_rot);

						colour_t c = texture->pixels[2 * texture_side + j * TEXTURE_WIDTH + i];

						square.colour = pack_colour_to_uint32(&c);
						if (texture == water_texture) {
							square.water = 1;
						}

						face.squares[count++] = square;
					}
				}
				break;
			}
			case LEFT: {
				// top left coord
				int x = x1;
				int y = y1;
				int z = z1;

				centre_x = x;
				centre_y = y - CUBE_WIDTH / 2;
				centre_z = z + CUBE_WIDTH / 2;

				int count = 0;
				for (int i = 0; i < TEXTURE_WIDTH; i++) {
					for (int j = 0; j < TEXTURE_WIDTH; j++) {
						square_t square = {0};

						square.coords[0] = rotate_and_project_by_rot_value((vec3_t) {x, y - (j * len), z + (i * len)}, x_rot, y_rot);
						square.coords[1] = rotate_and_project_by_rot_value((vec3_t) {x, y - (j * len), z + ((i + 1) * len)}, x_rot, y_rot);
						square.coords[2] = rotate_and_project_by_rot_value((vec3_t) {x, y - ((j + 1) * len), z + ((i + 1) * len)}, x_rot, y_rot);
						square.coords[3] = rotate_and_project_by_rot_value((vec3_t) {x, y - ((j + 1) * len), z + (i * len)}, x_rot, y_rot);

						colour_t c = texture->pixels[2 * texture_side + j * TEXTURE_WIDTH + i];

						square.colour = pack_colour_to_uint32(&c);
						if (texture == water_texture) {
							square.water = 1;
						}

						face.squares[count++] = square;
					}
				}
				break;
			}
			case RIGHT: {
				// top left coord
				int x = x1 + CUBE_WIDTH;
				int y = y1;
				int z = z1;

				centre_x = x;
				centre_y = y - CUBE_WIDTH / 2;
				centre_z = z + CUBE_WIDTH / 2;

				int count = 0;
				for (int i = 0; i < TEXTURE_WIDTH; i++) {
					for (int j = 0; j < TEXTURE_WIDTH; j++) {
						square_t square = {0};

						square.coords[0] = rotate_and_project_by_rot_value((vec3_t) {x, y - (j * len), z + (i * len)}, x_rot, y_rot);
						square.coords[1] = rotate_and_project_by_rot_value((vec3_t) {x, y - (j * len), z + ((i + 1) * len)}, x_rot, y_rot);
						square.coords[2] = rotate_and_project_by_rot_value((vec3_t) {x, y - ((j + 1) * len), z + ((i + 1) * len)}, x_rot, y_rot);
						square.coords[3] = rotate_and_project_by_rot_value((vec3_t) {x, y - ((j + 1) * len), z + (i * len)}, x_rot, y_rot);

						colour_t c = texture->pixels[2 * texture_side + j * TEXTURE_WIDTH + i];

						square.colour = pack_colour_to_uint32(&c);
						if (texture == water_texture) {
							square.water = 1;
						}

						face.squares[count++] = square;
					}
				}
				break;
			}
        }

		float r = sqrt((centre_x * centre_x) + (centre_y * centre_y) + (centre_z * centre_z));
		face.r = r;

		faces_array[index++] = face;

	}

	// sort the faces based on their distance to the camera
	qsort(faces_array, index, sizeof(face_t), compare_faces);
	return;
}

int compare_faces(const void *one, const void *two) {
	const face_t *face_one = one;
	const face_t *face_two = two;

	float r1 = face_one->r;
	float r2 = face_two->r;

	if (r1 > r2) {
		return -1;
	}
	if (r1 < r2) {
		return 1;
	}

	return 0;
}

int square_surrounds_centre_of_screen(square_t *square) {

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

void set_fog_level(colour_t *c, float fog_r) {

	if (fog_r > FOG_DIST) {
		float fog_percent = fog_r - FOG_DIST;
		fog_percent = (fog_percent * 100) / FOG_MAX;
		if (fog_percent > 100) {
			fog_percent = 100;
		}
		c->r += (((float)sky.r - (float)c->r) / 100) * fog_percent;
		c->g += (((float)sky.g - (float)c->g) / 100) * fog_percent;
		c->b += (((float)sky.b - (float)c->b) / 100) * fog_percent;
	}

	return;
}

void set_light_level(colour_t *c, float fog_r) {

	float illumination = 1.0f / (fog_r * 0.001);
	illumination += day_cycle / max_day_cycle;
	if (illumination > 1) {
		illumination = 1;
	}

	float r = ((float)c->r) * illumination;
	float g = ((float)c->g) * illumination;
	float b = ((float)c->b) * illumination;

	if (r < 0) {
		r = 0;
	}
	else if (r > c->r) {
		r = c->r;
	}
	if (g < 0) {
		g = 0;
	}
	else if (g > c->g) {
		g = c->g;
	}
	if (b < 0) {
		b = 0;
	}
	else if (b > c->b) {
		b = c->b;
	}

	c->r = (char) r;
	c->g = (char) g;
	c->b = (char) b;

	return;
}

vec3_t rotate_and_project(vec3_t pos) {

	// rotate
	vec3_t new_pos;
	new_pos.x = (pos.z * sine_x_rotation + pos.x * cos_x_rotation);
	int z2 = (pos.z * cos_x_rotation - pos.x * sine_x_rotation);

	new_pos.y = (z2 * sine_y_rotation + pos.y * cos_y_rotation);
	new_pos.z = (z2 * cos_y_rotation - pos.y * sine_y_rotation);

	int neg = 0;
	if (new_pos.z <= 0) {
		neg = 1;
		// avoid divide by 0
		new_pos.z = 7;
	}

	// project
	float percent_size = ((float)((WIDTH / 10) * FOV) / new_pos.z);
	new_pos.x *= percent_size;
	new_pos.y *= percent_size;

	// convert from standard grid to screen grid
	new_pos.x = new_pos.x + WIDTH / 2;
	new_pos.y = -new_pos.y + HEIGHT / 2;

	if (neg) {
		new_pos.z = -1;
	}
	return new_pos;
}

vec3_t rotate_and_project_by_rot_value(vec3_t pos, float x_rot, float y_rot) {

	// rotate
	vec3_t new_pos;
	new_pos.x = (pos.z * sin(x_rot) + pos.x * cos(x_rot));
	int z2 = (pos.z * cos(x_rot) - pos.x * sin(x_rot));

	new_pos.y = (z2 * sin(y_rot) + pos.y * cos(y_rot));
	new_pos.z = (z2 * cos(y_rot) - pos.y * sin(y_rot));

	int neg = 0;
	if (new_pos.z <= 0) {
		neg = 1;
		// avoid divide by 0
		new_pos.z = 7;
	}

	// project
	float percent_size = ((float)((WIDTH / 10) * FOV) / new_pos.z);
	new_pos.x *= percent_size;
	new_pos.y *= percent_size;

	// convert from standard grid to screen grid
	new_pos.x = new_pos.x + WIDTH / 2;
	new_pos.y = -new_pos.y + HEIGHT / 2;

	if (neg) {
		new_pos.z = -1;
	}

	return new_pos;
}

/* ------------------------------- cube handling ------------------------------- */
void generate_cube(int chunk_i, int cube_i, texture_t *texture) {
	if (place_cube(chunk_i, cube_i, texture)) {
		save_chunk_edit(chunk_i, cube_i, texture);
	}
	return;
}

int place_cube(int chunk_i, int cube_i, texture_t *texture) {
	if (cube_i >= CUBES_PER_CHUNK) {
		return -1;
	}

	chunks[chunk_i].cubes[cube_i].texture = texture;

	chunks[chunk_i].cubes[cube_i].top_neighbour = 0;
	chunks[chunk_i].cubes[cube_i].front_neighbour = 0;
	chunks[chunk_i].cubes[cube_i].left_neighbour = 0;
	chunks[chunk_i].cubes[cube_i].back_neighbour = 0;
	chunks[chunk_i].cubes[cube_i].right_neighbour = 0;
	chunks[chunk_i].cubes[cube_i].bottom_neighbour = 0;

	// check if we are at a chunk edge
	int x = CUBE_X(cube_i);
	int y = CUBE_Y(cube_i);
	int z = CUBE_Z(cube_i);

	int block_above = 1;
	int block_in_front = 1;
	int block_left = 1;
	int block_behind = 1;
	int block_right = 1;
	int block_below = 1;

	if (x == 0) {
		chunks[chunk_i].cubes[cube_i].left_neighbour = 0;
		block_left = 0;
	}
	else if (x == CHUNK_WIDTH - 1) {
		chunks[chunk_i].cubes[cube_i].right_neighbour = 0;
		block_right = 0;
	}
	if (y == 0) {
		chunks[chunk_i].cubes[cube_i].bottom_neighbour = 1;
		block_below = 0;
	}
	else if (y == CHUNK_WIDTH - 1) {
		chunks[chunk_i].cubes[cube_i].top_neighbour = 0;
		block_above = 0;
	}
	if (z == 0) {
		chunks[chunk_i].cubes[cube_i].front_neighbour = 0;
		block_in_front = 0;
	}
	else if (z == CHUNK_WIDTH - 1) {
		chunks[chunk_i].cubes[cube_i].back_neighbour = 0;
		block_behind = 0;
	}

	// wow for transparency
	if (chunks[chunk_i].cubes[cube_i].texture == water_texture) {

		// set water neighbour faces
		if (block_above) {
			// cube_i + - 1 goes right or left one cube
			// cube_i + - CHUNK_WIDTH goes up or down one cube
			// cube_i + - CHUNK_WIDTH * CHUNK_WIDTH goes back or forward one cube
			if (chunks[chunk_i].cubes[ABOVE_CUBE_I(cube_i)].texture == water_texture) {
				chunks[chunk_i].cubes[ABOVE_CUBE_I(cube_i)].bottom_neighbour = 1;
				chunks[chunk_i].cubes[cube_i].top_neighbour = 1;
			}
			else if (chunks[chunk_i].cubes[ABOVE_CUBE_I(cube_i)].texture != NULL) {
				chunks[chunk_i].cubes[cube_i].top_neighbour = 1;
			}
		}
		if (block_in_front) {
			if (chunks[chunk_i].cubes[IN_FRONT_CUBE_I(cube_i)].texture == water_texture) {
				chunks[chunk_i].cubes[IN_FRONT_CUBE_I(cube_i)].back_neighbour = 1;
				chunks[chunk_i].cubes[cube_i].front_neighbour = 1;
			}
			else if (chunks[chunk_i].cubes[IN_FRONT_CUBE_I(cube_i)].texture != NULL) {
				chunks[chunk_i].cubes[cube_i].front_neighbour = 1;
			}
		}
		if (block_left) {
			if (chunks[chunk_i].cubes[LEFT_CUBE_I(cube_i)].texture == water_texture) {
				chunks[chunk_i].cubes[LEFT_CUBE_I(cube_i)].right_neighbour = 1;
				chunks[chunk_i].cubes[cube_i].left_neighbour = 1;
			}
			else if (chunks[chunk_i].cubes[LEFT_CUBE_I(cube_i)].texture != NULL) {
				chunks[chunk_i].cubes[cube_i].left_neighbour = 1;
			}
		}
		if (block_behind) {
			if (chunks[chunk_i].cubes[BEHIND_CUBE_I(cube_i)].texture == water_texture) {
				chunks[chunk_i].cubes[BEHIND_CUBE_I(cube_i)].front_neighbour = 1;
				chunks[chunk_i].cubes[cube_i].back_neighbour = 1;
			}
			else if (chunks[chunk_i].cubes[BEHIND_CUBE_I(cube_i)].texture != NULL) {
				chunks[chunk_i].cubes[cube_i].back_neighbour = 1;
			}
		}
		if (block_right) {
			if (chunks[chunk_i].cubes[RIGHT_CUBE_I(cube_i)].texture == water_texture) {
				chunks[chunk_i].cubes[RIGHT_CUBE_I(cube_i)].left_neighbour = 1;
				chunks[chunk_i].cubes[cube_i].right_neighbour = 1;
			}
			else if (chunks[chunk_i].cubes[RIGHT_CUBE_I(cube_i)].texture != NULL) {
				chunks[chunk_i].cubes[cube_i].right_neighbour = 1;
			}
		}
		if (block_below) {
			if (chunks[chunk_i].cubes[BELOW_CUBE_I(cube_i)].texture == water_texture) {
				chunks[chunk_i].cubes[BELOW_CUBE_I(cube_i)].top_neighbour = 1;
				chunks[chunk_i].cubes[cube_i].bottom_neighbour = 1;
			}
			else if (chunks[chunk_i].cubes[BELOW_CUBE_I(cube_i)].texture != NULL) {
				chunks[chunk_i].cubes[cube_i].bottom_neighbour = 1;
			}
		}

		return 1;
	}

	// set neighbour faces
	if (block_above) {
		// cube_i + - 1 goes right or left one cube
		// cube_i + - CHUNK_WIDTH goes up or down one cube
		// cube_i + - CHUNK_WIDTH * CHUNK_WIDTH goes back or forward one cube
		if (chunks[chunk_i].cubes[ABOVE_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[ABOVE_CUBE_I(cube_i)].bottom_neighbour = 1;
			chunks[chunk_i].cubes[cube_i].top_neighbour = 1;
		}
	}
	if (block_in_front) {
		if (chunks[chunk_i].cubes[IN_FRONT_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[IN_FRONT_CUBE_I(cube_i)].back_neighbour = 1;
			chunks[chunk_i].cubes[cube_i].front_neighbour = 1;
		}
	}
	if (block_left) {
		if (chunks[chunk_i].cubes[LEFT_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[LEFT_CUBE_I(cube_i)].right_neighbour = 1;
			chunks[chunk_i].cubes[cube_i].left_neighbour = 1;
		}
	}
	if (block_behind) {
		if (chunks[chunk_i].cubes[BEHIND_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[BEHIND_CUBE_I(cube_i)].front_neighbour = 1;
			chunks[chunk_i].cubes[cube_i].back_neighbour = 1;
		}
	}
	if (block_right) {
		if (chunks[chunk_i].cubes[RIGHT_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[RIGHT_CUBE_I(cube_i)].left_neighbour = 1;
			chunks[chunk_i].cubes[cube_i].right_neighbour = 1;
		}
	}
	if (block_below) {
		if (chunks[chunk_i].cubes[BELOW_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[BELOW_CUBE_I(cube_i)].top_neighbour = 1;
			chunks[chunk_i].cubes[cube_i].bottom_neighbour = 1;
		}
	}

	return 1;
}

void remove_cube(int chunk_i, int cube_i) {

	chunks[chunk_i].cubes[cube_i].texture = NULL;

	// check if we are at a chunk edge
	int x = CUBE_X(cube_i);
	int y = CUBE_Y(cube_i);
	int z = CUBE_Z(cube_i);

	int block_above = 1;
	int block_in_front = 1;
	int block_left = 1;
	int block_behind = 1;
	int block_right = 1;
	int block_below = 1;

	if (x == 0) {
		chunks[chunk_i].cubes[cube_i].left_neighbour = 0;
		block_left = 0;
	}
	else if (x == CHUNK_WIDTH - 1) {
		chunks[chunk_i].cubes[cube_i].right_neighbour = 0;
		block_right = 0;
	}
	if (y == 0) {
		chunks[chunk_i].cubes[cube_i].bottom_neighbour = 1;
		block_below = 0;
	}
	else if (y == CHUNK_WIDTH - 1) {
		chunks[chunk_i].cubes[cube_i].top_neighbour = 0;
		block_above = 0;
	}
	if (z == 0) {
		chunks[chunk_i].cubes[cube_i].front_neighbour = 0;
		block_in_front = 0;
	}
	else if (z == CHUNK_WIDTH - 1) {
		chunks[chunk_i].cubes[cube_i].back_neighbour = 0;
		block_behind = 0;
	}

	// update neighbours
	if (block_above) {
		if (chunks[chunk_i].cubes[ABOVE_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[ABOVE_CUBE_I(cube_i)].bottom_neighbour = 0;
		}
	}
	if (block_in_front) {
		if (chunks[chunk_i].cubes[IN_FRONT_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[IN_FRONT_CUBE_I(cube_i)].back_neighbour = 0;
		}
	}
	else {
		int z2 = CHUNK_WIDTH - 1;
		chunks[IN_FRONT_CHUNK_I(chunk_i)].cubes[CUBE_I(x, y, z2)].back_neighbour = 0;
	}
	if (block_left) {
		if (chunks[chunk_i].cubes[LEFT_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[LEFT_CUBE_I(cube_i)].right_neighbour = 0;
		}
	}
	else {
		int x2 = CHUNK_WIDTH - 1;
		chunks[LEFT_CHUNK_I(chunk_i)].cubes[CUBE_I(x2, y, z)].right_neighbour = 0;
	}
	if (block_behind) {
		if (chunks[chunk_i].cubes[BEHIND_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[BEHIND_CUBE_I(cube_i)].front_neighbour = 0;
		}
	}
	else {
		int z2 = 0;
		chunks[BEHIND_CHUNK_I(chunk_i)].cubes[CUBE_I(x, y, z2)].front_neighbour = 0;
	}
	if (block_right) {
		if (chunks[chunk_i].cubes[RIGHT_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[RIGHT_CUBE_I(cube_i)].left_neighbour = 0;
		}
	}
	else {
		int x2 = 0;
		chunks[RIGHT_CHUNK_I(chunk_i)].cubes[CUBE_I(x2, y, z)].left_neighbour = 0;
	}
	if (block_below) {
		if (chunks[chunk_i].cubes[BELOW_CUBE_I(cube_i)].texture != NULL) {
			chunks[chunk_i].cubes[BELOW_CUBE_I(cube_i)].top_neighbour = 0;
		}
	}

	return;
}

void player_place_cube() {

	// check if cube highlighted is at a chunk edge
	int x = CUBE_X(highlighted_cube_index);
	int y = CUBE_Y(highlighted_cube_index);
	int z = CUBE_Z(highlighted_cube_index);

	// if it is, need to make sure we have the right chunk and cube index
	// as if the cube we're looking at is in the chunk to the right,
	// and we're looking at the left side of the cube,
	// highlighted_chunk_i will need to be the index of the chunk to the left of that,
	// and highlighted_cube_i will need to be the right most cube in that chunk etc
	if (x == 0) {
		if (highlighted_cube_face == LEFT) {
			highlighted_cube_chunk_index = LEFT_CHUNK_I(highlighted_cube_chunk_index);
			highlighted_cube_index += CHUNK_WIDTH;
		}
	}
	else if (x == CHUNK_WIDTH - 1) {
		if (highlighted_cube_face == RIGHT) {
			highlighted_cube_chunk_index = RIGHT_CHUNK_I(highlighted_cube_chunk_index);
			highlighted_cube_index -= CHUNK_WIDTH;
		}
	}
	if (z == 0) {
		if (highlighted_cube_face == FRONT) {
			highlighted_cube_chunk_index = IN_FRONT_CHUNK_I(highlighted_cube_chunk_index);
			highlighted_cube_index += (CHUNK_WIDTH) * CHUNK_WIDTH * CHUNK_WIDTH;
		}
	}
	else if (z == CHUNK_WIDTH - 1) {
		if (highlighted_cube_face == BACK) {
			highlighted_cube_chunk_index = BEHIND_CHUNK_I(highlighted_cube_chunk_index);
			highlighted_cube_index -= (CHUNK_WIDTH) * CHUNK_WIDTH * CHUNK_WIDTH;
		}
	}

	vec3_t pos = {0};

	pos.x = chunks[highlighted_cube_chunk_index].pos.x + (CUBE_X(highlighted_cube_index) * CUBE_WIDTH);
	pos.y = chunks[highlighted_cube_chunk_index].pos.y + (CUBE_Y(highlighted_cube_index) * CUBE_WIDTH);
	pos.z = chunks[highlighted_cube_chunk_index].pos.z + (CUBE_Z(highlighted_cube_index) * CUBE_WIDTH);

	int index_diff = 0;
	switch (highlighted_cube_face) {
		case TOP: {
			pos.y += CUBE_WIDTH;
			index_diff = CHUNK_WIDTH;
			break;
		}
		case FRONT: {
			pos.z -= CUBE_WIDTH;
			index_diff = - CHUNK_WIDTH * CHUNK_WIDTH;
			break;
		}
		case LEFT: {
			pos.x -= CUBE_WIDTH;
			index_diff = -1;
			break;
		}
		case BACK: {
			pos.z += CUBE_WIDTH;
			index_diff = CHUNK_WIDTH * CHUNK_WIDTH;
			break;
		}
		case RIGHT: {
			pos.x += CUBE_WIDTH;
			index_diff = 1;
			break;
		}
		case BOTTOM: {
			pos.y -= CUBE_WIDTH;
			index_diff = -CHUNK_WIDTH;
			break;
		}
	}
	int x1 = pos.x;
	int y1 = pos.y;
	int z1 = pos.z;

	if (player_inside_cube((vec3_t){x1, y1, z1})) {
		return;
	}

	texture_t *texture = grass_texture;

	switch (hotbar_selection) {
		case 0: {
			// grass
			break;
		}
		case 1: {
			texture = stone_texture;
			break;
		}
		case 2: {
			texture = wood_texture;
			break;
		}
		case 3: {
			texture = leaf_texture;
			break;
		}
		case 4: {
			texture = sand_texture;
			break;
		}
		case 5: {
			texture = water_texture;
			break;
		}
		case 6: {
			texture = black_texture;
			break;
		}
		case 7: {
			texture = white_texture;
			break;
		}
		case 8: {
			texture = beige_texture;
			break;
		}
	}

	int cube_i = highlighted_cube_index + index_diff;
	place_cube(highlighted_cube_chunk_index, cube_i, texture);
	save_chunk_edit(highlighted_cube_chunk_index, cube_i, texture);

	return;	
}

void player_remove_cube() {
	save_chunk_edit(highlighted_cube_chunk_index, highlighted_cube_index, NULL);

	remove_cube(highlighted_cube_chunk_index, highlighted_cube_index);
	return;
}

/* ------------------------------- chunks ------------------------------- */
void build_world() {

	int count = 0;
	for (int x = 0; x < SQRT_NUM_CHUNKS; x++) {
		for (int z = 0; z < SQRT_NUM_CHUNKS; z++) {
			chunks[count].pos = (vec3_t){x * CUBE_WIDTH * CHUNK_WIDTH, 0, z * CUBE_WIDTH * CHUNK_WIDTH};
			generate_chunk(count);
			count++;
		}
	}
	return;
}

void generate_chunk(int chunk_i) {

	chunk_t *chunk = &chunks[chunk_i];

	// if no edit exists for this chunk, we will generate trees later
	int edit_index = -1;
	for (int i = 0; i < chunk_edits.count; i++) {
		if (chunk_edits.items[i].pos.x == chunk->pos.x && chunk_edits.items[i].pos.z == chunk->pos.z) {
			edit_index = i;
		}
	}

	texture_t *texture = grass_texture;

	for (int x = 0; x < CHUNK_WIDTH; x++) {
		for (int y = 0; y < CHUNK_WIDTH; y++) {
			for (int z = 0; z < CHUNK_WIDTH; z++) {

				// set this value for a certain smoothness of terrain from the perlin noise
				float val = 0.00003;
				int perlin_y = floor(((perlin2D(&noise,
									            (chunk->pos.x + (x * CUBE_WIDTH)) * val,
												(chunk->pos.z + (z * CUBE_WIDTH)) * val)) * 10));

				// check neighbours to cull neighbour faces
				int right = floor(((perlin2D(&noise,
									         (chunk->pos.x + (x * CUBE_WIDTH) + CUBE_WIDTH) * val,
											 (chunk->pos.z + (z * CUBE_WIDTH)) * val)) * 10));
				int left = floor(((perlin2D(&noise,
									        (chunk->pos.x + (x * CUBE_WIDTH) - CUBE_WIDTH) * val,
											(chunk->pos.z + (z * CUBE_WIDTH)) * val)) * 10));
				int front = floor(((perlin2D(&noise,
									         (chunk->pos.x + (x * CUBE_WIDTH)) * val,
											 (chunk->pos.z + (z * CUBE_WIDTH) - CUBE_WIDTH) * val)) * 10));
				int back = floor(((perlin2D(&noise,
									        (chunk->pos.x + (x * CUBE_WIDTH)) * val,
											(chunk->pos.z + (z * CUBE_WIDTH) + CUBE_WIDTH) * val)) * 10));

				perlin_y += 5;
				right += 5;
				left += 5;
				front += 5;
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

				int cube_above = 0;
				int cube_left = 0;
				int cube_right = 0;
				int cube_in_front = 0;
				int cube_behind = 0;
				int cube_below= 1;

				if (front >= y) {
					cube_in_front = 1;
				}
				if (back >= y) {
					cube_behind = 1;
				}
				if (right >= y) {
					cube_right = 1;
				}
				if (left >= y) {
					cube_left = 1;
				}
				cube_above = 0;
				
				if (y > perlin_y) {
					// air
					texture = NULL;
				}
				else if (y == perlin_y) {
					// grass
					texture = grass_texture;
				}
				else if (y == 0) {
					// stone
					texture = stone_texture;
					cube_above = 1;
				}
				else {
					// dirt
					texture = dirt_texture;
					cube_above = 1;
				}
				chunk->cubes[CUBE_I(x, y, z)].texture = texture;
				chunk->cubes[CUBE_I(x, y, z)].left_neighbour = cube_left;
				chunk->cubes[CUBE_I(x, y, z)].right_neighbour = cube_right;
				chunk->cubes[CUBE_I(x, y, z)].front_neighbour = cube_in_front;
				chunk->cubes[CUBE_I(x, y, z)].back_neighbour = cube_behind;
				chunk->cubes[CUBE_I(x, y, z)].top_neighbour = cube_above;
				chunk->cubes[CUBE_I(x, y, z)].bottom_neighbour = cube_below;
			}
		}
	}

	if (edit_index > -1) {
		// apply the edits
		for (int j = 0; j < chunk_edits.items[edit_index].count; j++) {
			if (chunk_edits.items[edit_index].cubes[j].texture == NULL) {
				remove_cube(chunk_i, chunk_edits.items[edit_index].cubes[j].cube_i);
			}
			else {
				place_cube(chunk_i,
						   chunk_edits.items[edit_index].cubes[j].cube_i,
						   chunk_edits.items[edit_index].cubes[j].texture);
			}
		}
	}
	else {

		// generate some trees:
		for (int x = 0; x < CHUNK_WIDTH; x++) {
			for (int z = 0; z < CHUNK_WIDTH; z++) {
				for (int y = 0; y < CHUNK_WIDTH; y++) {

					if (chunk->cubes[CUBE_I(x, y, z)].texture != NULL) {
						continue;
					}

					// plant a tree at the first air cube
					if (rand() % 100 == 0) {
						generate_tree((vec3_t) {x, y, z}, chunk_i);
						continue;
					}
					else if (rand() % 6000 == 0) {
						generate_frog_shrine((vec3_t) {x, y, z}, chunk_i);
					}
					else if (rand() % 9000 == 0) {
						generate_obelisk((vec3_t) {x, y, z}, chunk_i);
					}
					else if (rand() % 3000 == 0) {
						generate_easter_island_statue((vec3_t) {x, y, z}, chunk_i);
					}

					break;

				}
			}
		}
	}
	return;
}

void update_chunks(int dir) {

	switch (dir) {
		case Z_POS: {
			// move player index forward
			occupied_chunk_index += 1;
			if (occupied_chunk_index % SQRT_NUM_CHUNKS == 0) {
				occupied_chunk_index -= SQRT_NUM_CHUNKS;
			}
			// take back chunks and move them to the front
			for (int i = 0; i < NUM_CHUNKS; i++) {
				if (player_pos.z - chunks[i].pos.z > ((SQRT_NUM_CHUNKS / 2) + 1) * CHUNK_WIDTH * CUBE_WIDTH) {
					chunks[i].pos.z += SQRT_NUM_CHUNKS * CUBE_WIDTH * CHUNK_WIDTH;
					generate_chunk(i);
				}
			}
			break;
		}
		case Z_NEG: {
			// etc
			if (occupied_chunk_index % SQRT_NUM_CHUNKS == 0) {
				occupied_chunk_index += SQRT_NUM_CHUNKS - 1;
			}
			else {
				occupied_chunk_index -= 1;
			}
			for (int i = 0; i < NUM_CHUNKS; i++) {
				if (chunks[i].pos.z - player_pos.z > (SQRT_NUM_CHUNKS / 2) * CHUNK_WIDTH * CUBE_WIDTH) {
					chunks[i].pos.z -= SQRT_NUM_CHUNKS * CUBE_WIDTH * CHUNK_WIDTH;
					generate_chunk(i);
				}
			}
			break;
		}
		case X_POS: {
			occupied_chunk_index += SQRT_NUM_CHUNKS;
			if (occupied_chunk_index > (NUM_CHUNKS - 1)) {
				occupied_chunk_index -= NUM_CHUNKS;
			}
			for (int i = 0; i < NUM_CHUNKS; i++) {
				if (player_pos.x - chunks[i].pos.x > ((SQRT_NUM_CHUNKS / 2) + 1) * CHUNK_WIDTH * CUBE_WIDTH) {
					chunks[i].pos.x += SQRT_NUM_CHUNKS * CUBE_WIDTH * CHUNK_WIDTH;
					generate_chunk(i);
				}
			}
			break;
		}
		case X_NEG: {
			occupied_chunk_index -= SQRT_NUM_CHUNKS;
			if (occupied_chunk_index < 0) {
				occupied_chunk_index += NUM_CHUNKS;
			}
			for (int i = 0; i < NUM_CHUNKS; i++) {
				if (chunks[i].pos.x - player_pos.x > (SQRT_NUM_CHUNKS / 2) * CHUNK_WIDTH * CUBE_WIDTH) {
					chunks[i].pos.x -= SQRT_NUM_CHUNKS * CUBE_WIDTH * CHUNK_WIDTH;
					generate_chunk(i);
				}
			}
			break;
		}
	}
	return;
}

void save_chunk_edit(int chunk_i, int cube_i, texture_t *texture) {

	vec3_t chunk_pos = chunks[chunk_i].pos;

	int edit_index = -1;
	for (int i = 0; i < chunk_edits.count; i++) {
		if (chunk_edits.items[i].pos.x == chunk_pos.x && chunk_edits.items[i].pos.z == chunk_pos.z) {
			edit_index = i;
			break;	
		}
	}

	if (edit_index > -1) {
		if (chunk_edits.items[edit_index].count == chunk_edits.items[edit_index].capacity) {
			chunk_edits.items[edit_index].capacity *= 2;
			chunk_edits.items[edit_index].cubes = realloc(chunk_edits.items[edit_index].cubes,
					                                chunk_edits.items[edit_index].capacity * sizeof(edit_cube_t));
		}
		int count = chunk_edits.items[edit_index].count;

		chunk_edits.items[edit_index].cubes[count].texture = texture;
		chunk_edits.items[edit_index].cubes[count].cube_i = cube_i;
		chunk_edits.items[edit_index].count++;
	}
	else {
		chunk_edit_t new_edit;
		new_edit.pos = chunk_pos;

		new_edit.cubes = malloc(256 * sizeof(edit_cube_t));
		new_edit.capacity = 256;
		new_edit.cubes[0].texture = texture;
		new_edit.cubes[0].cube_i = cube_i;
		new_edit.count = 1;

		if (chunk_edits.count == chunk_edits.capacity) {
			chunk_edits.capacity *= 2;
			chunk_edits.items = realloc(chunk_edits.items, chunk_edits.capacity * sizeof(chunk_edit_t)); 
		}
		chunk_edits.items[chunk_edits.count++] = new_edit;
	}
}

void generate_tree(vec3_t pos, int chunk_i) {
	// check boundaries
	int x = pos.x;
	int y = pos.y;
	int z = pos.z;

	if (x < 2 || x > CHUNK_WIDTH - 2) {
		return;
	}
	if (z < 2 || z > CHUNK_WIDTH - 2) {
		return;
	}
	int max_h = 3 + rand() % 10;
	if (y > CHUNK_WIDTH - max_h - 1) {
		return;
	}
	int h = 0;
	for (h = 0; h < max_h; h++) {
		if (h + 1 > CHUNK_WIDTH) {
			break;
		}
		int i = CUBE_I(x, (y + h) , z);
		generate_cube(chunk_i, i, wood_texture);
	}

	h--;
	int i = CUBE_I((x + 1), (y + h) , z);
	generate_cube(chunk_i, i, leaf_texture);

	i = CUBE_I((x - 1), (y + h), z);
	generate_cube(chunk_i, i, leaf_texture);

	i = CUBE_I(x, (y + h), (z + 1));
	generate_cube(chunk_i, i, leaf_texture);

	i = CUBE_I(x, (y + h), (z - 1));
	generate_cube(chunk_i, i, leaf_texture);

	i = CUBE_I(x, (y + h + 1), z);
	generate_cube(chunk_i, i, leaf_texture);
	return;
}

void generate_obelisk(vec3_t pos, int chunk_i) {
	// obelisk
	// check boundaries
	int x = pos.x;
	int y = pos.y;
	int z = pos.z;
	if (x < 4 || x > CHUNK_WIDTH - 4) {
		return;
	}
	if (z < 4 || z > CHUNK_WIDTH - 4) {
		return;
	}
	if (y > CHUNK_WIDTH - 6) {
		return;
	}

	for (int j = 0; j < 6; j++) {
		for (int k = 0; k < 3; k++) {
			int i = CUBE_I((x + k), (y + j) , z);
			generate_cube(chunk_i, i, stone_texture);
			i = CUBE_I((x + k), (y + j) , (z + 1));
			generate_cube(chunk_i, i, stone_texture);
		}
	}
	return;
}

void generate_frog_shrine(vec3_t pos, int chunk_i) {
	// frog statue
	// check boundaries
	int x = pos.x;
	int y = pos.y;
	int z = pos.z;

	if (x < 4 || x > CHUNK_WIDTH - 4) {
		return;
	}
	if (z < 4 || z > CHUNK_WIDTH - 4) {
		return;
	}
	if (y > CHUNK_WIDTH - 4) {
		return;
	}

	for (int j = 0; j < 5; j++) {
		int i = CUBE_I((x + j), y , z);
		generate_cube(chunk_i, i, stone_texture);

		i = CUBE_I((x + j), y , (z + 1));
		generate_cube(chunk_i, i, stone_texture);

		i = CUBE_I((x + j), (y + 1) , z);
		generate_cube(chunk_i, i, stone_texture);

		i = CUBE_I((x + j), (y + 1) , (z + 1));
		generate_cube(chunk_i, i, stone_texture);

		i = CUBE_I((x + j), y , (z - 1));
		generate_cube(chunk_i, i, stone_texture);
	}

	int i = (x + 1 + ((y + 2) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, leaf_texture);
	i = (x + 1 + ((y + 3) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x + 1 + ((y + 2) * CHUNK_WIDTH) + ((z + 1) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);

	i = (x + 2 + ((y + 2) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x + 2 + ((y + 1) * CHUNK_WIDTH) + ((z - 1) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);

	i = (x + 3 + ((y + 2) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, leaf_texture);
	i = (x + 3 + ((y + 3) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x + 3 + ((y + 2) * CHUNK_WIDTH) + ((z + 1) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);

	i = (x + 1 + (y * CHUNK_WIDTH) + ((z - 2) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x + 3 + (y * CHUNK_WIDTH) + ((z - 2) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);

	i = (x + 1 + (y * CHUNK_WIDTH) + ((z + 2) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x + 2 + (y * CHUNK_WIDTH) + ((z + 2) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x + 3 + (y * CHUNK_WIDTH) + ((z + 2) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	return;
}

void generate_easter_island_statue(vec3_t pos, int chunk_i) {
	// check boundaries
	int x = pos.x;
	int y = pos.y;
	int z = pos.z;
	if (x < 4 || x > CHUNK_WIDTH - 4) {
		return;
	}
	if (z < 4 || z > CHUNK_WIDTH - 4) {
		return;
	}
	if (y > CHUNK_WIDTH - 6) {
		return;
	}

	for (int j = 2; j < 6; j++) {
		for (int k = 0; k < 3; k++) {
			int i = (x + k + ((y + j) * CHUNK_WIDTH) + ((z + 1) * CHUNK_WIDTH * CHUNK_WIDTH));
			generate_cube(chunk_i, i, stone_texture);
		}
	}
	for (int k = 0; k < 3; k++) {
		int i = (x + k + ((y + 5) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
		generate_cube(chunk_i, i, stone_texture);
	}
	int i = (x + ((y + 4) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, leaf_texture);
	i = (x + 2 + ((y + 4) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, leaf_texture);

	for (int j = 2; j < 6; j++) {
		int i = (x + 1 + ((y + j) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
		generate_cube(chunk_i, i, stone_texture);
	}
	i = (x + 0 + ((y + 2) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x + 1 + ((y + 2) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x + 2 + ((y + 2) * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);

	i = (x + 1 + ((y + 2) * CHUNK_WIDTH) + ((z - 1) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);

	i = (x + 1 + ((y + 2) * CHUNK_WIDTH) + ((z - 1) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x - 1 + ((y + 2) * CHUNK_WIDTH) + ((z + 1) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x + 3 + ((y + 2) * CHUNK_WIDTH) + ((z + 1) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);

	i = (x + (y * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x + (y * CHUNK_WIDTH) + ((z + 1) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);

	i = (x + 2 + (y * CHUNK_WIDTH) + (z * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);
	i = (x + 2 + (y * CHUNK_WIDTH) + ((z + 1) * CHUNK_WIDTH * CHUNK_WIDTH));
	generate_cube(chunk_i, i, stone_texture);

	for (int j = 0; j < 2; j++) {
		for (int k = 0; k < 3; k++) {
			int i = (x + k + ((y + 1) * CHUNK_WIDTH) + ((z + j) * CHUNK_WIDTH * CHUNK_WIDTH));
			generate_cube(chunk_i, i, stone_texture);
		}
	}
}

/* ------------------------------- textures ------------------------------- */
void generate_textures() {
	// create grass texture
	// * 3 for top bottom side of cube
    int width = TEXTURE_WIDTH * 3;
    int height = TEXTURE_WIDTH;
	grass_texture = malloc(sizeof(texture_t));
    grass_texture->pixels = malloc(width * height * 3 * sizeof(colour_t));
	grass_texture->width = width;
	grass_texture->height = height;

	// top
	int i = 0;
	for (i; i < SQUARES_PER_FACE; i++) {
		grass_texture->pixels[i] = (colour_t){10, 180 + (rand() % 70), 20 + (rand() % 60)};
	}
	// bottom
	for (i; i < 2 * SQUARES_PER_FACE; i++) {
		grass_texture->pixels[i] = (colour_t){150 + (rand() % 70), 75 + (rand() % 60), 10 + (rand() % 60)};
	}
	// side
	for (i; i < 3 * SQUARES_PER_FACE; i++) {
		if (i < 2.5 * SQUARES_PER_FACE) {
			grass_texture->pixels[i] = (colour_t){10, 180 + (rand() % 70), 20 + (rand() % 60)};
		}
		else {
			grass_texture->pixels[i] = (colour_t){150 + (rand() % 70), 75 + (rand() % 60), 10 + (rand() % 60)};
		}
	}

	// create stone texture
	// top bottom side
	stone_texture = malloc(sizeof(texture_t));
    stone_texture->pixels = malloc(width * height * 3 * sizeof(colour_t));
	stone_texture->width = width;
	stone_texture->height = height;

	i = 0;
	for (i; i < SQUARES_PER_FACE * 3; i++) {
		stone_texture->pixels[i] = (colour_t){110 + (rand()  % 20), 110 + (rand()  % 20), 120 + (rand()  % 20)};
	}

	// create dirt texture
	// top bottom side
	dirt_texture = malloc(sizeof(texture_t));
    dirt_texture->pixels = malloc(width * height * 3 * sizeof(colour_t));
	dirt_texture->width = width;
	dirt_texture->height = height;

	i = 0;
	for (i; i < SQUARES_PER_FACE * 3; i++) {
		dirt_texture->pixels[i] = (colour_t){150 + (rand() % 70), 75 + (rand() % 60), 10 + (rand() % 60)};
	}

	// create wood texture
	// * 3 for top bottom side of cube
	// top
	wood_texture = malloc(sizeof(texture_t));
    wood_texture->pixels = malloc(width * height * 3 * sizeof(colour_t));
	wood_texture->width = width;
	wood_texture->height = height;

	i = 0;
	for (i; i < SQUARES_PER_FACE; i++) {
		wood_texture->pixels[i] = (colour_t){200 + (rand() % 50), 180 + (rand() % 50), 150 + (rand() % 50)};
	}
	// bottom
	for (i; i < 2 * SQUARES_PER_FACE; i++) {
		wood_texture->pixels[i] = (colour_t){200 + (rand() % 50), 180 + (rand() % 50), 150 + (rand() % 50)};
	}
	// side
	for (i; i < 3 * SQUARES_PER_FACE; i++) {
		wood_texture->pixels[i] = (colour_t){ 110 + (rand() % 7),  70 + (rand() % 15),  40 + (rand() % 10)};
	}

	// create leaf texture
	// top bottom side
	leaf_texture = malloc(sizeof(texture_t));
    leaf_texture->pixels = malloc(width * height * 3 * sizeof(colour_t));
	leaf_texture->width = width;
	leaf_texture->height = height;

	i = 0;
	for (i; i < SQUARES_PER_FACE * 3; i++) {
		leaf_texture->pixels[i] = (colour_t){5 - (rand()  % 5), 95 - (rand()  % 40), 7 - (rand()  % 7)};
	}

	// create sand texture
	// top bottom side
	sand_texture = malloc(sizeof(texture_t));
    sand_texture->pixels = malloc(width * height * 3 * sizeof(colour_t));
	sand_texture->width = width;
	sand_texture->height = height;

	i = 0;
	for (i; i < SQUARES_PER_FACE * 3; i++) {
		sand_texture->pixels[i] = (colour_t){240 - (rand()  % 5), 190 - (rand()  % 40), 80 - (rand()  % 7)};
	}

	// create water texture
	// top bottom side
	water_texture = malloc(sizeof(texture_t));
    water_texture->pixels = malloc(width * height * 3 * sizeof(colour_t));
	water_texture->width = width;
	water_texture->height = height;

	i = 0;
	for (i; i < SQUARES_PER_FACE * 3; i++) {
		water_texture->pixels[i] = (colour_t){50 + (rand() % 50), 50 + (rand() % 50), 255};
	}

	// create black texture
	// top bottom side
	black_texture = malloc(sizeof(texture_t));
    black_texture->pixels = malloc(width * height * 3 * sizeof(colour_t));
	black_texture->width = width;
	black_texture->height = height;

	i = 0;
	for (i; i < SQUARES_PER_FACE * 3; i++) {
		black_texture->pixels[i] = (colour_t){30 + (rand()  % 20), 30 + (rand()  % 20), 30 + (rand()  % 20)};
	}

	// create white texture
	// top bottom side
	white_texture = malloc(sizeof(texture_t));
    white_texture->pixels = malloc(width * height * 3 * sizeof(colour_t));
	white_texture->width = width;
	white_texture->height = height;

	i = 0;
	for (i; i < SQUARES_PER_FACE * 3; i++) {
		white_texture->pixels[i] = (colour_t){220 + (rand()  % 20), 220 + (rand()  % 20), 220 + (rand()  % 20)};
	}

	// create beige texture
	// top bottom side
	beige_texture = malloc(sizeof(texture_t));
    beige_texture->pixels = malloc(width * height * 3 * sizeof(colour_t));
	beige_texture->width = width;
	beige_texture->height = height;

	i = 0;
	for (i; i < SQUARES_PER_FACE * 3; i++) {
		beige_texture->pixels[i] = (colour_t){200 + (rand()  % 20), 150 + (rand()  % 20), 120 + (rand()  % 20)};
	}

	return;
}

// gpt perlin:
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
