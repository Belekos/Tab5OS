// Included after keyInsertText() in the main sketch; all mutations run on UI.
static tab5ime::Composer imeComposer;
static lv_obj_t* imeInput=nullptr;
static lv_obj_t* imePanel=nullptr;
static lv_obj_t* imePreview=nullptr;
static lv_obj_t* imeCandidates=nullptr;
static lv_obj_t* imeModeButton=nullptr;
static lv_obj_t* imeKanaButton=nullptr;
static Tab5ImeDictionary::Page imePage;
static uint32_t imeFirst=0;
static int imeSelected=-1;
static bool imeFloating=false;
static std::string imeLastLookup;
static tab5ime::Mode imeLastMode=tab5ime::Mode::None;
static uint32_t imeLastFirst=UINT32_MAX;
static void imeRender();
static lv_obj_t* keyboardScope();
// A search/dialog field can be deleted while the main loop still services
// composition, even with a Latin keyboard. LVGL 9.6's validity check reads
// the object; a freed pointer must be cleared by its DELETE event instead.
static void imeClearComposition() {
  imeComposer.clear();imeSelected=-1;imeFirst=0;imeLastLookup.clear();
  imePage={};imeLastFirst=UINT32_MAX;
}
static void imeInputDeleted(lv_event_t* event) {
  if(lv_event_get_target_obj(event)!=imeInput)return;
  imeInput=nullptr;imeClearComposition();
  // imePanel has its own DELETE watcher and may outlive the input (floating IME).
  if(imePanel)lv_obj_add_flag(imePanel,LV_OBJ_FLAG_HIDDEN);
}
static void imeUseInput(lv_obj_t* input) {
  if(input==imeInput)return;
  if(imeInput)lv_obj_remove_event_cb_with_user_data(imeInput,imeInputDeleted,nullptr);
  imeInput=nullptr;imeClearComposition();
  if(input&&lv_obj_add_event_cb(input,imeInputDeleted,LV_EVENT_DELETE,nullptr))imeInput=input;
}
static bool imeUsable(lv_obj_t* input){
  if(!input||!lv_obj_is_valid(input)||Tab5TextInput::readOnly(input)||Tab5Alarm::visible()||Tab5NativeView::visible())return false;
  auto* scope=keyboardScope();
  if(Tab5Writing.isInput(input))return activePage==Page::Writing&&Tab5Writing.editing()&&scope==Tab5Writing.object();
  if(!Tab5Focus.contains(scope,input))return false;
  for(auto* p=input;p;p=lv_obj_get_parent(p))
    if(lv_obj_has_flag(p,LV_OBJ_FLAG_HIDDEN)||lv_obj_has_state(p,LV_STATE_DISABLED))return false;
  return true;
}
static void imeService(){
  if(imeInput&&!imeUsable(imeInput)){
    imeUseInput(nullptr);
    if(imePanel&&lv_obj_is_valid(imePanel))lv_obj_add_flag(imePanel,LV_OBJ_FLAG_HIDDEN);
  }
}

static void imeCancel(){imeComposer.clear();imeSelected=-1;imeFirst=0;imeLastLookup.clear();imeRender();}
static void imeSync(lv_obj_t* input){
  imeUseInput(input);
  const auto mode=Tab5NumericKeyboard::mode(imeInput)?tab5ime::Mode::None:tab5ime::modeFor(Tab5Keyboards.selectedId().c_str());
  if(imeComposer.mode()!=mode){imeComposer.setMode(mode);imeClearComposition();}
}
static void imeInsert(const char* text){
  if(!imeUsable(imeInput)||!text||!text[0])return;
  if(Tab5Writing.isInput(imeInput))Tab5Writing.insert(text);
  else if(Tab5Tools.isInput(imeInput))Tab5Tools.insert(text);
  else Tab5TextInput::insert(imeInput,text);
}
static void imeCommit(int candidate=-1,bool space=false){
  if(candidate>=0&&size_t(candidate)<imePage.count)imeInsert(imePage.words[candidate].c_str());
  else imeInsert(imeComposer.preview(true).c_str());
  if(space)imeInsert(" ");imeComposer.clear();imeFirst=0;imeSelected=-1;imeLastLookup.clear();imeRender();
}
static void imePageEvent(lv_event_t* event){
  const intptr_t direction=reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
  if(direction<0){if(imeFirst>=Tab5ImeDictionary::PAGE_SIZE)imeFirst-=Tab5ImeDictionary::PAGE_SIZE;}
  else if(imeFirst+Tab5ImeDictionary::PAGE_SIZE<imePage.total)imeFirst+=Tab5ImeDictionary::PAGE_SIZE;
  imeSelected=0;imeRender();
}
static void imeMount(lv_obj_t* parent,bool floating=false){
  if(imePanel&&lv_obj_is_valid(imePanel))lv_obj_delete(imePanel);
  imePanel=lv_obj_create(parent);imeFloating=floating;
  lv_obj_set_size(imePanel,floating?SCREEN_W-24:LV_PCT(100),90);
  if(floating)lv_obj_align(imePanel,LV_ALIGN_BOTTOM_MID,0,-12);
  lv_obj_set_style_bg_color(imePanel,C_SURFACE(),0);
  lv_obj_set_style_pad_all(imePanel,3,0);lv_obj_set_style_border_width(imePanel,floating?1:0,0);
  lv_obj_remove_flag(imePanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(imePanel,[](lv_event_t*){imePanel=nullptr;imePreview=nullptr;imeCandidates=nullptr;imeModeButton=nullptr;imeKanaButton=nullptr;},LV_EVENT_DELETE,nullptr);
  imePreview=lv_label_create(imePanel);lv_obj_set_pos(imePreview,4,0);lv_obj_set_width(imePreview,LV_PCT(57));lv_label_set_long_mode(imePreview,LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_font(imePreview,uiFont(),0);lv_obj_set_style_text_color(imePreview,C_TEXT(),0);
  imeModeButton=makeButton(imePanel,"ABC",74,34);lv_obj_align(imeModeButton,LV_ALIGN_TOP_RIGHT,-86,0);
  lv_obj_add_event_cb(imeModeButton,[](lv_event_t*){if(!imeComposer.empty())imeCommit();imeComposer.toggleLatin();imeRender();},LV_EVENT_CLICKED,nullptr);
  imeKanaButton=makeButton(imePanel,"あ/ア",80,34);lv_obj_align(imeKanaButton,LV_ALIGN_TOP_RIGHT,-164,0);
  lv_obj_set_style_text_font(imeKanaButton,uiFont(),0);
  lv_obj_add_event_cb(imeKanaButton,[](lv_event_t*){imeComposer.toggleKana();imeRender();},LV_EVENT_CLICKED,nullptr);
  auto* commit=makeIconButton(imePanel,LV_SYMBOL_OK,76,34);lv_obj_align(commit,LV_ALIGN_TOP_RIGHT,0,0);
  lv_obj_add_event_cb(commit,[](lv_event_t*){imeCommit(imeSelected);},LV_EVENT_CLICKED,nullptr);
  imeCandidates=lv_obj_create(imePanel);lv_obj_set_pos(imeCandidates,0,38);lv_obj_set_size(imeCandidates,LV_PCT(100),44);
  lv_obj_set_style_pad_all(imeCandidates,0,0);lv_obj_set_style_pad_column(imeCandidates,3,0);lv_obj_set_style_border_width(imeCandidates,0,0);
  lv_obj_set_style_bg_opa(imeCandidates,LV_OPA_TRANSP,0);lv_obj_set_flex_flow(imeCandidates,LV_FLEX_FLOW_ROW);lv_obj_remove_flag(imeCandidates,LV_OBJ_FLAG_SCROLLABLE);
  imeRender();
}
static void imeRender(){
  if(!imePanel||!lv_obj_is_valid(imePanel))return;
  const bool active=imeComposer.mode()!=tab5ime::Mode::None;
  if(!active||(imeFloating&&imeComposer.empty())){lv_obj_add_flag(imePanel,LV_OBJ_FLAG_HIDDEN);return;}
  lv_obj_remove_flag(imePanel,LV_OBJ_FLAG_HIDDEN);
  const char* native=imeComposer.mode()==tab5ime::Mode::Pinyin?"拼音":imeComposer.mode()==tab5ime::Mode::Japanese?"あ":"한";
  lv_label_set_text(lv_obj_get_child(imeModeButton,0),imeComposer.latin()?native:"ABC");
  if(imeComposer.mode()==tab5ime::Mode::Japanese&&!imeComposer.latin())lv_obj_remove_flag(imeKanaButton,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(imeKanaButton,LV_OBJ_FLAG_HIDDEN);
  const std::string preview=imeComposer.preview();lv_label_set_text(imePreview,preview.empty()?native:preview.c_str());
  const std::string query=imeComposer.lookupKey();
  if(query!=imeLastLookup||imeFirst!=imeLastFirst||imeComposer.mode()!=imeLastMode){
    imePage=Tab5ImeWords.lookup(imeComposer.mode(),query.c_str(),imeFirst);
    imeLastLookup=query;imeLastFirst=imeFirst;imeLastMode=imeComposer.mode();
  }
  lv_obj_clean(imeCandidates);
  if(imeFirst){auto* prev=makeIconButton(imeCandidates,LV_SYMBOL_LEFT,40,40);lv_obj_add_event_cb(prev,imePageEvent,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(-1)));}
  for(size_t i=0;i<imePage.count;++i){
    String label=String(i+1)+" "+imePage.words[i];auto* b=makeButton(imeCandidates,label.c_str(),0,40);lv_obj_set_flex_grow(b,1);
    auto* text=lv_obj_get_child(b,0);lv_obj_set_style_text_font(text,uiFont(),0);lv_obj_set_width(text,LV_PCT(100));lv_label_set_long_mode(text,LV_LABEL_LONG_DOT);
    if(int(i)==imeSelected)lv_obj_set_style_bg_color(b,C_SELECTED(),0);
    lv_obj_add_event_cb(b,[](lv_event_t* e){imeCommit(int(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))));},LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(i)));
  }
  if(imeFirst+imePage.count<imePage.total){auto* next=makeIconButton(imeCandidates,LV_SYMBOL_RIGHT,40,40);lv_obj_add_event_cb(next,imePageEvent,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(1)));}
  if(!imePage.count&&!imeComposer.empty()&&imeComposer.mode()!=tab5ime::Mode::Korean&&!imePage.available){
    auto* status=lv_label_create(imeCandidates);lv_label_set_text(status,T("Dictionnaire de saisie indisponible"));lv_obj_set_style_text_font(status,uiFont(),0);
  }
}
static bool imePhysical(const writing::KeyEvent& e,lv_obj_t* input){
  if(!imeUsable(input))input=nullptr;
  if(!input||!lv_obj_is_valid(input)){if(!imeComposer.empty())imeCancel();imeUseInput(nullptr);return false;}
  imeSync(input);using K=writing::Key;
  if(!imeInput||imeComposer.mode()==tab5ime::Mode::None||!Tab5Cjk.availableFor(Tab5Keyboards.selectedId()))return false;
  if(e.ctrl&&!e.alt&&!e.meta&&e.key==K::Text&&e.text[0]==' '){if(!imeComposer.empty())imeCommit();imeComposer.toggleLatin();imeRender();return true;}
  if(!imeComposer.enabled()||e.ctrl||e.alt||e.meta)return false;
  if(!imeComposer.empty()){
    if(e.key==K::Escape){imeCancel();return true;}
    if(e.key==K::Backspace){imeComposer.backspace();imeFirst=0;imeSelected=-1;imeRender();return true;}
    if(e.key==K::Enter){imeCommit(imeSelected>=0?imeSelected:(imeComposer.mode()==tab5ime::Mode::Pinyin&&imePage.count?0:-1));return true;}
    if(e.key==K::PageDown||e.key==K::PageUp){if(e.key==K::PageUp){if(imeFirst>=Tab5ImeDictionary::PAGE_SIZE)imeFirst-=Tab5ImeDictionary::PAGE_SIZE;}else if(imeFirst+imePage.count<imePage.total)imeFirst+=Tab5ImeDictionary::PAGE_SIZE;imeSelected=0;imeRender();return true;}
    if(e.key==K::Left||e.key==K::Right){if(imePage.count){imeSelected=imeSelected<0?(e.key==K::Right?0:int(imePage.count)-1):(imeSelected+int(imePage.count)+(e.key==K::Right?1:-1))%int(imePage.count);imeRender();return true;}imeCommit();return false;}
    if(e.key==K::Text&&e.text[1]==0&&e.text[0]>='1'&&e.text[0]<='6'&&size_t(e.text[0]-'1')<imePage.count){imeCommit(e.text[0]-'1');return true;}
    if(e.key==K::Text&&e.text[0]==' '&&!e.text[1]){
      if(imePage.count){imeSelected=(imeSelected+1)%int(imePage.count);imeRender();}
      else imeCommit(-1,imeComposer.mode()==tab5ime::Mode::Korean);return true;
    }
  }
  if(e.key==K::Text&&e.text[0]&&!e.text[1]&&imeComposer.feed(e.text[0])){
    imeFirst=0;imeSelected=-1;
    if(!imePanel||!lv_obj_is_valid(imePanel)||(!imeFloating&&lv_obj_has_flag(keyboardPanel,LV_OBJ_FLAG_HIDDEN)))imeMount(lv_layer_top(),true);
    imeRender();return true;
  }
  // Punctuation or navigation commits the visible preedit before the normal
  // control handles the key; shortcuts above remain untouched.
  if(!imeComposer.empty())imeCommit(imeSelected);
  if(e.key==K::Text&&!e.text[1]&&imeComposer.mode()!=tab5ime::Mode::Korean){
    if(e.text[0]=='.'){imeInsert("。");return true;}
    if(e.text[0]==','){imeInsert(imeComposer.mode()==tab5ime::Mode::Japanese?"、":"，");return true;}
  }
  return false;
}
static bool imeTouch(const char* token){
  writing::KeyEvent e;
  if(!strcmp(token,"<BKSP>"))e.key=writing::Key::Backspace;
  else if(!strcmp(token,"<ENTER>"))e.key=writing::Key::Enter;
  else if(!strcmp(token,"<LEFT>"))e.key=writing::Key::Left;
  else if(!strcmp(token,"<RIGHT>"))e.key=writing::Key::Right;
  else if(!strcmp(token,"<UP>"))e.key=writing::Key::Up;
  else if(!strcmp(token,"<DOWN>"))e.key=writing::Key::Down;
  else if(!strcmp(token,"<HOME>"))e.key=writing::Key::Home;
  else if(!strcmp(token,"<END>"))e.key=writing::Key::End;
  else if(token[0]=='<')return false;
  else{e.key=writing::Key::Text;strncpy(e.text,token,sizeof(e.text)-1);}
  return imePhysical(e,keyboardTarget);
}
