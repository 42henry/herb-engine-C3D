#include <windows.h>
#include <mmsystem.h>
#include "herbengineC3D.c"

#define FRAME_LENGTH (FRAME_TIME_NS / 1000000)

static HBITMAP bitmap = NULL;
static HDC memDC = NULL;
HWND hwnd;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            BitBlt(hdc, 0, 0, WIDTH, HEIGHT, memDC, 0, 0, SRCCOPY);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN: {
            keys[wParam] = 1;
            return 0;
        }
        case WM_KEYUP: {
            keys[wParam] = 0;
            return 0;
        }
        case WM_LBUTTONDOWN: mouse_left_click  = 1; return 0;
        case WM_RBUTTONDOWN: mouse_right_click = 1; return 0;
        case WM_LBUTTONUP:   mouse_left_click  = 0; return 0;
        case WM_RBUTTONUP:   mouse_right_click = 0; return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    // --- Create pixel bitmap ---
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = WIDTH;
    bmi.bmiHeader.biHeight      = -HEIGHT; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    bitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    memDC  = CreateCompatibleDC(NULL);
    SelectObject(memDC, bitmap);

    // --- Register & create window ---
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "DIBWindow";
    RegisterClass(&wc);

    hwnd = CreateWindowEx(0, wc.lpszClassName, "DigMake",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);

    init_stuff();

    // --- Map keys to Windows Virtual Key codes ---
    w       = 'W';
    a       = 'A';
    s       = 'S';
    d       = 'D';
    shift   = VK_SHIFT;
    space   = VK_SPACE;
    control = VK_CONTROL;
    escape  = VK_ESCAPE;
    one     = '1';
    two     = '2';
    three   = '3';
    four    = '4';
    five    = '5';
    six     = '6';
    seven   = '7';
    eight   = '8';
    nine    = '9';

    // --- Bump timer resolution to 1ms so Sleep() is accurate ---
    timeBeginPeriod(1);
    clock_gettime(CLOCK_MONOTONIC, &last);

    int prev_hold_mouse = 0;

    // --- FPS counter state ---
    int frame_count = 0;
    int fps_display = 0;
    struct timespec fps_timer;
    clock_gettime(CLOCK_MONOTONIC, &fps_timer);

    MSG msg;
    while (1) {
        // --- Handle all pending messages (non-blocking) ---
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                timeEndPeriod(1);
                cleanup();
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // --- Mouse ---
        if (hold_mouse) {
            if (!prev_hold_mouse) {
                POINT center = { WIDTH / 2, HEIGHT / 2 };
                ClientToScreen(hwnd, &center);
                SetCursorPos(center.x, center.y);
                mouse.x = WIDTH / 2;
                mouse.y = HEIGHT / 2;
                mouse_left_click  = 0;
                mouse_right_click = 0;
                prev_hold_mouse = 1;
                while (ShowCursor(FALSE) >= 0);
                continue;
            }
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(hwnd, &p);
            mouse.x = p.x;
            mouse.y = p.y;
            POINT center = { WIDTH / 2, HEIGHT / 2 };
            ClientToScreen(hwnd, &center);
            SetCursorPos(center.x, center.y);
        } else {
            if (prev_hold_mouse) {
                while (ShowCursor(TRUE) < 0);
            }
            prev_hold_mouse = 0;
        }

        // --- Update ---
        update();

        // --- FPS: count frames, update display value once per second ---
        frame_count++;
        struct timespec fps_now;
        clock_gettime(CLOCK_MONOTONIC, &fps_now);
        long fps_elapsed = (fps_now.tv_sec  - fps_timer.tv_sec)  * 1000000000L
                         + (fps_now.tv_nsec - fps_timer.tv_nsec);
        if (fps_elapsed >= 1000000000L) {
            fps_display = frame_count;
            frame_count = 0;
            fps_timer = fps_now;
        }

        // --- Draw FPS onto memDC (on top of pixel buffer) ---
        char fps_text[32];
        sprintf(fps_text, "FPS: %d", fps_display);
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(255, 255, 255));
        RECT r = { 4, 4, 200, 24 };
        DrawText(memDC, fps_text, -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE);

        // --- Render ---
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateWindow(hwnd);

        // --- Frame timing (mirrors Linux) ---
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec  - last.tv_sec)  * 1000000000L
                     + (now.tv_nsec - last.tv_nsec);
        if (elapsed < FRAME_TIME_NS) {
            DWORD sleep_ms = (DWORD)((FRAME_TIME_NS - elapsed) / 1000000);
            if (sleep_ms > 0) Sleep(sleep_ms);
        }
        clock_gettime(CLOCK_MONOTONIC, &last);
    }

    timeEndPeriod(1);
    cleanup();
    return 0;
}
