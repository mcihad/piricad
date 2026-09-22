// Posts REAL mouse events through CoreGraphics — the same path a physical mouse
// takes — unlike System Events' `click at`, which performs an accessibility
// press on the element instead and never produces a mouse event at all.
#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void move(double x, double y)
{
    CGEventRef e = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, CGPointMake(x, y), kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, e);
    CFRelease(e);
}

static void click(double x, double y, int hold_ms, int right)
{
    CGEventType down = right ? kCGEventRightMouseDown : kCGEventLeftMouseDown;
    CGEventType up   = right ? kCGEventRightMouseUp : kCGEventLeftMouseUp;
    CGMouseButton b  = right ? kCGMouseButtonRight : kCGMouseButtonLeft;
    move(x, y);
    usleep(80 * 1000);
    CGEventRef d = CGEventCreateMouseEvent(NULL, down, CGPointMake(x, y), b);
    CGEventPost(kCGHIDEventTap, d);
    CFRelease(d);
    usleep(hold_ms * 1000);
    CGEventRef u = CGEventCreateMouseEvent(NULL, up, CGPointMake(x, y), b);
    CGEventPost(kCGHIDEventTap, u);
    CFRelease(u);
}

static void key(int code)
{
    CGEventRef d = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)code, true);
    CGEventPost(kCGHIDEventTap, d);
    CFRelease(d);
    usleep(30 * 1000);
    CGEventRef u = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)code, false);
    CGEventPost(kCGHIDEventTap, u);
    CFRelease(u);
}

int main(int argc, char** argv)
{
    if (!AXIsProcessTrusted()) { fprintf(stderr, "izin yok\n"); return 2; }
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "tik") && i + 3 < argc) {
            click(atof(argv[i+1]), atof(argv[i+2]), atoi(argv[i+3]), 0); i += 3;
        } else if (!strcmp(argv[i], "sagtik") && i + 2 < argc) {
            click(atof(argv[i+1]), atof(argv[i+2]), 60, 1); i += 2;
        } else if (!strcmp(argv[i], "git") && i + 2 < argc) {
            move(atof(argv[i+1]), atof(argv[i+2])); i += 2;
        } else if (!strcmp(argv[i], "tus") && i + 1 < argc) {
            key(atoi(argv[i+1])); i += 1;
        } else if (!strcmp(argv[i], "bekle") && i + 1 < argc) {
            usleep(atoi(argv[i+1]) * 1000); i += 1;
        } else { fprintf(stderr, "bilinmeyen: %s\n", argv[i]); return 1; }
        usleep(120 * 1000);
    }
    return 0;
}
