// Included after home actions and settings helpers in the UI translation unit.
#pragma once
static lv_obj_t* sidebarItems=nullptr;
static lv_obj_t* sidebarAdd=nullptr;
static lv_obj_t* sidebarCountLabel=nullptr;
static bool sidebarPicking=false;
static bool sidebarRenderedPicker=false;
static bool shellAtMainMenu(){return activePage==Page::Home&&homeMenu==0&&!homeCustomize&&!Tab5Tools.scope()&&!Tab5GitView::scope();}
static void shellDockPositions();
static void shellTopLayout(){
  const bool hidden=!shellAtMainMenu()&&(Tab5Shell::choices.dockWidth()==0||!Tab5Shell::choices.contains(Tab5Shell::homeKey()));
  if(topHomeButton)setTopActionVisible(topHomeButton,hidden);
  if(topPythonResume)lv_obj_set_x(topPythonResume,hidden?64:18);
  const int meshX=hidden?132:80;
  auto* badge=Tab5MeshBadge::object();
  if(badge)lv_obj_set_pos(badge,meshX,6);
  const int titleX=badge&&!lv_obj_has_flag(badge,LV_OBJ_FLAG_HIDDEN)?meshX+70:(hidden?132:112);
  if(titleLabel){
    // LONG_DOT needs a fixed one-line height; auto height permits wrapping.
    lv_obj_set_size(titleLabel,532-titleX,
        lv_font_get_line_height(lv_obj_get_style_text_font(titleLabel,LV_PART_MAIN)));
    lv_obj_align(titleLabel,LV_ALIGN_LEFT_MID,titleX,0);
  }
  Tab5TopBarLayout::date(dateLabel,topAudioIndicator,SCREEN_W);
  shellDockPositions();
}
static void shellDockPositions(){
  const auto& config=Tab5Shell::choices;unsigned visible=0;
  const unsigned limit=std::min<unsigned>(config.count,Tab5Shell::capacity(PAGE_H));
  for(unsigned slot=0;slot<limit;++slot)if(!(config.keys[slot]==Tab5Shell::homeKey()&&shellAtMainMenu()))++visible;
  const int pitch=visible>1?std::min(91,(PAGE_H-int(Tab5Shell::DockMargin)*2-int(Tab5Shell::DockButtonHeight))/int(visible-1)):0;
  unsigned row=0;
  for(unsigned slot=0;slot<Tab5Shell::DockCount;++slot){auto* button=dockButtons[slot];if(!button)continue;
    lv_obj_move_foreground(button);
    if(slot<limit&&!(config.keys[slot]==Tab5Shell::homeKey()&&shellAtMainMenu())){lv_obj_remove_flag(button,LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(button,6,int(Tab5Shell::DockMargin)+int(row++)*pitch);}
    else lv_obj_add_flag(button,LV_OBJ_FLAG_HIDDEN);
  }
}
static void applyShellLayout(){
  const int nextWidth=SCREEN_W-Tab5Shell::choices.dockWidth();
  const bool changed=nextWidth!=WORK_W;
  if(changed&&mapCanvas){++mapGeneration;if(!mapGeneration)++mapGeneration;freeMapCanvas();}
  WORK_W=nextWidth;MAP_VIEW_W=WORK_W-16;
  Tab5Shell::apply(WORK_W);
  if(pageHost){lv_obj_set_x(pageHost,SCREEN_W-WORK_W);lv_obj_set_width(pageHost,WORK_W);}
  if(dock)setTopActionVisible(dock,Tab5Shell::choices.dockWidth()!=0);
  shellDockPositions();shellTopLayout();
  const int tabWidth=(WORK_W-32-14*(SETTINGS_SECTION_COUNT-1))/SETTINGS_SECTION_COUNT;
  for(unsigned i=0;i<SETTINGS_SECTION_COUNT;++i)if(settingsSectionButtons[i]){lv_obj_set_width(settingsSectionButtons[i],tabWidth);lv_obj_set_x(settingsSectionButtons[i],16+int(i)*(tabWidth+14));}
  if(homeDefaultGrid)homeRequestRender();
  if(changed&&activePage==Page::Map){Tab5Map.beginGeneration(mapGeneration);renderMapViewport(false);}
  if(pageHost)lv_obj_update_layout(pageHost);
  Tab5Writing.resize(PAGE_H-(keyboardVisible?keyboardHeight():0));
  Tab5Tools.resize(PAGE_H-(keyboardVisible?keyboardHeight():0));
}
static void saveSidebar(){
  if(!Tab5Shell::save())toast("Enregistrement des préférences impossible",2800);
  shellRefreshDock();applyShellLayout();shellRefreshSidebar();
}
static void closeSidebarSettings(){
  Tab5Focus.clear();auto* old=sidebarSettingsOverlay;sidebarSettingsOverlay=nullptr;
  sidebarItems=sidebarAdd=sidebarCountLabel=nullptr;sidebarPicking=false;
  if(old&&lv_obj_is_valid(old))lv_obj_delete(old);
  Tab5Focus.restore(settingsPage);
}
static void sidebarAddKey(uint64_t key){
  if(key!=Tab5Shell::homeKey()&&!shellAppForKey(key)){toast("Application indisponible",2400);shellRefreshSidebar();return;}
  if(!Tab5Shell::choices.add(key,Tab5Shell::capacity(PAGE_H))){
    char message[96];snprintf(message,sizeof(message),T("Bandeau complet (%u raccourcis)"),Tab5Shell::capacity(PAGE_H));toast(message,2400);return;
  }
  sidebarPicking=false;saveSidebar();
}
static void shellRefreshSidebar(){
  if(!sidebarItems)return;
  const int32_t scroll=lv_obj_get_scroll_y(sidebarItems);
  auto* previous=Tab5Focus.focused();unsigned focused=0;uint64_t focusedKey=0;
  const bool hadFocus=previous&&Tab5Focus.contains(sidebarItems,previous);
  if(hadFocus){if(sidebarRenderedPicker){auto* key=static_cast<uint64_t*>(lv_obj_get_user_data(previous));if(key)focusedKey=*key;}
    else focused=unsigned(uintptr_t(lv_obj_get_user_data(previous)));Tab5Focus.clear();}
  lv_obj_clean(sidebarItems);lv_obj_t* nextFocus=nullptr;
  char count[64];snprintf(count,sizeof(count),T("%u / %u raccourcis"),unsigned(Tab5Shell::choices.count),Tab5Shell::capacity(PAGE_H));lv_label_set_text(sidebarCountLabel,count);
  setText(lv_obj_get_child(sidebarAdd,0),sidebarPicking?T("Retour"):T("Ajouter une application"));
  if(sidebarPicking||Tab5Shell::choices.count<Tab5Shell::capacity(PAGE_H))lv_obj_remove_state(sidebarAdd,LV_STATE_DISABLED);else lv_obj_add_state(sidebarAdd,LV_STATE_DISABLED);
  sidebarRenderedPicker=sidebarPicking;
  int y=0;
  if(sidebarPicking){
    bool memoryFailed=false;
    auto add=[&](const char* title,uint64_t key){
      auto* ref=static_cast<uint64_t*>(lv_malloc(sizeof(uint64_t)));if(!ref){memoryFailed=true;return;}*ref=key;
      auto* button=makeButton(sidebarItems,title,1010,48);lv_obj_set_pos(button,0,y);y+=54;
      auto* label=lv_obj_get_child(button,0);lv_obj_set_size(label,990,lv_font_get_line_height(uiFont()));lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(label);
      lv_obj_set_user_data(button,ref);if(key==focusedKey)nextFocus=button;
      if(!lv_obj_add_event_cb(button,[](lv_event_t* e){
        auto* key=static_cast<uint64_t*>(lv_event_get_user_data(e));
        if(lv_event_get_code(e)==LV_EVENT_DELETE)lv_free(key);
        else if(lv_event_get_code(e)==LV_EVENT_CLICKED)sidebarAddKey(*key);
      },LV_EVENT_ALL,ref)){if(nextFocus==button)nextFocus=nullptr;lv_free(ref);lv_obj_delete(button);y-=54;memoryFailed=true;}
    };
    if(!Tab5Shell::choices.contains(Tab5Shell::homeKey()))add("Accueil",Tab5Shell::homeKey());
    unsigned order[Tab5AppCatalogue::MaxApps];const unsigned n=Tab5AppCatalogue::ordered(order,false);
    const Tab5AppCatalogue::Kind kinds[]={Tab5AppCatalogue::Kind::Builtin,Tab5AppCatalogue::Kind::Executable,Tab5AppCatalogue::Kind::Script};
    const char* names[]={"Applications intégrées","Apps ELF","Apps Python"};
    for(unsigned group=0;group<3;++group){bool title=false;
      for(unsigned i=0;i<n;++i){const auto* app=Tab5AppCatalogue::at(order[i]);if(!app||app->kind!=kinds[group]||Tab5Shell::choices.contains(app->key))continue;
        if(!title){auto* label=homeLabel(sidebarItems,names[group],0,y+8,1010);lv_obj_set_style_text_color(label,C_MUTED(),0);y+=40;title=true;}
        add(app->title,app->key);
      }
    }
    if(memoryFailed)toast("Mémoire insuffisante",2400);
  }else{
    const auto& config=Tab5Shell::choices;
    for(unsigned slot=0;slot<config.count;++slot){
      const uint64_t key=config.keys[slot];const auto* app=shellAppForKey(key);
      auto* row=lv_obj_create(sidebarItems);lv_obj_remove_style_all(row);lv_obj_set_pos(row,0,y);lv_obj_set_size(row,1010,56);lv_obj_remove_flag(row,LV_OBJ_FLAG_SCROLLABLE);y+=62;
      makeSettingsText(row,key==Tab5Shell::homeKey()?"Accueil":app?app->title:"Application indisponible",8,14,790,false);
      const char* symbols[]={LV_SYMBOL_UP,LV_SYMBOL_DOWN,LV_SYMBOL_TRASH};const char* tips[]={"Monter","Descendre","Supprimer"};
      for(unsigned action=0;action<3;++action){
        auto* button=makeIconButton(row,symbols[action],48,48);lv_obj_set_pos(button,830+int(action)*60,4);Tab5Focus.describe(button,T(tips[action]));
        const unsigned token=1+slot*3+action;lv_obj_set_user_data(button,reinterpret_cast<void*>(uintptr_t(token)));
        if(token==focused||(key==focusedKey&&action==2))nextFocus=button;
        if((action==0&&slot==0)||(action==1&&slot+1==config.count))lv_obj_add_state(button,LV_STATE_DISABLED);
        lv_obj_add_event_cb(button,[](lv_event_t* e){
          const unsigned token=unsigned(uintptr_t(lv_event_get_user_data(e)))-1;const unsigned slot=token/3,action=token%3;
          if(action==2)Tab5Shell::choices.remove(slot);else Tab5Shell::choices.move(slot,action==0?-1:1);
          saveSidebar();
        },LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(token)));
      }
    }
  }
  lv_obj_update_layout(sidebarItems);lv_obj_scroll_to_y(sidebarItems,scroll,LV_ANIM_OFF);
  Tab5UiStyle::tree(sidebarItems);
  if(hadFocus)Tab5Focus.focusInitial(sidebarSettingsOverlay,nextFocus?nextFocus:sidebarAdd);
}
static void showSidebarSettings(){
  if(sidebarSettingsOverlay)return;
  setKeyboardVisible(false);Tab5Focus.remember(settingsPage);sidebarPicking=false;
  sidebarSettingsOverlay=lv_obj_create(root);lv_obj_remove_style_all(sidebarSettingsOverlay);
  lv_obj_set_size(sidebarSettingsOverlay,SCREEN_W,SCREEN_H);
  lv_obj_set_style_bg_color(sidebarSettingsOverlay,lv_color_black(),0);lv_obj_set_style_bg_opa(sidebarSettingsOverlay,LV_OPA_50,0);
  lv_obj_remove_flag(sidebarSettingsOverlay,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(sidebarSettingsOverlay,[](lv_event_t* e){
    if(lv_event_get_target_obj(e)==sidebarSettingsOverlay){sidebarSettingsOverlay=nullptr;sidebarItems=sidebarAdd=sidebarCountLabel=nullptr;sidebarPicking=false;}
  },LV_EVENT_DELETE,nullptr);
  auto* box=makeSettingsCard(sidebarSettingsOverlay,0,0,1120,672,"Accueil et bandeau");lv_obj_center(box);
  auto* close=makeIconButton(box,LV_SYMBOL_CLOSE,52,44);lv_obj_set_pos(close,1048,12);
  lv_obj_add_event_cb(close,[](lv_event_t*){closeSidebarSettings();},LV_EVENT_CLICKED,nullptr);
  auto* customize=makeButton(box,"Personnaliser l’accueil",480,48);lv_obj_set_pos(customize,20,66);
  lv_obj_add_event_cb(customize,[](lv_event_t*){closeSidebarSettings();homeOpenCatalogue(true);},LV_EVENT_CLICKED,nullptr);
  auto* toggle=lv_switch_create(box);styleSettingsSwitch(toggle);lv_obj_set_pos(toggle,530,70);
  makeSettingsText(box,"Afficher le bandeau latéral",622,78,454,false);Tab5Focus.describe(toggle,T("Afficher le bandeau latéral"));
  if(Tab5Shell::choices.enabled)lv_obj_add_state(toggle,LV_STATE_CHECKED);
  lv_obj_add_event_cb(toggle,[](lv_event_t* e){Tab5Shell::choices.enabled=lv_obj_has_state(lv_event_get_target_obj(e),LV_STATE_CHECKED);saveSidebar();},LV_EVENT_VALUE_CHANGED,nullptr);
  sidebarAdd=makeButton(box,"Ajouter une application",480,48);lv_obj_set_pos(sidebarAdd,20,126);
  lv_obj_add_event_cb(sidebarAdd,[](lv_event_t*){sidebarPicking=!sidebarPicking;lv_obj_scroll_to_y(sidebarItems,0,LV_ANIM_OFF);shellRefreshSidebar();Tab5Focus.focusObject(sidebarAdd);},LV_EVENT_CLICKED,nullptr);
  sidebarCountLabel=homeLabel(box,"",530,140,540);lv_obj_set_style_text_color(sidebarCountLabel,C_MUTED(),0);
  sidebarItems=lv_obj_create(box);lv_obj_remove_style_all(sidebarItems);lv_obj_set_pos(sidebarItems,20,188);lv_obj_set_size(sidebarItems,1080,464);
  lv_obj_add_flag(sidebarItems,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(sidebarItems,LV_DIR_VER);lv_obj_set_scrollbar_mode(sidebarItems,LV_SCROLLBAR_MODE_AUTO);
  homeAppsDirty=true;
  shellRefreshSidebar();Tab5UiStyle::tree(sidebarSettingsOverlay);Tab5Focus.focusObject(toggle);
}
