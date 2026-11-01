// UI task only; fonts remain alive until restart. The popup owns every pointer
// used by font callbacks, and clears them before its children are destroyed.
static void closeDefaultFontPopup(){
  if(!defaultFontPopup)return;
  // The picker remembers a font button: close it before destroying that button.
  Tab5FilePicker::close();Tab5Focus.clear();auto* old=defaultFontPopup;
  defaultFontPopup=nullptr;fontValue=fontPreview=nullptr;
  for(auto*& button:defaultFontButtons)button=nullptr;
  lv_obj_delete(old);Tab5Focus.restore(keyboardScope());
}
static void showDefaultFontPopup(){
  if(defaultFontPopup)return;setKeyboardVisible(false);Tab5Focus.remember(keyboardScope());Tab5Focus.clear();
  namespace L=Tab5SettingsLayout;
  defaultFontPopup=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(defaultFontPopup);
  lv_obj_set_size(defaultFontPopup,SCREEN_W,SCREEN_H);lv_obj_set_style_bg_color(defaultFontPopup,lv_color_black(),0);
  lv_obj_set_style_bg_opa(defaultFontPopup,LV_OPA_70,0);lv_obj_remove_flag(defaultFontPopup,LV_OBJ_FLAG_SCROLLABLE);
  auto* box=lv_obj_create(defaultFontPopup);lv_obj_set_size(box,980,640);lv_obj_center(box);
  lv_obj_set_style_bg_color(box,C_SURFACE(),0);lv_obj_set_style_text_color(box,C_TEXT(),0);
  lv_obj_set_style_text_font(box,uiFont(),0);lv_obj_set_style_pad_all(box,20,0);
  lv_obj_set_flex_flow(box,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(box,12,0);lv_obj_remove_flag(box,LV_OBJ_FLAG_SCROLLABLE);
  auto* title=lv_label_create(box);lv_label_set_text(title,T("Polices et texte"));lv_obj_set_style_text_font(title,uiHeadingFont(),0);
  auto* content=lv_obj_create(box);lv_obj_remove_style_all(content);L::column(content);lv_obj_set_width(content,lv_pct(100));lv_obj_set_height(content,0);lv_obj_set_flex_grow(content,1);lv_obj_add_flag(content,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(content,LV_DIR_VER);lv_obj_set_style_pad_right(content,10,0);
  const char* names[]={"Police de l’interface","Police des documents"};
  for(unsigned i=0;i<2;++i){
    auto* caption=makeSettingsText(content,names[i],0,0,220,false);
    defaultFontButtons[i]=makeButton(content,"Police standard",500,48);
    auto* reset=makeButton(content,"Par défaut",150,48);
    lv_obj_add_event_cb(defaultFontButtons[i],chooseDefaultFont,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(i)));
    lv_obj_add_event_cb(reset,resetDefaultFont,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(i)));
    L::row(content,{{caption,220},{defaultFontButtons[i],0,1},{reset,150}});L::action(defaultFontButtons[i]);L::action(reset);
  }
  auto* hint=makeSettingsText(content,"Les polices choisies s’appliquent au prochain démarrage.",0,0,900,false);lv_obj_set_width(hint,lv_pct(100));lv_label_set_long_mode(hint,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_color(hint,C_MUTED(),0);
  auto* caption=makeSettingsText(content,"Texte : éditeur et ebooks",0,0,400,false);
  fontValue=makeSettingsText(content,"",0,0,64,true);char value[20];snprintf(value,sizeof(value),"%u",fontSizeForIndex(settings.fontIndex));lv_label_set_text(fontValue,value);
  auto* minus=makeIconButton(content,LV_SYMBOL_MINUS,48,48);auto* plus=makeIconButton(content,LV_SYMBOL_PLUS,48,48);
  Tab5Focus.describe(minus,T("Réduire le texte"));Tab5Focus.describe(plus,T("Agrandir le texte"));
  lv_obj_add_event_cb(minus,fontMinusEvent,LV_EVENT_CLICKED,nullptr);lv_obj_add_event_cb(plus,fontPlusEvent,LV_EVENT_CLICKED,nullptr);
  L::row(content,{{caption,0,1},{fontValue,64},{minus,48},{plus,48}});
  fontPreview=makeSettingsText(content,"Aperçu du texte",0,0,900,false);lv_obj_set_width(fontPreview,lv_pct(100));lv_label_set_long_mode(fontPreview,LV_LABEL_LONG_WRAP);Tab5DocumentText::apply(fontPreview,documentFont());
  auto* done=makeButton(box,"Fermer",180,48);lv_obj_add_event_cb(done,[](lv_event_t*){closeDefaultFontPopup();},LV_EVENT_CLICKED,nullptr);
  refreshDefaultFontButtons();Tab5UiStyle::tree(box);Tab5Focus.focusObject(defaultFontButtons[0]);
}
