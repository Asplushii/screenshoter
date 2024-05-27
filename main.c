#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void save_to_clipboard(const char *filename) {
    char command[256];
    snprintf(command, sizeof(command), "xclip -selection clipboard -t image/png -i %s", filename);
    system(command);
}

void screenshot_now(const char *filename) {
    system("import -window root screenshot.png");
    save_to_clipboard(filename);
}

void screenshot_select(const char *filename) {
    system("import screenshot.png");
    save_to_clipboard(filename);
}

void screenshot_in(const char *filename, int seconds) {
    sleep(seconds);
    screenshot_now(filename);
}

Window select_window(Display *display) {
    Window root = DefaultRootWindow(display);
    Window selected_window = None;
    int screen = DefaultScreen(display);
    GC gc = DefaultGC(display, screen);
    XColor color;
    Colormap colormap = DefaultColormap(display, screen);
    XParseColor(display, colormap, "red", &color);
    XAllocColor(display, colormap, &color);

    XGrabPointer(display, root, False, ButtonPressMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    while (1) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == MotionNotify) {
            Window window;
            int win_x, win_y;
            unsigned int mask;
            XQueryPointer(display, root, &window, &selected_window, &win_x, &win_y, &win_x, &win_y, &mask);
            if (selected_window != None) {
                XDrawRectangle(display, root, gc, 0, 0, DisplayWidth(display, screen), DisplayHeight(display, screen));
                XDrawRectangle(display, root, gc, 1, 1, DisplayWidth(display, screen) - 2, DisplayHeight(display, screen) - 2);
            }
            XFlush(display);
        }
        else if (event.type == ButtonPress) {
            break;
        }
    }
    XUngrabPointer(display, CurrentTime);
    return selected_window;
}

void screenshot_active_window(const char *filename) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to open display\n");
        exit(1);
    }

    Window selected_window = select_window(display);
    if (selected_window != None) {
        char command[256];
        snprintf(command, sizeof(command), "import -window 0x%lx screenshot.png", selected_window);
        system(command);
        save_to_clipboard(filename);
    }

    XCloseDisplay(display);
}

int main(int argc, char *argv[]) {
    int delay = 0;
    int active = 0;
    int select = 0;
    char *filename = "screenshot.png";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--now") == 0) {
            delay = 0;
        } else if (strcmp(argv[i], "--select") == 0) {
            select = 1;
        } else if (strcmp(argv[i], "--in10") == 0) {
            delay = 10;
        } else if (strcmp(argv[i], "--in5") == 0) {
            delay = 5;
        } else if (strcmp(argv[i], "--active") == 0) {
            active = 1;
        }
    }

    if (select) {
        screenshot_select(filename);
    } else if (active) {
        screenshot_active_window(filename);
    } else {
        screenshot_in(filename, delay);
    }

    return 0;
}
