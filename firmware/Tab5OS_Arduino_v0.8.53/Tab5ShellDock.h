// Included after the home catalogue and its launch dispatcher.
#pragma once
static void shellRefreshSidebar();
static unsigned dockIconNext=0;
static bool dockUsesExternal=false;
static void shellDockCaption(char (&out)[16],const char* title){
  out[0]=0;unsigned used=0;
  for(unsigned count=0;title&&*title&&count<3;++count){
    const char* end=title+1;while(*end&&(static_cast<unsigned char>(*end)&0xc0)==0x80)++end;
    const unsigned bytes=unsigned(end-title);if(used+bytes>=sizeof(out))break;
    memcpy(out+used,title,bytes);out[used+bytes]=0;
    lv_point_t size{};lv_text_get_size(&size,out,uiFont(),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    if(used&&size.x>48){out[used]=0;break;}
    used+=bytes;title=end;
  }
}
static const Tab5AppCatalogue::App* shellAppForKey(uint64_t key){
  for(unsigned i=0;i<Tab5AppCatalogue::count();++i){const auto* app=Tab5AppCatalogue::at(i);if(app&&app->key==key)return app;}
  return nullptr;
}
static bool shellDockNeedsCatalogue(){
  return dockUsesExternal;
}
static void shellLaunchDock(unsigned slot){
  if(slot>=Tab5Shell::choices.count)return;
  const uint64_t key=Tab5Shell::choices.keys[slot];
  if(key==Tab5Shell::homeKey()){openHomeAction(HomeAction::Home);return;}
  // Resolve the stable key at activation: catalogue publication may reorder entries.
  const auto* app=shellAppForKey(key);
  if(!app){toast("Application indisponible",2400);return;}
  homeLaunchApp(*app);
}
static void shellRefreshDock(){
  if(!dock)return;
  int focused=-1;for(unsigned i=0;i<Tab5Shell::DockCount;++i)if(dockButtons[i]&&Tab5Focus.contains(dockButtons[i],Tab5Focus.focused()))focused=int(i);
  if(focused>=0)Tab5Focus.clear();
  lv_obj_clean(dock);memset(dockButtons,0,sizeof(dockButtons));dockIconNext=0;
  const auto& config=Tab5Shell::choices;dockUsesExternal=false;
  for(unsigned slot=0;slot<config.count&&slot<Tab5Shell::capacity(PAGE_H);++slot){
    const uint64_t key=config.keys[slot];const bool home=key==Tab5Shell::homeKey();const auto* app=shellAppForKey(key);
    const bool external=app&&app->kind!=Tab5AppCatalogue::Kind::Builtin;
    dockUsesExternal=dockUsesExternal||(!home&&(!app||external));
    const char* title=home?"Accueil":app?app->title:"Application indisponible";
    unsigned chosen=0;
    if(app)for(unsigned i=0;i<Tab5AppCatalogue::count();++i)if(Tab5AppCatalogue::at(i)==app){chosen=Tab5AppCatalogue::icon(i);break;}
    const bool caption=external&&!chosen;
    const char* symbol=home?LV_SYMBOL_HOME:app?(!chosen&&!strcmp(app->id,"image-editor")?TAB5_SYMBOL_DRAW:homeSymbol(chosen?chosen:app->symbol)):LV_SYMBOL_WARNING;
    auto* button=dockButtons[slot]=makeButton(dock,"",52,Tab5Shell::DockButtonHeight);
    lv_obj_clean(button);lv_obj_set_style_pad_all(button,0,0);lv_obj_set_style_radius(button,12,0);
    auto* icon=lv_obj_create(button);lv_obj_remove_style_all(icon);lv_obj_set_size(icon,external?48:52,external?48:56);lv_obj_set_pos(icon,external?2:0,external?4:0);homePassive(icon);
    if(!strcmp(symbol,TAB5_SYMBOL_ALARM))tab5AlarmIcon(icon,C_TEXT(),C_RAISED());
    else if(!strcmp(symbol,TAB5_SYMBOL_CALCULATOR))tab5CalculatorIcon(icon,C_TEXT());
    else if(!strcmp(symbol,TAB5_SYMBOL_PYTHON))tab5PythonIcon(icon,C_TEXT());
    else if(!strcmp(symbol,TAB5_SYMBOL_DRAW))tab5DrawingIcon(icon,C_TEXT());
    else{auto* glyph=lv_label_create(icon);lv_label_set_text(glyph,symbol);lv_obj_set_style_text_font(glyph,&lv_font_montserrat_24,0);lv_obj_set_style_text_color(glyph,C_TEXT(),0);if(caption)lv_obj_align(glyph,LV_ALIGN_TOP_MID,0,0);else lv_obj_center(glyph);homePassive(glyph);}
    if(caption){
      char shortTitle[16];shellDockCaption(shortTitle,T(title));
      auto* label=lv_label_create(button);lv_label_set_text(label,shortTitle);lv_obj_set_size(label,48,24);lv_obj_set_pos(label,2,30);
      lv_obj_set_style_text_font(label,uiFont(),0);lv_obj_set_style_text_color(label,C_TEXT(),0);lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);homePassive(label);
    }
    if(!home&&!app)lv_obj_add_state(button,LV_STATE_DISABLED);
    Tab5Focus.describe(button,T(title));
    lv_obj_add_event_cb(button,dockEvent,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(slot)));
  }
  shellDockPositions();updateNavState();
  if(focused>=0&&config.count)Tab5Focus.focusInitial(dock,dockButtons[std::min<unsigned>(unsigned(focused),config.count-1)]);
}
static void shellServiceDockIcon(){
  while(dockIconNext<Tab5Shell::choices.count){
    const unsigned slot=dockIconNext++;auto* button=dockButtons[slot];if(!button)continue;
    const auto* app=shellAppForKey(Tab5Shell::choices.keys[slot]);if(!app||app->kind==Tab5AppCatalogue::Kind::Builtin)continue;
    for(unsigned i=0;i<Tab5AppCatalogue::count();++i)if(Tab5AppCatalogue::at(i)==app){
      if(Tab5AppCatalogue::attachIcon(lv_obj_get_child(button,0),i)&&lv_obj_get_child_count(button)>1)lv_obj_set_hidden(lv_obj_get_child(button,1),true);
      return; // One bounded icon load in the existing catalogue service pass.
    }
  }
}
static void shellCatalogueChanged(){shellRefreshDock();shellRefreshSidebar();}
