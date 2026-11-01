// UI only. Enumeration and reports are serviced by the global USB tasks.
static lv_obj_t* settingsPadLabel=nullptr;
static lv_obj_t* settingsPadConfigure=nullptr;
static lv_obj_t* settingsPadCancel=nullptr;
static writing::GamepadInfo settingsPadInfo;
static uint8_t settingsPadMapping[8]{};
static int settingsPadStep=-1;
static bool settingsPadReleased=false;
static uint32_t settingsPadIdentity=0;
static bool gamepadSettingsVisible(){
  if(activePage!=Page::Settings||!settingsPadLabel)return false;
  for(auto* p=settingsPadLabel;p;p=lv_obj_get_parent(p))if(lv_obj_has_flag(p,LV_OBJ_FLAG_HIDDEN))return false;
  return true;
}

static void cancelGamepadSettings(){
  settingsPadStep=-1;settingsPadInfo={};
  if(settingsPadCancel)lv_obj_add_flag(settingsPadCancel,LV_OBJ_FLAG_HIDDEN);
}
static bool gamepadSettingsKey(const writing::KeyEvent& e){
  if(!gamepadSettingsVisible()||settingsPadStep<0)return false;
  if(e.key==writing::Key::Escape){cancelGamepadSettings();return true;}
  return e.key==writing::Key::F1; // Guide may be the button being assigned.
}
static void serviceGamepadSettings(){
  if(!settingsPadLabel)return;
  if(!gamepadSettingsVisible()){if(settingsPadStep>=0)cancelGamepadSettings();return;}
  static uint32_t updated=0;uint32_t now=millis();if(uint32_t(now-updated)<100)return;updated=now;
  writing::GamepadInfo info;const bool connected=writing::usbGamepadInfo(info);
  setButtonEnabled(settingsPadConfigure,connected&&settingsPadStep<0);
  if(settingsPadStep>=0){
    if(!connected||info.identity!=settingsPadIdentity){cancelGamepadSettings();lv_label_set_text(settingsPadLabel,T("Manette déconnectée"));return;}
    if(!info.buttons)settingsPadReleased=true;
    if(settingsPadReleased&&info.buttons&&!(info.buttons&(info.buttons-1))){
      uint8_t button=1;while(!(info.buttons&(1u<<(button-1))))++button;
      settingsPadMapping[settingsPadStep++]=button;settingsPadReleased=false;
      if(settingsPadStep==8){bool ok=writing::usbGamepadMap(settingsPadIdentity,settingsPadMapping);cancelGamepadSettings();toast(ok?"Commandes enregistrées":"Enregistrement impossible");return;}
    }
    const char* actions[]={"A","B","SELECT","START","X","Y","L","R"};
    lv_label_set_text_fmt(settingsPadLabel,"%s : %s",T(settingsPadReleased?"Appuyez sur un bouton":"Relâchez les boutons"),actions[settingsPadStep]);return;
  }
  if(connected==bool(settingsPadInfo.identity)&&info.identity==settingsPadInfo.identity&&
      info.buttons==settingsPadInfo.buttons&&info.directions==settingsPadInfo.directions&&
      info.reports==settingsPadInfo.reports&&info.rejected==settingsPadInfo.rejected&&info.errors==settingsPadInfo.errors&&info.restarts==settingsPadInfo.restarts)return;
  settingsPadInfo=info;
  if(!connected){lv_label_set_text(settingsPadLabel,T("Branchez une manette USB"));return;}
  String pressed;
  auto append=[&](const char* text){if(pressed.length())pressed+=" · ";pressed+=text;};
  const char* actions[]={"A","B","SELECT","START","X","Y","L","R"};
  for(unsigned i=0;i<8;++i)if(info.mapping[i]&&info.mapping[i]<=32&&(info.buttons&(1u<<(info.mapping[i]-1))))append(actions[i]);
  if(info.directions&4)append(LV_SYMBOL_UP);if(info.directions&8)append(LV_SYMBOL_DOWN);
  if(info.directions&2)append(LV_SYMBOL_LEFT);if(info.directions&1)append(LV_SYMBOL_RIGHT);
  if(!info.reports)append(T(info.rejected?"Commandes non reconnues":"Aucune commande reçue"));
  const char* name=info.vendor==0x2dc8&&(info.product==0x6001||info.product==0x6101)?"8BitDo SN30 Pro":T("Manette USB");
  char raw[49]{};for(unsigned i=0;i<std::min(unsigned(info.rawSize),16u);++i)snprintf(raw+i*3,sizeof(raw)-i*3,"%02X ",unsigned(info.raw[i]));
  lv_label_set_text_fmt(settingsPadLabel,"%s · %s · %s · %04X:%04X/%u\n%s\n%s : %lu / %lu · %s : %lu / %lu\n%u B · %s",
    T("Manette connectée"),name,info.xinput?"XInput":"HID",info.vendor,info.product,info.interfaceNumber,pressed.c_str(),
    T("Rapports reçus / rejetés"),(unsigned long)info.reports,(unsigned long)info.rejected,
    T("Erreurs USB / reprises"),(unsigned long)info.errors,(unsigned long)info.restarts,info.rawSize,raw);
}
static void createGamepadSettings(lv_obj_t* parent,int y){
  auto* card=makeSettingsCard(parent,16,y,WORK_W-32,300,"Manette");
  settingsPadLabel=makeSettingsText(card,"Branchez une manette USB",20,60,WORK_W-100,false);
  lv_obj_set_width(settingsPadLabel,lv_pct(100));lv_label_set_long_mode(settingsPadLabel,LV_LABEL_LONG_WRAP);
  settingsPadConfigure=makeButton(card,"Configurer la manette",360,52);lv_obj_set_pos(settingsPadConfigure,20,224);
  settingsPadCancel=makeButton(card,"Annuler",220,52);lv_obj_set_pos(settingsPadCancel,400,224);
  lv_obj_add_flag(settingsPadCancel,LV_OBJ_FLAG_HIDDEN);setButtonEnabled(settingsPadConfigure,false);
  lv_obj_add_event_cb(settingsPadConfigure,[](lv_event_t*){
    writing::GamepadInfo info;if(!writing::usbGamepadInfo(info))return;
    settingsPadIdentity=info.identity;settingsPadStep=0;settingsPadReleased=false;
    memcpy(settingsPadMapping,info.mapping,sizeof(settingsPadMapping));
    lv_obj_remove_flag(settingsPadCancel,LV_OBJ_FLAG_HIDDEN);Tab5Focus.focusObject(settingsPadCancel);
  },LV_EVENT_CLICKED,nullptr);
  lv_obj_add_event_cb(settingsPadCancel,[](lv_event_t*){cancelGamepadSettings();},LV_EVENT_CLICKED,nullptr);
}
