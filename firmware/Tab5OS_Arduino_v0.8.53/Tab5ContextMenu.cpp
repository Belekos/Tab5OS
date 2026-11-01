#include "Tab5ContextMenu.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5UiStyle.h"
#include <algorithm>
#include <cstring>
#include <new>

namespace Tab5ContextMenu {
namespace {
lv_obj_t *overlay = nullptr, *panel = nullptr, *owner = nullptr;
Item actions[Capacity]{};
size_t used = 0;
int pending = -1;
uint32_t generation = 0;
const lv_font_t* (*fontHook)() = nullptr;
const char* (*translateHook)(const char*) = nullptr;
const char* tr(const char* s) { return translateHook ? translateHook(s) : s; }
lv_color_t color(unsigned role) { return lv_color_hex(Tab5UiStyle::color(role)); }
void ownerDeleted(lv_event_t* e) {
  if (lv_event_get_target_obj(e) == owner) { owner = nullptr; pending = -2; ++generation; }
}
void observe(lv_obj_t* object) {
  for (uint32_t i = 0; i < lv_obj_get_event_count(object); ++i)
    if (lv_event_dsc_get_cb(lv_obj_get_event_dsc(object, i)) == ownerDeleted) return;
  lv_obj_add_event_cb(object, ownerDeleted, LV_EVENT_DELETE, nullptr);
}
void choose(lv_event_t* e) {
  const size_t i = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  if (i < used && actions[i].enabled) pending = int(i);
}
void dismiss(bool restore) {
  auto* old = overlay;
  overlay = panel = nullptr;
  if (old) Tab5Focus.clear();
  if (old) lv_obj_delete(old);
  // ownerDeleted remains active throughout LVGL destruction and focus events.
  if (restore && owner && lv_obj_is_valid(owner)) Tab5Focus.restore(owner);
}
struct OutputBinding { void (*clear)(void*); void* user; };
void outputDeleted(lv_event_t* e) {
  delete static_cast<OutputBinding*>(lv_event_get_user_data(e));
}
OutputBinding* binding(lv_obj_t* object) {
  if (!object || !lv_obj_is_valid(object)) return nullptr;
  for (uint32_t i = 0; i < lv_obj_get_event_count(object); ++i) {
    auto* event = lv_obj_get_event_dsc(object, i);
    if (lv_event_dsc_get_cb(event) == outputDeleted)
      return static_cast<OutputBinding*>(lv_event_dsc_get_user_data(event));
  }
  return nullptr;
}
void edit(void* value) {
  const uintptr_t code = reinterpret_cast<uintptr_t>(value);
  auto* target = owner;
  if (!target || !lv_obj_is_valid(target)) return;
  if (code == 1) { clearOutput(target); return; }
  writing::KeyEvent e;
  e.key = writing::Key::Text; e.ctrl = true; e.text[0] = char(code); e.text[1] = 0;
  Tab5Focus.focusObject(target);
  Tab5TextInput::key(target, e);
}
}
void configure(const lv_font_t* (*font)(), const char* (*translate)(const char*)) {
  fontHook = font; translateHook = translate;
}
bool show(lv_obj_t* target, int x, int y, const Item* items, size_t count) {
  close();
  if (!target || !lv_obj_is_valid(target) || !items || !count || count > Capacity) return false;
  ++generation; owner = target; observe(owner); Tab5Focus.remember(owner);
  std::copy(items, items + count, actions); used = count; pending = -1;
  overlay = lv_obj_create(lv_layer_top());
  if (!overlay) { owner = nullptr; used = 0; return false; }
  lv_obj_remove_style_all(overlay);
  lv_obj_set_size(overlay, lv_pct(100), lv_pct(100));
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(overlay, [](lv_event_t* e) {
    if (lv_event_get_target_obj(e) == overlay) pending = -2;
  }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(overlay, [](lv_event_t* e) {
    if (lv_event_get_target_obj(e) == overlay) {
      overlay = panel = nullptr; owner = nullptr; used = 0; pending = -1;
    }
  }, LV_EVENT_DELETE, nullptr);
  panel = lv_obj_create(overlay);
  if (!panel) { close(); return false; }
  const int sw = lv_display_get_horizontal_resolution(nullptr);
  const int sh = lv_display_get_vertical_resolution(nullptr);
  const int width = std::min(430, std::max(120, sw - 16));
  const int height = std::min(int(count) * 46 + 12, std::max(60, sh - 16));
  lv_obj_set_size(panel, width, height);
  lv_obj_set_pos(panel, std::clamp(x, 8, std::max(8, sw - width - 8)),
                       std::clamp(y, 8, std::max(8, sh - height - 8)));
  lv_obj_set_style_pad_all(panel, 5, 0);
  lv_obj_set_style_pad_row(panel, 2, 0);
  lv_obj_set_style_bg_color(panel, color(1), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, color(3), 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_radius(panel, Tab5UiStyle::radius(), 0);
  lv_obj_set_scroll_dir(panel, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  if (fontHook) lv_obj_set_style_text_font(panel, fontHook(), 0);
  for (size_t i = 0; i < count; ++i) {
    auto* button = lv_button_create(panel);
    if (!button) { close(); return false; }
    lv_obj_set_size(button, lv_pct(100), 44);
    lv_obj_set_style_pad_all(button, 6, 0);
    lv_obj_set_style_bg_color(button, color(1), 0);
    lv_obj_set_style_bg_color(button, color(8), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, color(4), 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    Tab5UiStyle::selection(button);
    if (!items[i].enabled || !items[i].action) lv_obj_add_state(button, LV_STATE_DISABLED);
    auto* label = lv_label_create(button);
    if (!label) { close(); return false; }
    lv_label_set_text(label, tr(items[i].title));
    const bool shortcut = items[i].shortcut && *items[i].shortcut;
    lv_obj_set_width(label, width - (shortcut ? 156 : 36));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    if (shortcut) {
      auto* keys = lv_label_create(button);
      if (!keys) { close(); return false; }
      lv_label_set_text(keys, tr(items[i].shortcut));
      lv_obj_set_style_text_color(keys, color(5), 0);
      lv_obj_set_width(keys, 120);
      lv_obj_set_style_text_align(keys, LV_TEXT_ALIGN_RIGHT, 0);
      lv_label_set_long_mode(keys, LV_LABEL_LONG_DOT);
      lv_obj_align(keys, LV_ALIGN_RIGHT_MID, 0, 0);
      lv_obj_remove_flag(keys, LV_OBJ_FLAG_CLICKABLE);
    }
    Tab5Focus.describe(button, tr(items[i].title));
    lv_obj_add_event_cb(button, choose, LV_EVENT_CLICKED, reinterpret_cast<void*>(uintptr_t(i)));
  }
  lv_obj_update_layout(overlay);
  Tab5Focus.focusInitial(panel);
  return true;
}
void close() { ++generation; dismiss(true); owner = nullptr; used = 0; pending = -1; }
void service() {
  if (!overlay) return;
  if (!owner || !lv_obj_is_valid(owner)) { close(); return; }
  if (pending == -1) return;
  const int selected = pending; pending = -1;
  const uint32_t before = generation;
  Item item{};
  if (selected >= 0 && size_t(selected) < used) item = actions[selected];
  dismiss(false);
  if (owner && lv_obj_is_valid(owner)) {
    Tab5Focus.restore(owner);
    if (owner && generation == before && item.action && item.enabled) item.action(item.user);
  }
  // The callback may open a replacement menu. Preserve that new owner/state.
  if (!overlay) { owner = nullptr; used = 0; pending = -1; }
}
bool key(const writing::KeyEvent& event) {
  if (!overlay) return false;
  using K = writing::Key;
  if (event.key == K::Escape || event.key == K::Backspace) { pending = -2; return true; }
  for (size_t i = 0; i < used; ++i) {
    const char* shortcut = actions[i].shortcut;
    if (!shortcut || !*shortcut || !actions[i].enabled) continue;
    // Both standard redo chords select the same deferred menu action.
    if (!strcmp(shortcut,"Ctrl+Y") && (event.ctrl||event.meta) && event.shift && !event.alt && event.key==K::Text && (event.text[0]=='z'||event.text[0]=='Z')) { pending=int(i); return true; }
    bool ctrl = false, shift = false, alt = false;
    for (;;) {
      if (!strncmp(shortcut, "Ctrl+", 5)) { ctrl = true; shortcut += 5; }
      else if (!strncmp(shortcut, "Maj+", 4)) { shift = true; shortcut += 4; }
      else if (!strncmp(shortcut, "Shift+", 6)) { shift = true; shortcut += 6; }
      else if (!strncmp(shortcut, "Alt+", 4)) { alt = true; shortcut += 4; }
      else break;
    }
    if (ctrl != bool(event.ctrl || event.meta) || shift != event.shift || alt != event.alt) continue;
    const char c = event.text[0] >= 'a' && event.text[0] <= 'z' ? event.text[0] - 32 : event.text[0];
    bool match = event.key == K::Text && shortcut[0] && !shortcut[1] && c == shortcut[0];
    match |= event.key == K::Delete && (!strcmp(shortcut, "Suppr") || !strcmp(shortcut, "Delete"));
    match |= event.key == K::F5 && !strcmp(shortcut, "F5");
    match |= event.key == K::F2 && !strcmp(shortcut, "F2");
    if (match) { pending = int(i); return true; }
  }
  Tab5Focus.key(event, panel); return true;
}
lv_obj_t* scope() { return overlay; }
bool bindOutput(lv_obj_t* output, void (*clear)(void*), void* user) {
  if (!output || !clear || !lv_obj_is_valid(output)) return false;
  if (auto* b = binding(output)) { b->clear = clear; b->user = user; return true; }
  auto* b = new(std::nothrow) OutputBinding{clear, user};
  if (!b) return false;
  lv_obj_add_event_cb(output, outputDeleted, LV_EVENT_DELETE, b); return true;
}
bool clearOutput(lv_obj_t* output) {
  auto* b = binding(output); if (!b) return false;
  const auto action = b->clear; void* user = b->user;
  // Clear selection before modifying the backing model so its next refresh is
  // not held by an invisible stale selection.
  auto* label = lv_obj_check_type(output, &lv_textarea_class) ? lv_textarea_get_label(output) : output;
  if (lv_obj_check_type(label, &lv_label_class)) {
    lv_label_set_text_selection_start(label, LV_DRAW_LABEL_NO_TXT_SEL);
    lv_label_set_text_selection_end(label, LV_DRAW_LABEL_NO_TXT_SEL);
  }
  action(user); return true;
}
bool text(lv_obj_t* hit, int x, int y) {
  if (!hit || !lv_obj_is_valid(hit)) return false;
  lv_obj_t* target = hit;
  // The label inside a textarea is hit first; edits belong to its textarea.
  for (auto* at = hit; at; at = lv_obj_get_parent(at))
    if (lv_obj_check_type(at, &lv_textarea_class)) { target = at; break; }
  const bool field = lv_obj_check_type(target, &lv_textarea_class);
  const bool label = lv_obj_check_type(target, &lv_label_class) && lv_obj_has_flag(target, LV_OBJ_FLAG_USER_1);
  if ((!field && !label) || lv_obj_has_state(target, LV_STATE_DISABLED)) return false;
  const bool writable = field && !Tab5TextInput::readOnly(target);
  const bool password = field && (lv_textarea_get_password_mode(target) || Tab5TextInput::protectedField(target));
  const bool selection = Tab5TextInput::hasSelection(target);
  const char* value = field ? lv_textarea_get_text(target) : lv_label_get_text(target);
  Item items[9]; size_t count = 0;
  auto add = [&](const char* title, const char* keys, uintptr_t code, bool enabled) {
    items[count++] = {title, keys, edit, reinterpret_cast<void*>(code), enabled};
  };
  if (writable && !password) {
    add("Annuler", "Ctrl+Z", 'z', Tab5TextInput::canUndo(target));
    add("Rétablir", "Ctrl+Y", 'y', Tab5TextInput::canRedo(target));
    add("Couper", "Ctrl+X", 'x', selection);
  }
  if (!password) add("Copier", "Ctrl+C", 'c', selection);
  if (writable) add("Coller", "Ctrl+V", 'v', *Tab5TextInput::clipboard());
  add("Tout sélectionner", "Ctrl+A", 'a', value && *value);
  if (binding(target)) add("Effacer la sortie", "Ctrl+L", 1, value && *value);
  return show(target, x, y, items, count);
}
}
