#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <png.h>
#include <omp.h>

void handle_error(const char *msg, int code) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(code);
}

XImage *capture_screen(Display *d, Window w) {
    XWindowAttributes a;
    XGetWindowAttributes(d, w, &a);
    return XGetImage(d, w, 0, 0, a.width, a.height, AllPlanes, ZPixmap);
}

void save_image_to_png(XImage *i, const char *f) {
    FILE *fp = fopen(f, "wb");
    if (!fp) {
        handle_error("Failed to open file for writing", 1);
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        handle_error("Failed to create PNG structure", 1);
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        handle_error("Failed to create PNG info structure", 1);
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info, i->width, i->height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    png_bytep row_pointers[i->height];

    for (int y = 0; y < i->height; y++) {
        row_pointers[y] = (png_bytep)malloc(i->width * 3 * sizeof(png_byte));
        if (!row_pointers[y]) {
            handle_error("Failed to allocate memory for row", 1);
        }
        for (int x = 0; x < i->width; x++) {
            unsigned long p = XGetPixel(i, x, y);
            int offset = x * 3;
            row_pointers[y][offset] = (p >> 16) & 0xFF;
            row_pointers[y][offset + 1] = (p >> 8) & 0xFF;
            row_pointers[y][offset + 2] = p & 0xFF;
        }
    }

    png_write_image(png, row_pointers);

    for (int y = 0; y < i->height; y++) {
        free(row_pointers[y]);
    }

    png_write_end(png, NULL);
    fclose(fp);
}

void screenshot_whole_screen(Display *d) {
    XImage *i = capture_screen(d, DefaultRootWindow(d));
    save_image_to_png(i, "/tmp/screenshot.png");
    XDestroyImage(i);

    system("notify-send -u low -t 1500 -i /tmp/screenshot.png 'Screenshot' 'Screenshot saved and copied to clipboard'");
    system("xclip -selection clipboard -t image/png -i /tmp/screenshot.png");
}

int main(int argc, char *argv[]) {
    int d = 0;
    if (argc == 1 || strncmp(argv[1], "--now", 5) == 0 || strncmp(argv[1], "-n", 2) == 0) {
        Display *display = XOpenDisplay(NULL);
        if (!display) handle_error("Failed to open display", 1);
        screenshot_whole_screen(display);
        XCloseDisplay(display);
        return 0;
    } else if (strncmp(argv[1], "--in", 4) == 0) {
        d = atoi(argv[1] + 4);
        if (d <= 0) handle_error("Invalid delay value", 1);
    } else if (argc == 1 || strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [--now, -n | --inX]\n", argv[0]);
        printf("Options:\n");
        printf("  --now, -n: Take screenshot immediately.\n");
        printf("  --inX: Take screenshot after X seconds.\n");
        return 0;
    } else {
        handle_error("Invalid argument", 1);
    }

    Display *display = XOpenDisplay(NULL);
    if (!display) handle_error("Failed to open display", 1);
    if (d > 0) sleep(d);
    screenshot_whole_screen(display);
    XCloseDisplay(display);
    return 0;
}
