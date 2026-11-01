#include "Tab5Focus.h"
#include "Tab5TextInput.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
namespace {
struct Description {
  std::string text;
};
// A callback can delete a list (or reparent its row before deleting it).
// lv_obj_is_valid is a tree-membership check in LVGL 9.6: it dereferences
// its argument and cannot validate a pointer after the object was freed.
class LiveObject {
 public:
  explicit LiveObject(lv_obj_t* object) : object_(object) {
    if (object_ && !lv_obj_add_event_cb(object_, deleted, LV_EVENT_DELETE, this))
      object_ = nullptr;
  }
  ~LiveObject() {
    if (object_) lv_obj_remove_event_cb_with_user_data(object_, deleted, this);
  }
  lv_obj_t* get() const { return object_; }
 private:
  static void deleted(lv_event_t* event) {
    auto* self = static_cast<LiveObject*>(lv_event_get_user_data(event));
    if (lv_event_get_target_obj(event) == self->object_) self->object_ = nullptr;
  }
  lv_obj_t* object_;
};
void descriptionEvent(lv_event_t* e) {
  auto* d = static_cast<Description*>(lv_event_get_user_data(e));
  if (lv_event_get_code(e) == LV_EVENT_DELETE) delete d;
}
bool visible(lv_obj_t* o) {
  if (!o || !lv_obj_is_valid(o)) return false;
  if (lv_obj_get_screen(o) != lv_screen_active() && lv_obj_get_screen(o) != lv_layer_top())
    return false;
  while (o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return false;
    o = lv_obj_get_parent(o);
  }
  return true;
}
void collect(lv_obj_t* o, std::vector<lv_obj_t*>& out, lv_obj_t* exclude) {
  if (!o || o == exclude || !visible(o) || lv_obj_has_state(o, LV_STATE_DISABLED)) return;
  bool control =
      lv_obj_has_flag(o, LV_OBJ_FLAG_USER_1) || lv_obj_has_class(o, &lv_button_class) ||
      lv_obj_check_type(o, &lv_textarea_class) || lv_obj_check_type(o, &lv_dropdown_class) ||
      lv_obj_check_type(o, &lv_switch_class) || lv_obj_check_type(o, &lv_checkbox_class) ||
      lv_obj_check_type(o, &lv_slider_class) || lv_obj_check_type(o, &lv_roller_class);
  if (control && (lv_obj_has_flag(o,LV_OBJ_FLAG_CLICKABLE)||lv_obj_has_flag(o,LV_OBJ_FLAG_USER_1))) {
    out.push_back(o);
    return;
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i)
    collect(lv_obj_get_child(o, i), out, exclude);
}
}  // namespace
Tab5FocusClass Tab5Focus;
bool Tab5FocusClass::contains(lv_obj_t* scope, lv_obj_t* o) const {
  if (!o || !lv_obj_is_valid(o)) return false;
  for (; o; o = lv_obj_get_parent(o))
    if (o == scope) return true;
  return false;
}
void Tab5FocusClass::deleted(lv_event_t* e) {
  auto* self = static_cast<Tab5FocusClass*>(lv_event_get_user_data(e));
  auto* o = lv_event_get_target_obj(e);
  if (self->tip_ == o) self->tip_ = nullptr;
  if (self->current_ == o) {
    self->current_ = nullptr;
    self->controlEditing_ = false;
    if (self->tip_) lv_obj_add_flag(self->tip_, LV_OBJ_FLAG_HIDDEN);
  }
  for (auto& m : self->memories_) {
    if (m.scope == o) m.scope = nullptr;
    if (m.object == o) m.object = nullptr;
  }
  self->memories_.erase(std::remove_if(self->memories_.begin(), self->memories_.end(),
                                       [](const Memory& m) { return !m.scope; }),
                        self->memories_.end());
}
void Tab5FocusClass::watch(lv_obj_t* o) {
  for (uint32_t i = 0; i < lv_obj_get_event_count(o); ++i)
    if (lv_event_dsc_get_cb(lv_obj_get_event_dsc(o, i)) == deleted) return;
  lv_obj_add_event_cb(o, deleted, LV_EVENT_DELETE, this);
}
void Tab5FocusClass::remember(lv_obj_t* scope) {
  auto* o = focused();
  if (!scope || !contains(scope, o)) return;
  watch(scope);
  watch(o);
  for (auto& m : memories_)
    if (m.scope == scope) {
      m.object = o;
      return;
    }
  memories_.push_back({scope, o});
}
void Tab5FocusClass::restore(lv_obj_t* scope) {
  for (auto& m : memories_)
    if (m.scope == scope && m.object && visible(m.object) && contains(scope, m.object) &&
        !lv_obj_has_state(m.object, LV_STATE_DISABLED)) {
      focus(m.object);
      return;
    }
  focusFirst(scope);
}
void Tab5FocusClass::activation(lv_obj_t* o, lv_event_cb_t cb, void* data) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_USER_3);
  lv_obj_add_event_cb(o, cb, LV_EVENT_READY, data);
}
void Tab5FocusClass::focusObject(lv_obj_t* o) {
  if (visible(o)) focus(o);
}

void Tab5FocusClass::editObject(lv_obj_t* o) {
  if (visible(o) && lv_obj_check_type(o, &lv_textarea_class) && !lv_obj_has_state(o, LV_STATE_DISABLED)) {
    focus(o);
    controlEditing_ = true;
  }
}

void Tab5FocusClass::begin(lv_obj_t* screen, const lv_font_t* font) {
  if (tipTimer_) lv_timer_delete(tipTimer_);
  tipTimer_ = lv_timer_create(tipTimer, 800, this);
  lv_timer_pause(tipTimer_);
  if (tip_ && lv_obj_is_valid(tip_)) lv_obj_delete(tip_);
  tip_ = lv_label_create(screen);
  watch(tip_);
  lv_obj_set_style_text_font(tip_, font, 0);
  lv_obj_set_style_text_color(tip_, text_, 0);
  lv_obj_set_style_bg_color(tip_, surface_, 0);
  lv_obj_set_style_bg_opa(tip_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(tip_, 5, 0);
  lv_obj_set_style_radius(tip_, 7, 0);
  lv_obj_set_width(tip_, LV_SIZE_CONTENT);
  lv_obj_set_style_max_width(tip_, 360, 0);
  lv_obj_set_style_max_height(tip_, 58, 0);
  lv_label_set_long_mode(tip_, LV_LABEL_LONG_DOT);
  lv_obj_add_flag(tip_, LV_OBJ_FLAG_HIDDEN);
}
void Tab5FocusClass::theme(lv_color_t selected, lv_color_t text, lv_color_t surface) {
  selected_ = selected;
  text_ = text;
  surface_ = surface;
  if (tip_ && lv_obj_is_valid(tip_)) {
    lv_obj_set_style_text_color(tip_, text_, 0);
    lv_obj_set_style_bg_color(tip_, surface_, 0);
  }
  if (current_ && lv_obj_is_valid(current_)) {
    lv_obj_remove_state(current_, static_cast<lv_state_t>(LV_STATE_FOCUS_KEY | LV_STATE_FOCUSED));
    applyFocusStyle(current_);
    lv_obj_add_state(current_, keyboardHighlight_ ? static_cast<lv_state_t>(LV_STATE_FOCUS_KEY | LV_STATE_FOCUSED) : LV_STATE_FOCUSED);
  }
}
void Tab5FocusClass::applyFocusStyle(lv_obj_t* o) {
  const auto selector = static_cast<lv_style_selector_t>(
      LV_PART_MAIN | LV_STATE_FOCUS_KEY | LV_STATE_FOCUSED);
  const bool button = lv_obj_has_class(o, &lv_button_class);
  // Read the *unfocused* style. In particular, preserve a RichView's transparent
  // background and a textarea's foreground. Making a transparent object's
  // unused default background opaque can hide near-white document text.
  const lv_color_t bg = lv_obj_get_style_bg_color(o, LV_PART_MAIN);
  const lv_color_t fg = lv_obj_get_style_text_color(o, LV_PART_MAIN);
  const lv_opa_t opacity = lv_obj_get_style_bg_opa(o, LV_PART_MAIN);
  lv_obj_set_style_outline_width(o, 0, selector);
  lv_obj_set_style_shadow_width(o, 0, selector);
  lv_obj_set_style_bg_color(o, button ? selected_ : bg, selector);
  lv_obj_set_style_bg_opa(o, button ? LV_OPA_COVER : opacity, selector);
  lv_obj_set_style_text_color(o, button ? text_ : fg, selector);
}
void Tab5FocusClass::describe(lv_obj_t* o, const char* text) {
  // Lists can contain thousands of rows. Do not retain a heap allocation per
  // row for obvious instructions that will never be displayed.
  const bool useful = text && (strstr(text, "Ctrl") || strstr(text, "F1") || strstr(text, "F6"));
  for (uint32_t i = 0; i < lv_obj_get_event_count(o); ++i) {
    auto* e = lv_obj_get_event_dsc(o, i);
    if (lv_event_dsc_get_cb(e) == descriptionEvent) {
      auto& hint = static_cast<Description*>(lv_event_dsc_get_user_data(e))->text;
      if (useful) hint = text; else std::string().swap(hint);
      return;
    }
  }
  if (!useful) return;
  auto* d = new Description{text};
  lv_obj_add_event_cb(o, descriptionEvent, LV_EVENT_ALL, d);
}
lv_obj_t* Tab5FocusClass::focused() const {
  return current_ && lv_obj_is_valid(current_) && visible(current_) ? current_ : nullptr;
}
void Tab5FocusClass::clear() {
  if (current_ && lv_obj_is_valid(current_)) {
    lv_obj_remove_state(current_, static_cast<lv_state_t>(LV_STATE_FOCUS_KEY | LV_STATE_FOCUSED));
    lv_obj_set_style_outline_width(current_, 0, LV_STATE_FOCUS_KEY);
  }
  if (tipTimer_) lv_timer_pause(tipTimer_);
  current_ = nullptr;
  controlEditing_ = false;
  if (tip_ && lv_obj_is_valid(tip_)) lv_obj_add_flag(tip_, LV_OBJ_FLAG_HIDDEN);
}
void Tab5FocusClass::pointerMode() {
  if (!keyboardHighlight_) return;
  keyboardHighlight_ = false;
  if (current_) lv_obj_remove_state(current_, LV_STATE_FOCUS_KEY);
  if (tipTimer_) lv_timer_pause(tipTimer_);
  if (tip_) lv_obj_set_hidden(tip_, true);
}
void Tab5FocusClass::keyboardMode() {
  if (keyboardHighlight_) return;
  keyboardHighlight_ = true;
  if (current_) lv_obj_add_state(current_, LV_STATE_FOCUS_KEY);
}
void Tab5FocusClass::focus(lv_obj_t* o) {
  if (o == focused()) return;
  clear();
  current_ = o;
  if (!o) return;
  watch(o);
  applyFocusStyle(o);
  lv_obj_add_state(o, keyboardHighlight_ ? static_cast<lv_state_t>(LV_STATE_FOCUS_KEY | LV_STATE_FOCUSED) : LV_STATE_FOCUSED);
  lv_obj_scroll_to_view_recursive(o, LV_ANIM_OFF);
  if (lv_obj_send_event(o, LV_EVENT_FOCUSED, nullptr) == LV_RESULT_INVALID) return;
  if (inputChanged_ && current_ == o && lv_obj_check_type(o, &lv_textarea_class) &&
      !Tab5TextInput::readOnly(o)) inputChanged_(o);
  if (current_ != o || !lv_obj_is_valid(o)) return;
  const char* text = "";
  for (uint32_t i = 0; i < lv_obj_get_event_count(o); ++i) {
    auto* d = lv_obj_get_event_dsc(o, i);
    if (lv_event_dsc_get_cb(d) == descriptionEvent) {
      text = static_cast<Description*>(lv_event_dsc_get_user_data(d))->text.c_str();
    }
  }
  // Only a non-obvious shortcut earns a compact, delayed tip. Repeated
  // Enter/Tab/arrows instructions and copies of captions belong in F1 help.
  std::string hint = text;
  size_t shortcut = hint.find("Ctrl");
  if (shortcut == std::string::npos) shortcut = hint.find("F6");
  if (shortcut == std::string::npos) shortcut = hint.find("F1");
  if (tip_ && tipTimer_ && shortcut != std::string::npos) {
    hint = hint.substr(shortcut, 54);
    lv_label_set_text(tip_, hint.c_str());
    lv_timer_set_period(tipTimer_, 800);
    lv_timer_reset(tipTimer_);
    lv_timer_resume(tipTimer_);
  }
}
void Tab5FocusClass::tipTimer(lv_timer_t* timer) {
  auto* self = static_cast<Tab5FocusClass*>(lv_timer_get_user_data(timer));
  if (!self->tip_ || !lv_obj_is_valid(self->tip_) || !self->focused()) {
    lv_timer_pause(timer);
    return;
  }
  if (!lv_obj_has_flag(self->tip_, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_add_flag(self->tip_, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(timer);
    return;
  }
  lv_obj_remove_flag(self->tip_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(self->tip_, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
  lv_obj_move_foreground(self->tip_);
  lv_timer_set_period(timer, 2000);
}

void Tab5FocusClass::focusFirst(lv_obj_t* scope) {
  std::vector<lv_obj_t*> list;
  collect(scope, list, nullptr);
  focus(list.empty() ? nullptr : list.front());
}
void Tab5FocusClass::focusInitial(lv_obj_t* scope, lv_obj_t* primary) {
  std::vector<lv_obj_t*> list;
  collect(scope, list, nullptr);
  if (!primary || std::find(list.begin(), list.end(), primary) == list.end()) {
    primary = nullptr;
    for (auto* item : list)
      if (lv_obj_check_type(item, &lv_textarea_class) && !Tab5TextInput::readOnly(item)) { primary = item; break; }
    if (!primary && !list.empty()) primary = list.front();
  }
  focus(primary);
  // FOCUSED/inputChanged callbacks may destroy the initial control and open
  // another dialog. Only update the control that still owns focus.
  if (current_ == primary)
    controlEditing_ = primary && lv_obj_check_type(primary, &lv_textarea_class) && !Tab5TextInput::readOnly(primary);
}
bool Tab5FocusClass::edit(lv_obj_t* o, const writing::KeyEvent& e) {
  return Tab5TextInput::key(o,e);
}

bool Tab5FocusClass::key(const writing::KeyEvent& e, lv_obj_t* scope, lv_obj_t* exclude,
                         lv_obj_t* neighbor) {
  keyboardMode();
  using K = writing::Key;
  std::vector<lv_obj_t*> list;
  collect(scope, list, exclude);
  if (neighbor && (e.key == K::Left || e.key == K::Right)) collect(neighbor, list, exclude);
  auto* current = focused();
  auto it = std::find(list.begin(), list.end(), current);
  if(current&&contains(scope,current)&&lv_obj_check_type(current,&lv_label_class)&&Tab5TextInput::key(current,e))return true;
  if (it == list.end()) current = nullptr;
  if (current && lv_obj_check_type(current, &lv_textarea_class) && controlEditing_ &&
      e.key != K::Tab) {
    if (e.key == K::Escape) {
      controlEditing_ = false;
      return true;
    }
    // READY may close this dialog and focus a different input. Finish the
    // old editing session before dispatch; never inspect its freed textarea
    // or overwrite the new dialog's editing state afterwards.
    if (e.key == K::Enter && lv_textarea_get_one_line(current)) controlEditing_ = false;
    return edit(current, e);
  }
  if (current && controlEditing_ && lv_obj_has_flag(current, LV_OBJ_FLAG_USER_2) &&
      e.key == K::Text && !e.ctrl && !e.meta) {
    uint32_t key = static_cast<unsigned char>(e.text[0]);
    lv_obj_send_event(current, LV_EVENT_KEY, &key);
    return true;
  }
  bool arrows = e.key == K::Left || e.key == K::Right || e.key == K::Up || e.key == K::Down;
  if (current && controlEditing_ &&
      (arrows || e.key == K::Enter || e.key == K::Escape || e.key == K::Home || e.key == K::End ||
       e.key == K::PageUp || e.key == K::PageDown)) {
    uint32_t key = e.key == K::Left       ? LV_KEY_LEFT
                   : e.key == K::Right    ? LV_KEY_RIGHT
                   : e.key == K::Up       ? LV_KEY_UP
                   : e.key == K::Down     ? LV_KEY_DOWN
                   : e.key == K::Home     ? LV_KEY_HOME
                   : e.key == K::End      ? LV_KEY_END
                   : e.key == K::PageUp   ? LV_KEY_PREV
                   : e.key == K::PageDown ? LV_KEY_NEXT
                   : e.key == K::Enter    ? LV_KEY_ENTER
                                          : LV_KEY_ESC;
    lv_obj_send_event(current, LV_EVENT_KEY, &key);
    if (e.key == K::Escape || e.key == K::Enter) controlEditing_ = false;
    return true;
  }
  if (e.key == K::Tab || arrows) {
    if (list.empty()) return false;
    if (e.key == K::Tab || !current) {
      int step = e.shift ? -1 : 1;
      int index = current ? static_cast<int>(it - list.begin()) : step > 0 ? -1 : 0;
      index = (index + step + list.size()) % list.size();
      focus(list[index]);
      controlEditing_ = current_ && lv_obj_check_type(current_, &lv_textarea_class) &&
                        !Tab5TextInput::readOnly(current_);
      remember(scope);
      return true;
    }
    lv_obj_update_layout(scope);
    auto bounds = [](lv_obj_t* o) {
      lv_area_t a;
      lv_obj_get_coords(o, &a);
      // A document can be taller than its viewport. Navigate to its visible area.
      if (lv_obj_has_flag(o, LV_OBJ_FLAG_USER_1))
        for (auto* p = lv_obj_get_parent(o); p; p = lv_obj_get_parent(p)) {
          lv_area_t c;
          lv_obj_get_coords(p, &c);
          a.x1 = std::max(a.x1, c.x1);
          a.x2 = std::min(a.x2, c.x2);
          a.y1 = std::max(a.y1, c.y1);
          a.y2 = std::min(a.y2, c.y2);
        }
      return a;
    };
    auto a = bounds(current);
    bool horizontal = e.key == K::Left || e.key == K::Right;
    int sign = e.key == K::Left || e.key == K::Up ? -1 : 1;
    long long best = std::numeric_limits<long long>::max();
    lv_obj_t* target = nullptr;
    for (auto* candidate : list) {
      if (candidate == current) continue;
      auto b = bounds(candidate);
      int primary = horizontal ? (b.x1 + b.x2 - a.x1 - a.x2) : (b.y1 + b.y2 - a.y1 - a.y2);
      if (primary * sign <= 1) continue;
      int secondary =
          horizontal ? std::abs(b.y1 + b.y2 - a.y1 - a.y2) : std::abs(b.x1 + b.x2 - a.x1 - a.x2);
      bool beam = horizontal ? b.y1 <= a.y2 && b.y2 >= a.y1 : b.x1 <= a.x2 && b.x2 >= a.x1;
      long long score = (beam ? 0 : 1000000000LL) + std::abs(primary) * 4LL + secondary;
      if (score < best) {
        best = score;
        target = candidate;
      }
    }
    if (!target) return false;  // The caller can cross from the app to the dock.
    remember(scope);
    focus(target);
    remember(scope);
    return true;
  }
  if (current && (e.key == K::Enter || (e.key == K::Text && !strcmp(e.text, " ") &&
       !e.ctrl && !e.alt && !e.meta && !lv_obj_check_type(current, &lv_textarea_class)))) {
    const bool wasList = lv_obj_has_class(current, &lv_list_button_class);
    LiveObject previousParent(wasList ? lv_obj_get_parent(current) : nullptr);
    const auto finish = [&]() {
      if (wasList && !focused() && previousParent.get() && visible(previousParent.get()))
        focusFirst(previousParent.get());
      return true;
    };
    if (lv_obj_has_flag(current, LV_OBJ_FLAG_USER_3)) {
      if (lv_obj_send_event(current, LV_EVENT_READY, nullptr) == LV_RESULT_INVALID) return finish();
    } else if (lv_obj_has_flag(current, LV_OBJ_FLAG_USER_2)) {
      controlEditing_ = true;
      uint32_t key = LV_KEY_ENTER;
      if (lv_obj_send_event(current, LV_EVENT_KEY, &key) == LV_RESULT_INVALID) return finish();
    } else if (lv_obj_check_type(current, &lv_textarea_class)) {
      controlEditing_ = true;
      if (lv_obj_send_event(current, LV_EVENT_CLICKED, nullptr) == LV_RESULT_INVALID) return finish();
    } else if (lv_obj_check_type(current, &lv_dropdown_class)) {
      controlEditing_ = true;
      lv_dropdown_open(current);
    } else if (lv_obj_check_type(current, &lv_slider_class) ||
               lv_obj_check_type(current, &lv_roller_class))
      controlEditing_ = true;
    else {
      if (lv_obj_has_flag(current, LV_OBJ_FLAG_CHECKABLE) || lv_obj_check_type(current, &lv_switch_class) ||
          lv_obj_check_type(current, &lv_checkbox_class)) {
        if (lv_obj_has_state(current, LV_STATE_CHECKED))
          lv_obj_remove_state(current, LV_STATE_CHECKED);
        else
          lv_obj_add_state(current, LV_STATE_CHECKED);
        if (lv_obj_send_event(current, LV_EVENT_VALUE_CHANGED, nullptr) == LV_RESULT_INVALID)
          return finish();
      }
      if (lv_obj_send_event(current, LV_EVENT_SINGLE_CLICKED, nullptr) == LV_RESULT_INVALID)
        return finish();
      if (lv_obj_send_event(current, LV_EVENT_CLICKED, nullptr) == LV_RESULT_INVALID)
        return finish();
    }
    return finish();
  }
  return false;
}
