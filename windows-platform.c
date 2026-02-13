#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#define WIDTH  800
#define HEIGHT 500

#define FRAME_LENGTH 16 // milliseconds - 60fps

static void update_pixels();

// --- Our pixel buffer (points into the Device Independent Bitmap section) ---
static uint32_t *pixels = NULL;

// --- Bitmap and Device Context we render into ---
static HBITMAP bitmap = NULL;
static HDC memDC = NULL;

static POINT mouse = {0};

HWND hwnd;

// --- window message handler ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
		case WM_TIMER:
		// --- this is our draw loop ---
		{
			GetCursorPos(&mouse);
			ScreenToClient(hwnd, &mouse);

			update_pixels();

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
	SetTimer(hwnd, 1, FRAME_LENGTH, NULL); // ~60 Hz

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

void update_pixels() {
	int r = 0;
	int g = 255;
	int b = 0;
	int a = 1;
	for (int i = 0; i < WIDTH * HEIGHT; i++) {
		pixels[i] = (a << 24 | r << 16) | (g << 8) | b;
	}
}
