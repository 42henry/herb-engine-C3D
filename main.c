#include <windows.h>
#include <stdint.h>
#include <stdlib.h>

#define WIDTH  800
#define HEIGHT 500
#define CUBE_WIDTH 50

#define WM_PIXELS_BOUNDS_ERROR (WM_USER + 0)

typedef struct {
	int x;
	int y;
	int z;	
} Vec3;

typedef struct {
	Vec3 *squares;
	int count;
} Squares_t;

// --- Our pixel buffer (points into the Device Independent Bitmap section) ---
static uint32_t *pixels = NULL;
static Squares_t square_array;

// --- Bitmap and Device Context we render into ---
static HBITMAP bitmap = NULL;
static HDC memDC = NULL;

static void init_stuff();
static void update_pixels(uint32_t *pixels);
static uint32_t get_colour(int a, int r, int g, int b);
static int set_pixel(int x, int y, int r, int g, int b);

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
		case WM_PIXELS_BOUNDS_ERROR:
		{
			char szFileName[MAX_PATH];
			HINSTANCE hInstance = GetModuleHandle(NULL);

			GetModuleFileName(hInstance, szFileName, MAX_PATH);
			MessageBox(hwnd, szFileName, "Accessed pixels out of bounds", MB_OK | MB_ICONINFORMATION);
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

    // --- Fill pixels with a test pattern ---
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            uint8_t r = (uint8_t)x;
            uint8_t g = (uint8_t)y;
            uint8_t b = 0;

            pixels[y * WIDTH + x] =
                (r << 16) | (g << 8) | b;
        }
    }

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
    Vec3 *cube = malloc(sizeof(Vec3));
	cube->x = 50;
	cube->y = 50;
	cube->z = 50;
	square_array.squares = cube;
	square_array.count = 1;
}

void update_pixels(uint32_t *pixels) {
	if (set_pixel(100, 1000000, 255, 0, 0) < 0) {
		return;
	}
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            pixels[y * WIDTH + x] = get_colour(1, 0, 255, 0);
        }
    }


	//for (int i = 0; i < square_array.count; i++) {
		//for (int j = 0; j < CUBE_WIDTH; j++) {
			//for (int k = 0; k < CUBE_WIDTH; k++) {
				//int x = square_array.squares[0].x;
				//int y = square_array.squares[0].y;
				//x += j;
				//y += k;
				//pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
			//}
		//}
	//}

	int x1 = 100;
	int y1 = 100;

	int x2 = 300;
	int y2 = 400;

	int change_in_y = y2 - y1;
	int change_in_x = x2 - x1;

	float m = (float)change_in_y / (float)change_in_x;
	int c = y1 - (m * x1);

	// TODO: what if change in x or y is negative :o
	if (change_in_y > change_in_x) {
		for (int y = y1; y < y2; y++) {
			int x = (y - c) / m;
			pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
		}
	}
	else {
		for (int x = x1; x < x2; x++) {
			int y = (m * x) + c;
			pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
		}
	}

	x1 = 100;
	y1 = 100;

	x2 = 700;
	y2 = 300;

	change_in_y = y2 - y1;
	change_in_x = x2 - x1;

	m = (float)change_in_y / (float)change_in_x;
	c = y1 - (m * x1);

	// TODO: what if change in x or y is negative :o
	if (change_in_y > change_in_x) {
		for (int y = y1; y < y2; y++) {
			int x = (y - c) / m;
			pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
		}
	}
	else {
		for (int x = x1; x < x2; x++) {
			int y = (m * x) + c;
			pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
		}
	}

	x1 = 700;
	y1 = 100;

	x2 = 100;
	y2 = 300;

	change_in_y = y2 - y1;
	change_in_x = x2 - x1;

	m = (float)change_in_y / (float)change_in_x;
	c = y1 - (m * x1);

	// TODO: make it so we cant set a pixel out off array
	if (abs(change_in_y) > abs(change_in_x)) {
		if (change_in_y > 0) {
			for (int y = y1; y < y2; y++) {
				int x = (y - c) / m;
				pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
			}
		}
		else {
			for (int y = y1; y > y2; y--) {
				int x = (y - c) / m;
				pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
			}
		}
	}
	else {
		if (change_in_x > 0) {
			for (int x = x1; x < x2; x++) {
				int y = (m * x) + c;
				pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
			}
		}
		else {
			for (int x = x1; x > x2; x--) {
				int y = (m * x) + c;
				pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
			}
		}
	}

	x1 = 100;
	y1 = 600;

	x2 = 200;
	y2 = 100;

	change_in_y = y2 - y1;
	change_in_x = x2 - x1;

	m = (float)change_in_y / (float)change_in_x;
	c = y1 - (m * x1);

	// TODO: make it so we cant set a pixel out off array
	if (abs(change_in_y) > abs(change_in_x)) {
		if (change_in_y > 0) {
			for (int y = y1; y <= y2; y++) {
				int x = (y - c) / m;
				pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
			}
		}
		else {
			for (int y = y1; y >= y2; y--) {
				int x = (y - c) / m;
				if ((y * WIDTH + x) < (WIDTH * HEIGHT)) {
					pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
				}
			}
		}
	}
	else {
		if (change_in_x > 0) {
			for (int x = x1; x <= x2; x++) {
				int y = (m * x) + c;
				pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
			}
		}
		else {
			for (int x = x1; x >= x2; x--) {
				int y = (m * x) + c;
				pixels[y * WIDTH + x] = get_colour(1, 255, 0, 0);
			}
		}
	}

	// access an x, y by [y * WIDTH + x]
	// set pixel like pixels[i] = get_colour(a, r, g, b)
}

uint32_t get_colour(int a, int r, int g, int b) {
    return (a << 24 | r << 16) | (g << 8) | b;
}

int set_pixel(int x, int y, int r, int g, int b) {
	if ((y * WIDTH + x) < (WIDTH * HEIGHT)) {
		pixels[y * WIDTH + x] = get_colour(1, r, g, b);
		return 0;
	}
	else {
		PostMessage(hwnd, WM_PIXELS_BOUNDS_ERROR, 0, 0);
		return -1;
	}
}
