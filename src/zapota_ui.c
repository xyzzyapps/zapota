#include "zapota_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

enum { UI_W = 480, UI_H = 360 };

static float curve_y(double cutoff_hz, float x, float left, float width, float top, float height) {
    /* Log-frequency 20 Hz .. 20 kHz across the panel. */
    double t = (double)(x - left) / (double)width;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    double freq = 20.0 * pow(1000.0, t);
    double ratio = freq / cutoff_hz;
    double mag = 1.0 / sqrt(1.0 + ratio * ratio);
    return top + (float)((1.0 - mag) * (double)height);
}

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static double g_cutoff;

static void paint_filter(HDC dc, double cutoff_hz) {
    RECT all = { 0, 0, UI_W, UI_H };
    HBRUSH bg = CreateSolidBrush(RGB(30, 30, 35));
    FillRect(dc, &all, bg);
    DeleteObject(bg);

    RECT panel = { 20, 20, UI_W - 20, 200 };
    HBRUSH panel_br = CreateSolidBrush(RGB(15, 15, 20));
    FillRect(dc, &panel, panel_br);
    DeleteObject(panel_br);

    HPEN curve = CreatePen(PS_SOLID, 3, RGB(0, 220, 255));
    HGDIOBJ old = SelectObject(dc, curve);
    float left = 28.0f, width = (float)(UI_W - 56), top = 32.0f, height = 156.0f;
    MoveToEx(dc, (int)left, (int)curve_y(cutoff_hz, left, left, width, top, height), NULL);
    for (int x = (int)left + 1; x <= (int)(left + width); x++) {
        LineTo(dc, x, (int)curve_y(cutoff_hz, (float)x, left, width, top, height));
    }
    SelectObject(dc, old);
    DeleteObject(curve);

    int cx = UI_W / 2, cy = 270, r = 36;
    HBRUSH knob = CreateSolidBrush(RGB(50, 50, 60));
    HPEN ring = CreatePen(PS_SOLID, 2, RGB(80, 80, 100));
    old = SelectObject(dc, knob);
    HGDIOBJ oldp = SelectObject(dc, ring);
    Ellipse(dc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(dc, old);
    SelectObject(dc, oldp);
    DeleteObject(knob);
    DeleteObject(ring);

    double norm = (cutoff_hz - 20.0) / (20000.0 - 20.0);
    double ang = (norm * 270.0 - 135.0) * 3.14159265358979323846 / 180.0;
    HPEN needle = CreatePen(PS_SOLID, 3, RGB(255, 180, 0));
    old = SelectObject(dc, needle);
    MoveToEx(dc, cx, cy, NULL);
    LineTo(dc, cx + (int)(r * 0.75 * sin(ang)), cy - (int)(r * 0.75 * cos(ang)));
    SelectObject(dc, old);
    DeleteObject(needle);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(220, 220, 220));
    wchar_t label[64];
    swprintf(label, 64, L"Zapota lowpass    cutoff %.0f Hz", cutoff_hz);
    TextOutW(dc, 24, 318, label, (int)wcslen(label));
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        paint_filter(dc, g_cutoff);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_TIMER || msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int zapota_ui_show(double cutoff_hz) {
    g_cutoff = cutoff_hz;
    printf("zapota UI: Win32 GDI filter view (cutoff %.0f Hz)\n", cutoff_hz);
    HINSTANCE inst = GetModuleHandleW(NULL);
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"zapota_filter";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, L"zapota_filter", L"Zapota Filter",
                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT, UI_W, UI_H,
                                 NULL, NULL, inst, NULL);
    if (!hwnd) {
        fprintf(stderr, "CreateWindowExW failed\n");
        return 1;
    }
    SetTimer(hwnd, 1, 2500, NULL);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    printf("window closed\n");
    return 0;
}

#elif defined(__APPLE__)

#include <dlfcn.h>

typedef struct { double x, y, w, h; } CGRect;
typedef void *id;
typedef void *SEL;
typedef id (*get_class_fn)(const char *);
typedef SEL (*sel_fn)(const char *);
typedef id (*msg0_fn)(id, SEL);
typedef id (*msg_id_fn)(id, SEL, id);
typedef id (*msg_cstr_fn)(id, SEL, const char *);
typedef id (*msg_win_fn)(id, SEL, CGRect, unsigned long, unsigned long, signed char);
typedef id (*msg_d_fn)(id, SEL, double);
typedef void (*msg_long_fn)(id, SEL, long);
typedef void (*msg_bool_fn)(id, SEL, signed char);
typedef id (*msg_rgba_fn)(id, SEL, double, double, double, double);

int zapota_ui_show(double cutoff_hz) {
    printf("zapota UI: AppKit filter view (cutoff %.0f Hz)\n", cutoff_hz);
    void *objc = dlopen("/usr/lib/libobjc.A.dylib", RTLD_NOW);
    void *appkit = dlopen("/System/Library/Frameworks/AppKit.framework/AppKit", RTLD_NOW);
    if (!objc || !appkit) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    get_class_fn getClass = (get_class_fn)dlsym(objc, "objc_getClass");
    sel_fn reg = (sel_fn)dlsym(objc, "sel_registerName");
    void *send = dlsym(objc, "objc_msgSend");
    if (!getClass || !reg || !send) return 1;

    msg0_fn msg0 = (msg0_fn)send;
    msg_id_fn msg_id = (msg_id_fn)send;
    msg_cstr_fn msg_cstr = (msg_cstr_fn)send;
    msg_win_fn msg_win = (msg_win_fn)send;
    msg_d_fn msg_d = (msg_d_fn)send;
    msg_long_fn msg_long = (msg_long_fn)send;
    msg_bool_fn msg_bool = (msg_bool_fn)send;
    msg_rgba_fn msg_rgba = (msg_rgba_fn)send;

    id app = msg0(getClass("NSApplication"), reg("sharedApplication"));
    msg_long(app, reg("setActivationPolicy:"), 0);

    CGRect rect = { 200, 200, UI_W, UI_H };
    id window = msg_win(msg0(getClass("NSWindow"), reg("alloc")),
                        reg("initWithContentRect:styleMask:backing:defer:"),
                        rect, 15UL, 2UL, 0);
    char caption[96];
    snprintf(caption, sizeof(caption), "Zapota lowpass  %.0f Hz", cutoff_hz);
    msg_id(window, reg("setTitle:"), msg_cstr(getClass("NSString"), reg("stringWithUTF8String:"), caption));

    id color = msg_rgba(getClass("NSColor"), reg("colorWithRed:green:blue:alpha:"),
                        30.0 / 255.0, 30.0 / 255.0, 35.0 / 255.0, 1.0);
    id content = msg0(window, reg("contentView"));
    msg_id(content, reg("setWantsLayer:"), (id)1);
    /* layer background */
    typedef void (*msg_setbg_fn)(id, SEL, id);
    ((msg_setbg_fn)send)(content, reg("setBackgroundColor:"), color) ;

    id field = msg0(msg0(getClass("NSTextField"), reg("alloc")), reg("init"));
    msg_id(field, reg("setStringValue:"),
           msg_cstr(getClass("NSString"), reg("stringWithUTF8String:"), caption));
    msg_bool(field, reg("setBezeled:"), 0);
    msg_bool(field, reg("setDrawsBackground:"), 0);
    msg_bool(field, reg("setEditable:"), 0);
    typedef void (*msg_frame_fn)(id, SEL, CGRect);
    ((msg_frame_fn)send)(field, reg("setFrame:"), (CGRect){ 24, 24, 420, 28 });
    msg_id(content, reg("addSubview:"), field);

    msg_id(window, reg("makeKeyAndOrderFront:"), (id)0);
    msg_bool(app, reg("activateIgnoringOtherApps:"), 1);

    id until = msg_d(getClass("NSDate"), reg("dateWithTimeIntervalSinceNow:"), 2.0);
    typedef id (*next_fn)(id, SEL, unsigned long long, id, id, signed char);
    next_fn nextEvent = (next_fn)send;
    id mode = msg_cstr(getClass("NSString"), reg("stringWithUTF8String:"), "NSDefaultRunLoopMode");
    typedef signed char (*cmp_fn)(id, SEL, id);
    cmp_fn cmp = (cmp_fn)send;
    for (;;) {
        id ev = nextEvent(app, reg("nextEventMatchingMask:untilDate:inMode:dequeue:"),
                          ~0ULL, until, mode, 1);
        if (ev) msg_id(app, reg("sendEvent:"), ev);
        id now = msg0(getClass("NSDate"), reg("date"));
        if (cmp(now, reg("compare:"), until) != (signed char)-1) break;
    }
    printf("window closed\n");
    return 0;
}

#else

#include <dlfcn.h>
#include <time.h>

typedef unsigned long XID;
typedef XID Window;
typedef XID Pixmap;
typedef XID Drawable;
typedef XID GC;
typedef XID Colormap;
typedef struct _XDisplay Display;
typedef struct { unsigned long pixel; unsigned short red, green, blue; char flags, pad; } XColor;

typedef Display *(*fn_XOpenDisplay)(const char *);
typedef int (*fn_XDefaultScreen)(Display *);
typedef Window (*fn_XRootWindow)(Display *, int);
typedef Colormap (*fn_XDefaultColormap)(Display *, int);
typedef int (*fn_XAllocColor)(Display *, Colormap, XColor *);
typedef Window (*fn_XCreateSimpleWindow)(Display *, Window, int, int, unsigned, unsigned, unsigned, unsigned long, unsigned long);
typedef int (*fn_XStoreName)(Display *, Window, const char *);
typedef int (*fn_XMapWindow)(Display *, Window);
typedef int (*fn_XFlush)(Display *);
typedef int (*fn_XPending)(Display *);
typedef GC (*fn_XCreateGC)(Display *, Drawable, unsigned long, void *);
typedef int (*fn_XSetForeground)(Display *, GC, unsigned long);
typedef int (*fn_XFillRectangle)(Display *, Drawable, GC, int, int, unsigned, unsigned);
typedef int (*fn_XDrawLine)(Display *, Drawable, GC, int, int, int, int);
typedef int (*fn_XDrawArc)(Display *, Drawable, GC, int, int, unsigned, unsigned, int, int);
typedef int (*fn_XDrawString)(Display *, Drawable, GC, int, int, const char *, int);
typedef int (*fn_XFreeGC)(Display *, GC);
typedef int (*fn_XCloseDisplay)(Display *);
typedef unsigned long (*fn_XBlackPixel)(Display *, int);
typedef unsigned long (*fn_XWhitePixel)(Display *, int);

#define LOAD(lib, T, name)                              \
    T name = (T)dlsym(lib, #name);                      \
    if (!name) {                                        \
        fprintf(stderr, "missing symbol %s\n", #name);  \
        return 1;                                       \
    }

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static unsigned long color_of(Display *dpy, Colormap cmap, fn_XAllocColor alloc, unsigned char r, unsigned char g, unsigned char b, unsigned long fallback) {
    XColor c;
    memset(&c, 0, sizeof(c));
    c.red = (unsigned short)(r * 257);
    c.green = (unsigned short)(g * 257);
    c.blue = (unsigned short)(b * 257);
    c.flags = 7;
    if (!alloc(dpy, cmap, &c)) return fallback;
    return c.pixel;
}

int zapota_ui_show(double cutoff_hz) {
    printf("zapota UI: X11 filter view (cutoff %.0f Hz)\n", cutoff_hz);
    void *lib = dlopen("libX11.so.6", RTLD_NOW);
    if (!lib) lib = dlopen("libX11.so", RTLD_NOW);
    if (!lib) {
        fprintf(stderr, "libX11 not installed (%s)\n", dlerror());
        return 1;
    }
    LOAD(lib, fn_XOpenDisplay, XOpenDisplay);
    LOAD(lib, fn_XDefaultScreen, XDefaultScreen);
    LOAD(lib, fn_XRootWindow, XRootWindow);
    LOAD(lib, fn_XDefaultColormap, XDefaultColormap);
    LOAD(lib, fn_XAllocColor, XAllocColor);
    LOAD(lib, fn_XCreateSimpleWindow, XCreateSimpleWindow);
    LOAD(lib, fn_XStoreName, XStoreName);
    LOAD(lib, fn_XMapWindow, XMapWindow);
    LOAD(lib, fn_XFlush, XFlush);
    LOAD(lib, fn_XPending, XPending);
    LOAD(lib, fn_XCreateGC, XCreateGC);
    LOAD(lib, fn_XSetForeground, XSetForeground);
    LOAD(lib, fn_XFillRectangle, XFillRectangle);
    LOAD(lib, fn_XDrawLine, XDrawLine);
    LOAD(lib, fn_XDrawArc, XDrawArc);
    LOAD(lib, fn_XDrawString, XDrawString);
    LOAD(lib, fn_XFreeGC, XFreeGC);
    LOAD(lib, fn_XCloseDisplay, XCloseDisplay);
    LOAD(lib, fn_XBlackPixel, XBlackPixel);
    LOAD(lib, fn_XWhitePixel, XWhitePixel);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "No DISPLAY\n");
        return 1;
    }
    int screen = XDefaultScreen(dpy);
    unsigned long black = XBlackPixel(dpy, screen);
    unsigned long white = XWhitePixel(dpy, screen);
    Colormap cmap = XDefaultColormap(dpy, screen);
    unsigned long bg = color_of(dpy, cmap, XAllocColor, 30, 30, 35, black);
    unsigned long panel = color_of(dpy, cmap, XAllocColor, 15, 15, 20, black);
    unsigned long cyan = color_of(dpy, cmap, XAllocColor, 0, 220, 255, white);
    unsigned long knob = color_of(dpy, cmap, XAllocColor, 50, 50, 60, white);
    unsigned long gold = color_of(dpy, cmap, XAllocColor, 255, 180, 0, white);

    Window root = XRootWindow(dpy, screen);
    Window win = XCreateSimpleWindow(dpy, root, 100, 100, UI_W, UI_H, 0, black, bg);
    XStoreName(dpy, win, "Zapota Filter");
    XMapWindow(dpy, win);
    GC gc = XCreateGC(dpy, win, 0, NULL);

    XSetForeground(dpy, gc, bg);
    XFillRectangle(dpy, win, gc, 0, 0, UI_W, UI_H);
    XSetForeground(dpy, gc, panel);
    XFillRectangle(dpy, win, gc, 20, 20, UI_W - 40, 180);

    XSetForeground(dpy, gc, cyan);
    float left = 28.0f, width = (float)(UI_W - 56), top = 32.0f, height = 156.0f;
    int prev_x = (int)left;
    int prev_y = (int)curve_y(cutoff_hz, left, left, width, top, height);
    for (int x = prev_x + 1; x <= (int)(left + width); x++) {
        int y = (int)curve_y(cutoff_hz, (float)x, left, width, top, height);
        XDrawLine(dpy, win, gc, prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }

    int cx = UI_W / 2, cy = 270, r = 36;
    XSetForeground(dpy, gc, knob);
    XFillRectangle(dpy, win, gc, cx - r, cy - r, (unsigned)(r * 2), (unsigned)(r * 2));
    XSetForeground(dpy, gc, gold);
    XDrawArc(dpy, win, gc, cx - r, cy - r, (unsigned)(r * 2), (unsigned)(r * 2), 0, 360 * 64);
    double norm = (cutoff_hz - 20.0) / (20000.0 - 20.0);
    double ang = (norm * 270.0 - 135.0) * 3.14159265358979323846 / 180.0;
    XDrawLine(dpy, win, gc, cx, cy,
              cx + (int)(r * 0.75 * sin(ang)),
              cy - (int)(r * 0.75 * cos(ang)));

    XSetForeground(dpy, gc, white);
    char label[96];
    snprintf(label, sizeof(label), "Zapota lowpass  cutoff %.0f Hz", cutoff_hz);
    XDrawString(dpy, win, gc, 24, 330, label, (int)strlen(label));
    XFlush(dpy);
    printf("window mapped\n");

    double start = now_sec();
    while (now_sec() - start < 2.5) {
        (void)XPending(dpy);
        struct timespec pause = { 0, 50 * 1000 * 1000 };
        nanosleep(&pause, NULL);
    }
    XFreeGC(dpy, gc);
    XCloseDisplay(dpy);
    printf("window closed\n");
    return 0;
}

#endif
