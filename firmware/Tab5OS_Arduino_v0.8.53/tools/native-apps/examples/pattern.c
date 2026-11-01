/* Minimal cooperative graphical application. UI controls belong to Tab5OS. */
#include "Tab5AppApi.h"
int tab5_app_main(int argc, char **argv) {
    (void)argc; (void)argv;
    const struct tab5_app_api2 *api = tab5_app_api_v2();
    if (!api || api->base.version != TAB5_APP_ABI_V2 || api->base.size < sizeof(*api)) return 1;
    const unsigned width = 128, height = 96;
    uint16_t *pixels = api->base.allocate(width * height * sizeof(*pixels));
    if (!pixels) return 2;
    if (api->video_begin(width, height)) { api->base.release(pixels); return 3; }
    unsigned frame = 0;
    while (!api->base.should_stop() && !(api->buttons() & (1u << 8))) {
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x)
                pixels[y * width + x] = (uint16_t)(((x + frame) & 31) << 11 | (y & 63) << 5 | ((x + y) & 31));
        if (api->video_present(pixels, width * height)) break;
        if (api->base.sleep_ms(33)) break;
        ++frame;
    }
    api->base.release(pixels);
    return 0;
}
