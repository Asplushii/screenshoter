#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <png.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define ERROR_FILE_OPEN 2
#define ERROR_PNG_STRUCT 3
#define ERROR_PNG_INFO 4
#define ERROR_PNG_CREATION 5
#define ERROR_CAPTURE_SCREEN 6
#define ERROR_DISPLAY_OPEN 9

void handle_error(const char* error_message, int exit_code) {
    fprintf(stderr, "Error: %s\n", error_message);
    exit(exit_code);
}

XImage* capture_screen(Display* display, Window window) {
    XWindowAttributes window_attributes;
    if (!XGetWindowAttributes(display, window, &window_attributes)) {
        handle_error("Failed to get window attributes", ERROR_CAPTURE_SCREEN);
    }
    return XGetImage(display, window, 0, 0, window_attributes.width, window_attributes.height, AllPlanes, ZPixmap);
}

void save_image_to_png(XImage* img, const char* filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) handle_error("Could not open file for writing", ERROR_FILE_OPEN);

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) handle_error("png_create_write_struct failed", ERROR_PNG_STRUCT);

    png_infop info = png_create_info_struct(png);
    if (!info) handle_error("png_create_info_struct failed", ERROR_PNG_INFO);

    if (setjmp(png_jmpbuf(png))) handle_error("Error during PNG creation", ERROR_PNG_CREATION);

    png_init_io(png, fp);
    png_set_IHDR(png, info, img->width, img->height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_bytep row_pointers[img->height];
    for (int y = 0; y < img->height; y++) {
        png_byte* row = (png_byte*)malloc(img->width * 3 * sizeof(png_byte));
        for (int x = 0; x < img->width; x++) {
            unsigned long pixel = XGetPixel(img, x, y);
            row[x * 3] = (pixel >> 16) & 0xFF;
            row[x * 3 + 1] = (pixel >> 8) & 0xFF;
            row[x * 3 + 2] = pixel & 0xFF;
        }
        row_pointers[y] = row;
    }

    png_set_rows(png, info, row_pointers);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);

    for (int y = 0; y < img->height; y++) {
        free(row_pointers[y]);
    }
    png_destroy_write_struct(&png, &info);
    fclose(fp);
}

Window select_window(Display* display) {
    Window root = DefaultRootWindow(display);
    Window selected_window = None;
    Cursor cursor;
    XEvent event;

    cursor = XCreateFontCursor(display, XC_crosshair);
    XDefineCursor(display, root, cursor);

    while (1) {
        XNextEvent(display, &event);
        switch (event.type) {
            case ButtonPress:
                if (event.xbutton.button == Button1) {
                    selected_window = root;
                    goto end;
                }
                break;
            case MotionNotify:
                break;
            case ButtonRelease:
                break;
        }
    }

end:
    XUndefineCursor(display, root);
    return selected_window;
}

void screenshot_active_window(Display* display) {
    Window selected_window = select_window(display);
    if (selected_window != None) {
        XImage* img = capture_screen(display, selected_window);
        if (img) {
            save_image_to_png(img, "/tmp/screenshot_active_window.png");
            XDestroyImage(img);
            system("xclip -selection clipboard -t image/png -i /tmp/screenshot_active_window.png");
            system("notify-send -i /tmp/screenshot_active_window.png 'Screenshot' 'Screenshot saved and copied to clipboard'");
        } else {
            handle_error("Failed to capture screen", ERROR_CAPTURE_SCREEN);
        }
    }
}

void screenshot_whole_screen(Display* display) {
    XImage* img = capture_screen(display, DefaultRootWindow(display));
    if (img) {
        save_image_to_png(img, "/tmp/screenshot_whole_screen.png");
        XDestroyImage(img);
        system("xclip -selection clipboard -t image/png -i /tmp/screenshot_whole_screen.png");
        system("notify-send -i /tmp/screenshot_whole_screen.png 'Screenshot' 'Screenshot saved and copied to clipboard'");
    } else {
        handle_error("Failed to capture screen", ERROR_CAPTURE_SCREEN);
    }
}

int main(int argc, char* argv[]) {
    int delay = 0, active = 0, select = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--now") == 0) {
            delay = 0;
        } else if (strcmp(argv[i], "--select") == 0) {
            select = 1;
        } else if (strcmp(argv[i], "--active") == 0) {
            active = 1;
        } else if (strncmp(argv[i], "--in", 4) == 0) {
            delay = atoi(argv[i] + 4);
            if (delay <= 0) {
                delay = 0;
            }
        }
    }

    Display* display = XOpenDisplay(NULL);
    if (!display) handle_error("Failed to open display", ERROR_DISPLAY_OPEN);

    if (delay > 0) sleep(delay);

    if (select || active) screenshot_active_window(display);
    else screenshot_whole_screen(display);

    XCloseDisplay(display);
    return 0;
}
