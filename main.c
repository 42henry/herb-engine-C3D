#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define WIDTH  800
#define HEIGHT 500
#define CUBE_WIDTH 50
#define MAX_SQUARES 100

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
	int r;
	int g;
	int b;
} Colour_t;

typedef struct {
	Vec3 coords[16];	
} Square_t;

typedef struct {
	Square_t *items;
	int count;
} Squares_t;

// --- Our pixel buffer (points into the Device Independent Bitmap section) ---
static uint32_t *pixels = NULL;
static Squares_t squares;

// --- Bitmap and Device Context we render into ---
static HBITMAP bitmap = NULL;
static HDC memDC = NULL;

static void init_stuff();
static void debug_log(char *str);

static void update_pixels(uint32_t *pixels);
static uint32_t pack_colour_to_uint32(int a, Colour_t colour);
static Colour_t unpack_colour_from_uint32(uint32_t packed_colour);
static void set_pixel(Vec2 coord, Colour_t colour);
static Colour_t get_pixel_colour(Vec2 coord);
static void clear_screen(Colour_t colour);
static void draw_line(Vec3 start, Vec3 end, Colour_t colour);
static void fill_square(Vec3 one, Vec3 two, Vec3 three, Vec3 four, Colour_t new_colour);

static Squares_t squares = {0};
static int paused = 0;

static Colour_t debug_colour = {0, 0, 255};
static Colour_t red = {255, 0, 0};
static Colour_t green = {0, 255, 0};

HWND hwnd;

// --- window message handler ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
		case WM_TIMER:
		// --- this is our draw loop ---
		{
			update_pixels(pixels);

			// --- Request redraw ---
			InvalidateRect(hwnd, NULL, FALSE);
			return 0;
		}

        case WM_PAINT:
        {
			// --- draw our bitmap of pixels to the window ---
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            BitBlt(
                hdc,
                0, 0,
                WIDTH, HEIGHT,
                memDC,
                0, 0,
                SRCCOPY
            );

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
		    // --- Close window ---
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// --- Entry Point ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    // --- Describe a 32-bit top-down bitmap ---
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = WIDTH;
    bmi.bmiHeader.biHeight      = -HEIGHT; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // --- Create the Device Independent Bitmap section (array of pixels to use as a bitmap ((map of bits)) to put on screen) ---
    bitmap = CreateDIBSection(
        NULL,
        &bmi,
        DIB_RGB_COLORS,
        (void**)&pixels,
        NULL,
        0
    );

    // --- Create a memory Device Context (virtual screen like device) and select the bitmap into it ---
    memDC = CreateCompatibleDC(NULL);
    SelectObject(memDC, bitmap);

    // --- Register window class ---
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "DIBWindow";

    RegisterClass(&wc);

    // --- Create window ---
    hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        "DigMake",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WIDTH, HEIGHT,
        NULL, NULL,
        hInstance,
        NULL
    );

    ShowWindow(hwnd, nCmdShow);

	// --- Setup a timer to redraw the window every 16 ms (60 fps) ---
	SetTimer(hwnd, 1, 16, NULL); // ~60 Hz

	init_stuff();

    // --- Message loop ---
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

void init_stuff() {
	debug_colour.r = 69;
	debug_colour.g = 69;
	debug_colour.b = 69;
	red.r = 255;

	squares.items = malloc(MAX_SQUARES * sizeof(Square_t));
	for (int i = 100; i < 400; i += 80) {
		Square_t square = {0};
		int count = 0;
		for (int j = 0; j < 4; j++) {
			for (int k = 0; k < 4; k++) {
				square.coords[count] = (Vec3){i + (k * (CUBE_WIDTH / 3)), i + (j * (CUBE_WIDTH / 3)), 10};
				count++;
			}
		}
		squares.items[squares.count] = square;
		squares.count++;
	}

	return;
}

void update_pixels(uint32_t *pixels) {
	if (paused) {
		clear_screen((Colour_t){255, 255, 0});
		return;
	}

    clear_screen((Colour_t){100, 100, 100});

	//Vec3 one = squares.items[0].coords[0];
	//Vec3 two = squares.items[0].coords[1];
	//Vec3 three = squares.items[0].coords[5];
	//Vec3 four = squares.items[0].coords[4];

	Vec3 one = {100, 90, 10};
	Vec3 two = {200, 200, 10};
	Vec3 three = {150, 210, 10};
	Vec3 four = {120, 150, 10};

	draw_line(one, two, green);
	draw_line(two, three, green);
	draw_line(three, four, green);
	draw_line(four, one, green);

	fill_square(one, two, three, four, red);

	one.x = 400;
	one.y = 100;

	two.x = 300;
	two.y = 300;

	three.x = 280;
	three.y = 200;

	four.x = 300;
	four.y = 150;

	draw_line(one, two, green);
	draw_line(two, three, green);
	draw_line(three, four, green);
	draw_line(four, one, green);

	fill_square(one, two, three, four, red);

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
	debug_log("set_pixel out of bounds");
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

	int change_in_y = end.y - start.y;
	int change_in_x = end.x - start.x;

	float m = (float)change_in_y / (float)change_in_x;
	if (change_in_x == 0){
		m = 9999999;	
	}
	int c = start.y - (m * start.x);

	if (abs(change_in_y) > abs(change_in_x)) {
		if (change_in_y > 0) {
			for (int y = start.y; y <= end.y; y++) {
				int x = (y - c) / m;
				set_pixel((Vec2){x, y}, colour);
			}
		}
		else {
			for (int y = start.y; y >= end.y; y--) {
				int x = (y - c) / m;
				set_pixel((Vec2){x, y}, colour);
			}
		}
	}
	else {
		if (change_in_x > 0) {
			for (int x = start.x; x <= end.x; x++) {
				int y = (m * x) + c;
				set_pixel((Vec2){x, y}, colour);
			}
		}
		else {
			for (int x = start.x; x >= end.x; x--) {
				int y = (m * x) + c;
				set_pixel((Vec2){x, y}, colour);
			}
		}
	}
	return;
}

void fill_square(Vec3 one, Vec3 two, Vec3 three, Vec3 four, Colour_t colour) {
	Vec3 coords[4] = {one, two, three, four};

	// find the smallest y:
	int smallest_y = HEIGHT + 1;
	int smallest_y_index = 0;
	for (int i = 0; i < 4; i++) {
		if (coords[i].y < smallest_y) {
			smallest_y = coords[i].y;
			smallest_y_index = i;
		}
	}

	// define two lines
	Vec3 top = coords[smallest_y_index];
	int left_index = smallest_y_index - 1;
	if (left_index == -1) {
		left_index = 3;
	}
	int right_index = (smallest_y_index + 1) % 4;
	Vec3 left = coords[left_index];
	Vec3 right = coords[right_index];

	int change_in_y_left = left.y - top.y;
	int change_in_x_left = left.x - top.x;

	float m_left = (float)change_in_y_left / (float)change_in_x_left;
	if (change_in_x_left == 0){
		m_left = 9999999;	
	}
	int c_left = top.y - (m_left * top.x);

	int change_in_y_right = right.y - top.y;
	int change_in_x_right = right.x - top.x;

	float m_right = (float)change_in_y_right / (float)change_in_x_right;
	if (change_in_x_right == 0){
		m_right = 9999999;	
	}
	int c_right = top.y - (m_right * top.x);

	// draw both lines, waiting until both are on the same y:
	int y_left = top.y;
	int y_right = top.y;

	// find which point has larger y:
	int y_lim = HEIGHT + 1;
	if (left.y < y_lim) {
		y_lim = left.y;
	}
	if (right.y < y_lim) {
		y_lim = right.y;
	}

	int x_left = top.x;
	int x_right = top.x;
	for (int y_end = top.y + 1; y_end < y_lim; y_end++) {
		//  draw lines to y_end
		if (abs(change_in_y_left) > abs(change_in_x_left)) {
			x_left = (y_left - c_left) / m_left;
			set_pixel((Vec2){x_left, y_left}, colour);
			y_left++;
		}
		else {
			if (change_in_x_left > 0) {
				while (y_left != y_end) {
					y_left = (m_left * x_left) + c_left;
					set_pixel((Vec2){x_left, y_left}, colour);
					x_left++;
				}
			}
			else {
				while (y_left != y_end) {
					y_left = (m_left * x_left) + c_left;
					set_pixel((Vec2){x_left, y_left}, colour);
					x_left--;
				}
			}
		}

		if (abs(change_in_y_right) > abs(change_in_x_right)) {
			x_right = (y_right - c_right) / m_right;
			set_pixel((Vec2){x_right, y_right}, colour);
			y_right++;
		}
		else {
			if (change_in_x_right > 0) {
				while (y_right != y_end) {
					y_right = (m_right * x_right) + c_right;
					set_pixel((Vec2){x_right, y_right}, colour);
					x_right++;
				}
			}
			else {
				while (y_right != y_end) {
					y_right = (m_right * x_right) + c_right;
					set_pixel((Vec2){x_right, y_right}, colour);
					x_right--;
				}
			}
		}
		//  connect lines across x
		//if (y_end > 120) {
			//char str[100];
			//sprintf(str, "x_left: %d, x_right: %d, y: %d", x_left, x_right, y_end);
			//debug_log(str);
		//}
		if (x_left < x_right) {
			for (int x = x_left; x < x_right; x++) {
				set_pixel((Vec2){x, y_end}, colour);
			}
		}
		else {
			for (int x = x_right; x < x_left; x++) {
				set_pixel((Vec2){x, y_end}, colour);
			}
		}
	}

	return;
}

void debug_log(char *str) {
	paused = 1;
	char log[100];
	strcpy(log, str);
	MessageBox(hwnd, log, "Debug Log", MB_ICONWARNING);
	PostQuitMessage(0);
}
