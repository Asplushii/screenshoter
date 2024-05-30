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
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    png_init_io(png, fp);
    png_set_IHDR(png, info, i->width, i->height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    png_byte *pixel_data = (png_byte *)malloc(i->width * i->height * 3 * sizeof(png_byte));
    if (!pixel_data) {
        fclose(fp);
        png_destroy_write_struct(&png, &info);
        handle_error("Failed to allocate memory", 1);
    }

    #pragma omp parallel for
    for (int y = 0; y < i->height; y++) {
        for (int x = 0; x < i->width; x++) {
            unsigned long p = XGetPixel(i, x, y);
            int offset = (y * i->width + x) * 3;
            pixel_data[offset] = (p >> 16) & 0xFF;
            pixel_data[offset + 1] = (p >> 8) & 0xFF;
            pixel_data[offset + 2] = p & 0xFF;
        }
    }

    for (int y = 0; y < i->height; y++) {
        png_write_row(png, &pixel_data[y * i->width * 3]);
    }

    png_write_end(png, NULL);
    fclose(fp);
    free(pixel_data);
}

void screenshot_whole_screen(Display *d) {
    XImage *i = capture_screen(d, DefaultRootWindow(d));
    save_image_to_png(i, "/tmp/screenshot.png");
    XDestroyImage(i);
    system("xclip -selection clipboard -t image/png -i /tmp/screenshot.png");
    system("notify-send -i /tmp/screenshot.png 'Screenshot' 'Screenshot saved and copied to clipboard'");
}

int main(int argc, char *argv[]) {
    int d = 0;
    if (argc == 1) handle_error("Invalid argument", 1);
    if (strncmp(argv[1], "--in", 4) == 0) {
        d = atoi(argv[1] + 4);
        if (d <= 0) handle_error("Invalid delay value", 1);
    } else if (strcmp(argv[1], "--now") != 0) handle_error("Invalid argument", 1);
    Display *display = XOpenDisplay(NULL);
    if (!display) handle_error("Failed to open display", 1);
    if (d > 0) sleep(d);
    screenshot_whole_screen(display);
    XCloseDisplay(display);
    return 0;
}
