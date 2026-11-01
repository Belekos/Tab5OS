// Integration at the USB input boundary, with production Mouse/Focus/UiStyle
// modules and real LVGL rendering. The application scene is deliberately small;
// this is not a full Tab5HomeUi, hardware USB or display-driver simulation.
#include <lvgl.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
#include "WriterUsb.h"

namespace {
unsigned checks, changes, clicks, flushes, wideInvalidations;
bool baseline;
uint32_t now;
lv_indev_data_t touchData{};
lv_display_t* display;
std::vector<uint8_t> framebuffer(800 * 480 * 3), drawbuf(800 * 48 * 3);
void check(bool value, const char* why) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
}

// Only the transport is substituted: service() polls real relative USB reports.
namespace writing {
std::deque<MouseReport> reports;
bool connected = true;
bool usbMouseConnected() { return connected; }
bool usbMousePoll(MouseReport& report) {
    if (reports.empty()) return false;
    report = reports.front(); reports.pop_front(); return true;
}
}
#include "Tab5Mouse.cpp"

// The constructor and its passive label helpers are extracted verbatim from
// Tab5HomeUi.h by run.py. Catalogue lookups, palette/font and app launch are
// test dependencies; no hover style is added by this fixture.
namespace Tab5AppCatalogue {
constexpr unsigned MaxApps = 2, SymbolCount = 25;
struct App { uint64_t key; const char* title; unsigned symbol; };
const App* at(unsigned index) {
    static const App apps[] = {{1, "Explorateur", 1}, {2, "Éditeur", 2}};
    return index < MaxApps ? &apps[index] : nullptr;
}
unsigned icon(unsigned) { return 0; }
}
lv_obj_t* homeAppButtons[2]{};
uint64_t homeAppButtonKeys[2]{};
lv_obj_t* homeIconBoxes[2]{};
const char* T(const char* text) { return text; }
const lv_font_t* uiFont() { return &lv_font_montserrat_22; }
lv_color_t C_SURFACE() { return lv_color_hex(Tab5UiStyle::color(1)); }
lv_color_t C_ACTIVE() { return lv_color_hex(Tab5UiStyle::color(2)); }
lv_color_t C_BORDER() { return lv_color_hex(Tab5UiStyle::color(3)); }
lv_color_t C_TEXT() { return lv_color_hex(Tab5UiStyle::color(4)); }
void homeAppLaunch(lv_event_t*) { ++clicks; }
#define TAB5_SYMBOL_CALCULATOR "calculator-unused"
#define TAB5_SYMBOL_PYTHON "python-unused"
#define TAB5_SYMBOL_ALARM "alarm-unused"
void tab5CalculatorIcon(lv_obj_t*, lv_color_t) { check(false, "unexpected custom icon branch"); }
void tab5PythonIcon(lv_obj_t*, lv_color_t) { check(false, "unexpected custom icon branch"); }
void tab5AlarmIcon(lv_obj_t*, lv_color_t, lv_color_t) { check(false, "unexpected custom icon branch"); }
#include "home_card.inc"

namespace {
void paint() {
    lv_obj_update_layout(lv_screen_active());
    lv_obj_update_layout(lv_layer_top());
    lv_refr_now(display);
}
void move(int x, int y, uint8_t buttons = 0, int wheel = 0) {
    writing::MouseReport report{};
    report.x = x - Tab5Mouse::x;
    report.y = y - Tab5Mouse::y;
    report.buttons = buttons;
    report.wheel = wheel;
    writing::reports.push_back(report);
    check(Tab5Mouse::service(), "queued USB movement/button report consumed");
    paint();
}
void press(int x, int y) { move(x, y, 1); move(x, y, 0); }
lv_point_t center(lv_obj_t* object) {
    lv_area_t area; lv_obj_get_coords(object, &area);
    return {(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
}
uint32_t pixel(int x, int y) {
    const size_t i = (size_t(y) * 800 + x) * 3;
    return uint32_t(framebuffer[i + 2]) << 16 |
           uint32_t(framebuffer[i + 1]) << 8 | framebuffer[i];
}
uint32_t color(lv_obj_t* object) {
    return lv_color_to_u32(lv_obj_get_style_bg_color(object, LV_PART_MAIN)) & 0xffffff;
}
bool key(writing::Key value, lv_obj_t* scope) {
#ifdef TAB5_HOVER_FIXED
    // The OS dispatchKeyboard entrypoint invokes these modality hooks before
    // dispatching to app-specific handlers and then to Tab5Focus::key.
    Tab5Focus.keyboardMode();
    Tab5UiStyle::keyboardMode();
#endif
    writing::KeyEvent event{}; event.key = value;
    const bool consumed = Tab5Focus.key(event, scope);
    paint(); return consumed;
}
}

int main(int argc, char** argv) {
    baseline = argc > 1 && !std::strcmp(argv[1], "baseline");
    lv_init(); lv_tick_set_cb([]() { return now; });
    display = lv_display_create(800, 480);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(display, drawbuf.data(), nullptr, drawbuf.size(), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t* area, uint8_t* pixels) {
        ++flushes;
        const size_t bytes = (area->x2 - area->x1 + 1) * 3;
        const size_t stride = lv_draw_buf_width_to_stride(area->x2 - area->x1 + 1, LV_COLOR_FORMAT_RGB888);
        for (int y = area->y1; y <= area->y2; ++y)
            std::memcpy(framebuffer.data() + (size_t(y) * 800 + area->x1) * 3,
                        pixels + size_t(y - area->y1) * stride, bytes);
        lv_display_flush_ready(d);
    });
    lv_display_add_event_cb(display, [](lv_event_t* event) {
        const auto* area = static_cast<const lv_area_t*>(lv_event_get_param(event));
        if (area->x2 - area->x1 >= 100) ++wideInvalidations;
    }, LV_EVENT_INVALIDATE_AREA, nullptr);

    auto* screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    Tab5Focus.begin(lv_layer_top(), &lv_font_montserrat_22);
    Tab5Focus.theme(lv_color_hex(0xcbdce8), lv_color_hex(0x161616), lv_color_hex(0xffffff));
    homeApplicationCard(screen, 0, 20, 25, 180, 60, true, true);
    homeApplicationCard(screen, 1, 230, 25, 180, 60, true, true);
    auto* first = homeAppButtons[0];
    auto* second = homeAppButtons[1];
    // Match the real post-construction theme traversal, which must not be
    // assumed to opt every raw lv_button into the selection style.
    Tab5UiStyle::tree(screen);
    auto* choice = lv_dropdown_create(screen);
    lv_obj_set_pos(choice, 20, 135); lv_obj_set_size(choice, 220, 45);
    lv_dropdown_set_options(choice, "Premier\nDeuxième\nTroisième\nQuatrième");
    Tab5UiStyle::control(choice);
    lv_obj_add_event_cb(choice, [](lv_event_t*) { ++changes; }, LV_EVENT_VALUE_CHANGED, nullptr);

    Tab5Focus.focusObject(first); paint();
    const auto homeSelected = color(first), secondRest = color(second);
    check(homeSelected != secondRest, "keyboard focus visible before mouse");
    Tab5Mouse::begin(display); Tab5Mouse::service();
    auto point = center(second); move(point.x, point.y);
    const bool sticky = color(first) == homeSelected;
    const bool secondHovered = color(second) != secondRest;
    check(clicks == 0 && changes == 0, "hover does not activate");
    check(Tab5Focus.focused() == first, "hover preserves logical keyboard focus");
    if (baseline) {
        check(sticky, "baseline retains Explorer focus on mouse");
        check(!secondHovered, "baseline does not visually hover second home item");
    } else {
        check(!sticky, "mouse suppresses stale Explorer keyboard highlight");
        check(secondHovered, "mouse highlights hovered home item");
    }

    point = center(choice); press(point.x, point.y);
    check(lv_dropdown_is_open(choice), "real mouse click opens native dropdown");
    auto* list = lv_dropdown_get_list(choice);
    auto* text = lv_obj_get_child(list, 0);
    lv_area_t listArea, labelArea;
    lv_obj_get_coords(list, &listArea); lv_obj_get_coords(text, &labelArea);
    const int h = lv_font_get_line_height(lv_obj_get_style_text_font(text, LV_PART_MAIN)) +
                  lv_obj_get_style_text_line_space(text, LV_PART_MAIN);
    const int sampleX = listArea.x2 - 12;
    const auto firstBefore = pixel(sampleX, labelArea.y1 + h / 2);
    const auto thirdBefore = pixel(sampleX, labelArea.y1 + 2 * h + h / 2);
    move(listArea.x1 + 12, labelArea.y1 + 2 * h + h / 2);
    const auto firstAfter = pixel(sampleX, labelArea.y1 + h / 2);
    const auto thirdAfter = pixel(sampleX, labelArea.y1 + 2 * h + h / 2);
    check(changes == 0 && lv_dropdown_get_selected(choice) == 0,
          "hover preserves selected value and emits no VALUE_CHANGED");
    if (baseline) {
        check(thirdAfter == thirdBefore, "baseline does not hover third dropdown row");
        check(firstAfter == firstBefore, "baseline keeps original selected row");
    } else {
        check(thirdAfter != thirdBefore, "hover changes third dropdown row rendering");
        check(firstAfter == thirdBefore, "old selected row has ordinary background");
    }
    const auto beforeWide = wideInvalidations;
    move(listArea.x1 + 13, labelArea.y1 + 2 * h + h / 2);
    check(wideInvalidations == beforeWide, "same-row movement redraws cursor only, not entire list");
    auto before = flushes;
    for (unsigned n = 0; n < 50; ++n) check(!Tab5Mouse::service(), "idle has no USB work");
    paint(); check(flushes == before, "idle mouse does not invalidate screen");

    press(listArea.x1 + 13, labelArea.y1 + 2 * h + h / 2);
    check(!lv_dropdown_is_open(choice), "mouse selection closes dropdown");
    check(lv_dropdown_get_selected(choice) == 2 && changes == 1, "mouse commits hovered row exactly once");
    check(key(writing::Key::Tab, screen), "Tab resumes keyboard navigation");
    check(Tab5Focus.focused() == second, "keyboard resumes from original logical focus");
    check(lv_obj_has_state(second, LV_STATE_FOCUS_KEY), "keyboard focus highlight restored");

    // Released USB heartbeat must not steal the keyboard modality.
    writing::reports.push_back({});
    const bool heartbeatActivity = Tab5Mouse::service(); paint();
    if (!baseline) check(!heartbeatActivity, "stationary released HID heartbeat ignored");
    check(lv_obj_has_state(second, LV_STATE_FOCUS_KEY), "idle HID report keeps keyboard highlight");
    // Pointer can return to an already visited object after keyboard input.
    point = center(second); move(point.x + 1, point.y);
    if (!baseline) {
        check(lv_obj_has_state(second, LV_STATE_HOVERED), "hover returns on same button after keyboard mode");
        check(!lv_obj_has_state(second, LV_STATE_FOCUS_KEY), "pointer suppresses resumed keyboard visual");
    }

    check(key(writing::Key::Tab, screen), "keyboard leaves hovered button without moving mouse");
    move(point.x + 2, point.y);
    if (!baseline) check(lv_obj_has_state(second, LV_STATE_HOVERED),
                         "same hit object regains hover after keyboard cleared its state");

    Tab5Focus.focusObject(choice);
    check(key(writing::Key::Enter, screen), "keyboard opens dropdown");
    check(lv_dropdown_is_open(choice), "dropdown open by Enter");
    check(key(writing::Key::Up, screen), "keyboard Up accepted");
    check(lv_dropdown_get_selected(choice) == 1, "keyboard selects preceding row");
    check(key(writing::Key::Escape, screen), "keyboard Escape accepted");
    check(!lv_dropdown_is_open(choice) && lv_dropdown_get_selected(choice) == 2 && changes == 1,
          "Escape cancels candidate and retains committed value");

    // A long native list must follow the scrolled row, while the committed
    // setting remains unchanged. Use the actual OS mouse wheel route.
    std::string options;
    for (unsigned i = 0; i < 30; ++i) {
        if (i) options += "\n";
        options += "Élément " + std::to_string(i);
    }
    lv_dropdown_set_options(choice, options.c_str());
    lv_dropdown_set_selected(choice, 0);
    point = center(choice); press(point.x, point.y);
    list = lv_dropdown_get_list(choice); text = lv_obj_get_child(list, 0);
    lv_obj_set_style_max_height(list, 120, 0); lv_obj_set_height(list, 120); paint();
    lv_obj_get_coords(list, &listArea); lv_obj_get_coords(text, &labelArea);
    const int wheelX = listArea.x1 + 12, wheelY = listArea.y1 + 55;
    move(wheelX, wheelY);
    const int scrollBefore = lv_obj_get_scroll_y(list);
    move(wheelX, wheelY, 0, -1);
    check(lv_obj_get_scroll_y(list) > scrollBefore, "USB wheel scrolls long native list");
    check(changes == 1 && lv_dropdown_get_selected(choice) == 0,
          "wheel hover does not commit a setting");
    if (!baseline) check(pixel(listArea.x2 - 12, wheelY) == homeSelected,
                         "scrolled row beneath pointer is highlighted");

    // Cover only the pointer-side half. The exposed right half lets the test
    // distinguish a stale underlying highlight from the overlay's own pixels.
    auto* overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(overlay, listArea.x1, listArea.y1);
    lv_obj_set_size(overlay, 120, 120); paint();
    move(wheelX + 1, wheelY);
    if (!baseline) check(pixel(listArea.x2 - 12, wheelY) == thirdBefore,
                         "overlay prevents hover of the obscured dropdown");
    lv_obj_delete(overlay); move(wheelX + 2, wheelY);
    if (!baseline) check(pixel(listArea.x2 - 12, wheelY) == homeSelected,
                         "hover resumes when pointer overlay is removed");
    check(changes == 1 && lv_dropdown_get_selected(choice) == 0,
          "overlay and scrolling preserve committed selection");
    lv_dropdown_close(choice); paint();

    // ASan/UBSan check that pointer visual ownership cannot outlive its object.
    point = center(second); move(point.x, point.y);
    lv_obj_delete(second); move(700, 400);
    check(clicks == 0, "hover and deletion never activate buttons");
    point = center(choice); press(point.x, point.y);
    check(lv_dropdown_is_open(choice), "reopen before destruction");
    list = lv_dropdown_get_list(choice); text = lv_obj_get_child(list, 0);
    lv_obj_get_coords(list, &listArea); lv_obj_get_coords(text, &labelArea);
    move(listArea.x1 + 12, labelArea.y1 + h / 2);
    lv_obj_delete(choice); move(700, 400);
    check(changes == 1, "deleting hovered dropdown never commits a value");
    writing::connected = false;
    check(!Tab5Mouse::service(), "USB disconnect handled without fake activity"); paint();

    // A separate native pointer indev reproduces the tactile input route. The
    // touch hook must remove mouse-only HOVERED without suppressing press/click.
    auto* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch, display);
    lv_indev_set_mode(touch, LV_INDEV_MODE_EVENT);
    lv_indev_set_read_cb(touch, [](lv_indev_t*, lv_indev_data_t* data) { *data = touchData; });
#ifdef TAB5_HOVER_FIXED
    const auto eventsBefore = lv_indev_get_event_count(touch);
    Tab5UiStyle::touchInput(touch); Tab5UiStyle::touchInput(touch);
    check(lv_indev_get_event_count(touch) == eventsBefore + 1,
          "touch hook registration is idempotent");
#endif
    touchData.point = center(first); touchData.state = LV_INDEV_STATE_PRESSED;
    lv_indev_read(touch); paint();
    check(lv_obj_has_state(first, LV_STATE_PRESSED), "touch preserves native pressed visual");
    touchData.state = LV_INDEV_STATE_RELEASED; lv_indev_read(touch); paint();
    check(clicks == 1, "touch activates exactly one native click");
    check(!lv_obj_has_state(first, LV_STATE_PRESSED), "touch release clears pressed visual");
    if (!baseline) check(!lv_obj_has_state(first, LV_STATE_HOVERED),
                         "touch release leaves no persistent mouse hover");
    check(changes == 1, "touch hook never changes dropdown value");

    std::printf("{\"baseline\":%s,\"checks\":%u,\"stickyHomeFocus\":%s,\"homeHover\":%s,"
                "\"firstBefore\":%u,\"firstAfter\":%u,\"thirdBefore\":%u,\"thirdAfter\":%u,"
                "\"changes\":%u,\"clicks\":%u,\"passed\":true}\n",
                baseline ? "true" : "false", checks, sticky ? "true" : "false",
                secondHovered ? "true" : "false", firstBefore, firstAfter, thirdBefore,
                thirdAfter, changes, clicks);
    Tab5Focus.clear(); lv_obj_clean(lv_layer_top()); lv_obj_clean(screen); lv_deinit();
    return 0;
}
