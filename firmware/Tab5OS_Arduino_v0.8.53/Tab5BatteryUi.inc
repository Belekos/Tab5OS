// Included by the sketch after its UI helpers. UI task only; no blocking waits.
static lv_obj_t* batteryGuardBanner = nullptr;
static lv_obj_t* batteryGuardMessage = nullptr;
static bool batteryGuardCycle = false;

static void closeBatteryProtectionNotice() {
  if (batteryGuardBanner) lv_obj_delete(batteryGuardBanner);
  batteryGuardBanner = batteryGuardMessage = nullptr;
}

static void showBatteryProtectionNotice(uint32_t seconds) {
  if (!batteryGuardBanner) {
    batteryGuardBanner = lv_obj_create(lv_layer_top());
    lv_obj_remove_flag(batteryGuardBanner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(batteryGuardBanner, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(batteryGuardBanner, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(batteryGuardBanner, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(batteryGuardBanner, 12, 0);
    lv_obj_set_style_pad_row(batteryGuardBanner, 4, 0);
    lv_obj_set_style_border_width(batteryGuardBanner, 2, 0);
    lv_obj_set_style_radius(batteryGuardBanner, 10, 0);
    batteryGuardMessage = lv_label_create(batteryGuardBanner);
    lv_obj_set_width(batteryGuardMessage, LV_PCT(100));
    lv_label_set_long_mode(batteryGuardMessage, LV_LABEL_LONG_WRAP);
  }
  const int width = lv_display_get_horizontal_resolution(lv_display_get_default());
  lv_obj_set_width(batteryGuardBanner, std::min(980, std::max(1, width - 24)));
  lv_obj_align(batteryGuardBanner, LV_ALIGN_TOP_MID, 0, 64);
  lv_obj_set_style_bg_color(batteryGuardBanner, C_RAISED(), 0);
  lv_obj_set_style_bg_opa(batteryGuardBanner, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(batteryGuardBanner, C_ACTIVE(), 0);
  lv_obj_set_style_text_color(batteryGuardMessage, C_TEXT(), 0);
  lv_obj_set_style_text_font(batteryGuardMessage, richFontFor(20, 0), 0);
  char text[512];
  snprintf(text, sizeof(text), T("Batterie critique : arrêt dans %lu s. Branchez la tablette et enregistrez votre travail."), static_cast<unsigned long>(seconds));
  setLabelIfChanged(batteryGuardMessage, text);
  lv_obj_move_foreground(batteryGuardBanner);
}

static void serviceBatteryProtection(uint32_t now) {
  const auto state = Tab5BatteryGuard::service(now);
  using Event = Tab5BatteryGuard::Event;
  if (state.event == Event::None) return;
  if (state.event == Event::Low) {
    screenIdle.wake(now);
    toast("Batterie faible : branchez la tablette et enregistrez votre travail.", 8000);
    return;
  }
  if (state.event == Event::Cancelled) {
    if (batteryGuardCycle) {
      closeBatteryProtectionNotice();
      toast("Arrêt automatique annulé", 3000);
    }
    batteryGuardCycle = false;
    return;
  }
  if (!batteryGuardCycle) {
    batteryGuardCycle = true;
    // Cancellation flags only: let active workers release their own resources.
    // KeyStore has no nonblocking cancel API; never call its lock from here.
    Tab5Firmware::cancel();
    Tab5GitView::cancel();
    Tab5Terminal::stop();
    Tab5Vault::cancel();
    Tab5FileOperation::cancel();
  }
  screenIdle.wake(now);
  showBatteryProtectionNotice(state.seconds);
  if (state.event != Event::Shutdown) return;
  // Power protection does not save documents or change the autosave setting.
  if (!Tab5BatteryGuard::powerOffIfStillCritical(now)) {
    batteryGuardCycle = false;
    closeBatteryProtectionNotice();
    toast("Arrêt automatique annulé", 3000);
  }
}
