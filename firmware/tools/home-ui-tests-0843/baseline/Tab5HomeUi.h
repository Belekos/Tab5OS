// Included after the native home actions, in the UI task's translation unit.
#pragma once
#include "Tab5AppRefresh.h"
static void renderHomeApps();
static Page homeCustomizeReturn=Page::Settings;
static lv_obj_t* homeLayoutChoice=nullptr;
static lv_obj_t* homeDensityChoice=nullptr;
static lv_obj_t* homeLabelsChoice=nullptr;
static unsigned homeMenu=0; // 0: native home, 1: ELF, 2: Python
static int32_t homeMenuScroll[3]{};
static bool homeRestoreMenuScroll=false,homeFocusMenu=false;
static uint64_t homePendingFocus=0;
static lv_obj_t* homeAppButtons[Tab5AppCatalogue::MaxApps]{};
static uint64_t homeAppButtonKeys[Tab5AppCatalogue::MaxApps]{};
static void homeOpenMenu(unsigned menu){
  if(homeNativeView&&!homeCustomize)homeMenuScroll[homeMenu]=lv_obj_get_scroll_y(homeNativeView);
  homePendingFocus=menu==0&&homeMenu?Tab5AppCatalogue::keyFor(homeMenu==1?"elf-apps":"python-apps"):0;
  homeMenu=menu<3?menu:0;homeCustomize=false;homePythonLibrary=homeMenu!=0;
  homeRestoreMenuScroll=true;homeFocusMenu=true;homeRenderPending=true;
  if(activePage!=Page::Home)showPage(Page::Home);
  updateTopBarForPage();
}
static const char* homePageTitle(){return homeCustomize?"Personnaliser l'accueil":homeMenu==1?"Applications ELF":homeMenu==2?"Applications Python":"Tab5OS";}
static const char* homeSymbol(unsigned index){
  static const char* symbols[]={LV_SYMBOL_PLAY,LV_SYMBOL_DIRECTORY,LV_SYMBOL_EDIT,LV_SYMBOL_AUDIO,
    LV_SYMBOL_GPS,LV_SYMBOL_SETTINGS,LV_SYMBOL_PLAY,LV_SYMBOL_IMAGE,LV_SYMBOL_DOWNLOAD,LV_SYMBOL_SD_CARD,
    LV_SYMBOL_EYE_OPEN,TAB5_SYMBOL_CALCULATOR,LV_SYMBOL_KEYBOARD,LV_SYMBOL_HOME,LV_SYMBOL_VIDEO,
    LV_SYMBOL_WIFI,LV_SYMBOL_BELL,LV_SYMBOL_SAVE,LV_SYMBOL_FILE,LV_SYMBOL_LIST,LV_SYMBOL_UPLOAD,
    LV_SYMBOL_CALL,LV_SYMBOL_CHARGE,LV_SYMBOL_GPS,TAB5_SYMBOL_PYTHON};
  static_assert(sizeof(symbols)/sizeof(symbols[0])==Tab5AppCatalogue::SymbolCount,"home symbol choices");
  return symbols[index<Tab5AppCatalogue::SymbolCount?index:0];
}
static void homeRequestRender(){homeRenderPending=true;}
static void homeShowView(){
  if(!homeNativeView||!homePythonView)return;
  if(homeCustomize){lv_obj_add_flag(homeNativeView,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(homePythonView,LV_OBJ_FLAG_HIDDEN);}
  else{lv_obj_remove_flag(homeNativeView,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(homePythonView,LV_OBJ_FLAG_HIDDEN);}
  homePythonLibrary=homeCustomize||homeMenu!=0;
  homeRequestRender();updateTopBarForPage();
}
static void homeOpenCatalogue(bool customize){
  if(customize){
    homeCustomizeReturn=activePage==Page::Home?Page::Settings:activePage;
    if(activePage!=Page::Home)showPage(Page::Home);
    homeCustomize=true;
  }else{
    if(homeCustomize)Tab5AppCatalogue::loadChoices();
    homeCustomize=false;
    if(activePage!=Page::Home)showPage(Page::Home);
  }
  Tab5Focus.clear();homeShowView();
}
static void homeCancelCustomize(){
  Tab5AppCatalogue::loadChoices();homeCustomize=false;homeShowView();
  showPage(homeCustomizeReturn);
}
static void homeAppLaunch(lv_event_t* event){
  const unsigned index=unsigned(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  const auto* source=Tab5AppCatalogue::at(index);if(!source)return;
  const Tab5AppCatalogue::App app=*source;
  if(homeAppsScanning)homeAppsDirty=true;
  Tab5AppCatalogue::endScan();homeAppsScanning=false;
  if(app.kind==Tab5AppCatalogue::Kind::Builtin){
    if(app.action==254){homeOpenMenu(1);return;}
    if(app.action==253){homeOpenMenu(2);return;}
    openHomeAction(static_cast<HomeAction>(app.action),true);return;
  }
  setKeyboardVisible(false);keyboardDockFocus=false;keyboardUiNavigation=true;Tab5Focus.clear();
  // Keep the home view and its scroll position below the application.
  if(app.kind==Tab5AppCatalogue::Kind::Script){
    if(Tab5Tools.openPythonApp(app.id))Tab5Focus.focusFirst(Tab5Tools.scope());
  }else{
    Tab5Tools.launchExecutable(app.id);
    if(Tab5Tools.scope())Tab5Focus.focusFirst(Tab5Tools.scope());
  }
}
static void homeLaunchSettings(lv_event_t* event){
  const unsigned index=unsigned(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  const auto* app=Tab5AppCatalogue::at(index);if(!app)return;
  Tab5AppCatalogue::endScan();homeAppsScanning=false;
  Tab5Tools.launchExecutable(app->id,true);
  if(Tab5Tools.scope())Tab5Focus.focusFirst(Tab5Tools.scope());
}
static void homePinEvent(lv_event_t* event){
  const unsigned index=unsigned(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  if(!Tab5AppCatalogue::pin(index,!Tab5AppCatalogue::pinned(index)))toast("Impossible de personnaliser l'accueil",2800);
  homeRequestRender();
}
static void homeMoveEvent(lv_event_t* event){
  uintptr_t data=reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  Tab5AppCatalogue::move(unsigned(data>>1),(data&1)?1:-1);homeRequestRender();
}
static void homeIconEvent(lv_event_t* event){
  const unsigned index=unsigned(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  auto* dropdown=static_cast<lv_obj_t*>(lv_event_get_target(event));
  if(!Tab5AppCatalogue::setIcon(index,uint8_t(lv_dropdown_get_selected(dropdown))))toast("Impossible de personnaliser l'accueil",2800);
  homeRequestRender();
}
static void homeSaveEvent(lv_event_t*){
  if(!Tab5AppCatalogue::saveChoices()){toast("Enregistrement des préférences impossible",3200);return;}
  homeCustomize=false;homeShowView();showPage(homeCustomizeReturn);toast("Accueil enregistré",1800);
}
static void showPythonLibrary(bool visible){
  if(!visible&&homeCustomize){homeCancelCustomize();return;}
  homeOpenMenu(visible?2:0);
}
static void homeScrollbar(lv_obj_t* object){
  lv_obj_add_flag(object,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(object,LV_DIR_VER);
  lv_obj_set_scrollbar_mode(object,LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_width(object,8,LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(object,C_MUTED(),LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(object,LV_OPA_80,LV_PART_SCROLLBAR);
  lv_obj_set_style_radius(object,4,LV_PART_SCROLLBAR);
}
static void homePassive(lv_obj_t* object){lv_obj_remove_flag(object,lv_obj_flag_t(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE));}
static lv_obj_t* homeLabel(lv_obj_t* parent,const char* text,int x,int y,int width){
  auto* label=lv_label_create(parent);lv_label_set_text(label,T(text));lv_obj_set_pos(label,x,y);lv_obj_set_width(label,width);
  lv_obj_set_style_text_font(label,uiFont(),0);lv_obj_set_style_text_color(label,C_TEXT(),0);lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);homePassive(label);return label;
}
static void homeSection(lv_obj_t* parent,const char* title,int y){
  auto* label=homeLabel(parent,title,28,y+6,WORK_W-56);lv_obj_set_style_text_color(label,C_MUTED(),0);
}
static void homeCustomizeRow(lv_obj_t* parent,unsigned index,int y,bool first,bool last){
  const auto* app=Tab5AppCatalogue::at(index);if(!app)return;
  const int width=WORK_W-56;
  auto* object=lv_obj_create(parent);lv_obj_set_pos(object,28,y);lv_obj_set_size(object,width,82);
  lv_obj_set_style_bg_color(object,C_SURFACE(),0);lv_obj_set_style_border_color(object,C_BORDER(),0);
  lv_obj_set_style_pad_all(object,0,0);lv_obj_remove_flag(object,LV_OBJ_FLAG_SCROLLABLE);
  const bool selected=Tab5AppCatalogue::pinned(index);
  auto* toggle=makeIconButton(object,selected?LV_SYMBOL_EYE_OPEN:LV_SYMBOL_EYE_CLOSE,48,48);lv_obj_set_pos(toggle,10,16);
  Tab5Focus.describe(toggle,T(selected?"Masquer sur l'accueil":"Afficher sur l'accueil"));
  lv_obj_add_event_cb(toggle,homePinEvent,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(index)));
  const int iconWidth=310,controlsX=width-iconWidth-218;
  auto* title=homeLabel(object,app->title,74,27,controlsX-92);if(!selected)lv_obj_set_style_text_color(title,C_MUTED(),0);
  if(app->kind==Tab5AppCatalogue::Kind::Executable){
    auto* launch=makeIconButton(object,LV_SYMBOL_SETTINGS,48,48);lv_obj_set_pos(launch,controlsX,16);Tab5Focus.describe(launch,T("Réglages de lancement"));
    lv_obj_add_event_cb(launch,homeLaunchSettings,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(index)));
  }
  auto* up=makeIconButton(object,LV_SYMBOL_UP,48,48);lv_obj_set_pos(up,controlsX+58,16);Tab5Focus.describe(up,T("Monter"));
  auto* down=makeIconButton(object,LV_SYMBOL_DOWN,48,48);lv_obj_set_pos(down,controlsX+116,16);Tab5Focus.describe(down,T("Descendre"));
  if(!selected||first)lv_obj_add_flag(up,LV_OBJ_FLAG_HIDDEN);
  if(!selected||last)lv_obj_add_flag(down,LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(up,homeMoveEvent,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(index)*2));
  lv_obj_add_event_cb(down,homeMoveEvent,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(index)*2+1));
  auto* icon=lv_dropdown_create(object);lv_obj_set_pos(icon,width-iconWidth-12,16);lv_obj_set_size(icon,iconWidth,48);
  const char* labels[]={"Automatique","Dossier","Écriture","Audio","Carte","Réglages","Lecture","Image","Téléchargement","Stockage","Livre","Calculatrice","Clavier","Accueil","Vidéo","Réseau","Réveil","Enregistrement","Document","Liste","Envoi","Téléphone","Énergie","Navigation","Terminal"};
  String options;for(const auto* label:labels){if(options.length())options+='\n';options+=T(label);}
  lv_dropdown_set_options(icon,options.c_str());lv_dropdown_set_selected(icon,Tab5AppCatalogue::icon(index));
  lv_obj_set_style_text_font(icon,uiFont(),0);Tab5Focus.describe(icon,T("Icône de l'application"));
  lv_obj_add_event_cb(icon,homeIconEvent,LV_EVENT_VALUE_CHANGED,reinterpret_cast<void*>(uintptr_t(index)));
}
static void homeApplicationCard(lv_obj_t* parent,unsigned index,int x,int y,int width,int height,bool list,bool labels){
  const auto* app=Tab5AppCatalogue::at(index);if(!app)return;
  auto* card=lv_button_create(parent);lv_obj_set_pos(card,x,y);lv_obj_set_size(card,width,height);
  homeAppButtons[index]=card;homeAppButtonKeys[index]=app->key;
  lv_obj_set_style_bg_color(card,C_SURFACE(),0);lv_obj_set_style_bg_color(card,C_ACTIVE(),LV_STATE_PRESSED);
  lv_obj_set_style_radius(card,14,0);lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_border_color(card,C_BORDER(),0);
  lv_obj_set_style_pad_all(card,0,0);lv_obj_set_style_shadow_width(card,0,0);lv_obj_remove_flag(card,LV_OBJ_FLAG_SCROLLABLE);
  Tab5Focus.describe(card,T(app->title));
  lv_obj_add_event_cb(card,homeAppLaunch,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(index)));
  auto* iconBox=lv_obj_create(card);lv_obj_remove_style_all(iconBox);lv_obj_set_size(iconBox,56,56);
  lv_obj_set_style_bg_color(iconBox,C_ACTIVE(),0);lv_obj_set_style_bg_opa(iconBox,LV_OPA_COVER,0);lv_obj_set_style_radius(iconBox,12,0);
  lv_obj_set_pos(iconBox,list?14:(width-56)/2,list?(height-56)/2:(labels?14:(height-56)/2));homePassive(iconBox);
  unsigned chosen=Tab5AppCatalogue::icon(index);if(!chosen)chosen=app->symbol;
  const char* symbol=homeSymbol(chosen);
  if(!strcmp(symbol,TAB5_SYMBOL_CALCULATOR))tab5CalculatorIcon(iconBox,C_TEXT());
  else if(!strcmp(symbol,TAB5_SYMBOL_PYTHON))tab5PythonIcon(iconBox,C_TEXT());
  else{auto* label=lv_label_create(iconBox);lv_label_set_text(label,symbol);lv_obj_set_style_text_font(label,&lv_font_montserrat_32,0);lv_obj_set_style_text_color(label,C_TEXT(),0);lv_obj_center(label);homePassive(label);}
  homeIconBoxes[index]=iconBox;
  if(labels||list){
    auto* title=homeLabel(card,app->title,list?86:10,list?(height-28)/2:height-40,list?width-110:width-20);
    lv_obj_set_style_text_align(title,list?LV_TEXT_ALIGN_LEFT:LV_TEXT_ALIGN_CENTER,0);
  }
}
static void renderHomeApps(){
  if(!homeAppsGrid||!homeDefaultGrid)return;
  if(homeCustomize){lv_obj_add_flag(homeNativeView,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(homePythonView,LV_OBJ_FLAG_HIDDEN);}
  else{lv_obj_remove_flag(homeNativeView,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(homePythonView,LV_OBJ_FLAG_HIDDEN);}
  if(homePythonLibrary!=(homeCustomize||homeMenu!=0)){homePythonLibrary=homeCustomize||homeMenu!=0;updateTopBarForPage();}
  uint64_t focusedKey=0;auto* focused=Tab5Focus.focused();
  for(unsigned i=0;i<Tab5AppCatalogue::MaxApps;++i)if(homeAppButtons[i]&&homeAppButtons[i]==focused){focusedKey=homeAppButtonKeys[i];break;}
  homeRenderPending=false;homeIconNext=0;memset(homeIconBoxes,0,sizeof(homeIconBoxes));memset(homeAppButtons,0,sizeof(homeAppButtons));Tab5Focus.clear();
  auto* scroll=homeCustomize?lv_obj_get_parent(homeAppsGrid):homeNativeView;
  const int32_t previous=lv_obj_get_scroll_y(scroll);
  lv_obj_clean(homeAppsGrid);lv_obj_clean(homeDefaultGrid);
  auto& config=Tab5AppCatalogue::choices();
  if(homeLayoutChoice)lv_dropdown_set_selected(homeLayoutChoice,config.list()?1:0);
  if(homeDensityChoice)lv_dropdown_set_selected(homeDensityChoice,config.density());
  if(homeLabelsChoice){
    if(config.list())lv_obj_add_flag(homeLabelsChoice,LV_OBJ_FLAG_HIDDEN);else lv_obj_remove_flag(homeLabelsChoice,LV_OBJ_FLAG_HIDDEN);
    if(config.labels())lv_obj_add_state(homeLabelsChoice,LV_STATE_CHECKED);else lv_obj_remove_state(homeLabelsChoice,LV_STATE_CHECKED);
  }
  unsigned order[Tab5AppCatalogue::MaxApps];homeAppCount=Tab5AppCatalogue::ordered(order,!homeCustomize);
  lv_obj_t* target=homeCustomize?homeAppsGrid:homeDefaultGrid;
  const Tab5AppCatalogue::Kind kinds[]={Tab5AppCatalogue::Kind::Builtin,Tab5AppCatalogue::Kind::Executable,Tab5AppCatalogue::Kind::Script};
  const char* headings[]={"Applications intégrées","Applications ELF","Applications Python"};
  int y=8;unsigned shown=0;lv_obj_t* firstApp=nullptr;
  if(!homeCustomize&&homeMenu){
    auto* back=makeIconButton(target,LV_SYMBOL_LEFT,52,48);lv_obj_set_pos(back,28,8);
    Tab5Focus.describe(back,T("Retour à l'accueil · Retour arrière"));
    lv_obj_add_event_cb(back,[](lv_event_t*){homeOpenMenu(0);},LV_EVENT_CLICKED,nullptr);
    homeLabel(target,homePageTitle(),96,20,WORK_W-124);y=78;
  }
  for(unsigned group=0;group<3;++group){
    if(!Tab5Shell::homeVisible(homeMenu,unsigned(kinds[group]),homeCustomize))continue;
    unsigned groupOrder[Tab5AppCatalogue::MaxApps],n=0;
    for(unsigned i=0;i<homeAppCount;++i)if(Tab5AppCatalogue::at(order[i])->kind==kinds[group])groupOrder[n++]=order[i];
    if(!n)continue;
    shown+=n;
    if(homeCustomize){homeSection(target,headings[group],y);y+=46;}
    const unsigned density=config.density();const bool list=config.list();
    const int gap=16,columns=list?(density==2?1:2):(density==1?5:density==2?3:4);
    const int width=(WORK_W-56-(columns-1)*gap)/columns;
    const int height=list?(density==1?70:density==2?98:82):(density==1?112:density==2?164:140);
    int firstSelected=-1,lastSelected=-1;
    for(unsigned i=0;i<n;++i)if(Tab5AppCatalogue::pinned(groupOrder[i])){if(firstSelected<0)firstSelected=int(i);lastSelected=int(i);}
    for(unsigned i=0;i<n;++i){
      if(homeCustomize)homeCustomizeRow(target,groupOrder[i],y+int(i)*92,int(i)==firstSelected,int(i)==lastSelected);
      else {homeApplicationCard(target,groupOrder[i],28+int(i%columns)*(width+gap),y+int(i/columns)*(height+gap),width,height,list,config.labels());if(!firstApp)firstApp=lv_obj_get_child(target,lv_obj_get_child_count(target)-1);}
    }
    y+=homeCustomize?int(n)*92:int((n+columns-1)/columns)*(height+gap);y+=18;
  }
  if(!shown)homeLabel(target,"Aucune application visible",28,y+12,WORK_W-56);
  lv_obj_set_height(target,y>48?y:64);
  lv_obj_update_layout(scroll);
  if(homeRestoreMenuScroll&&!homeCustomize){lv_obj_scroll_to_y(scroll,homeMenuScroll[homeMenu],LV_ANIM_OFF);homeRestoreMenuScroll=false;}
  else lv_obj_scroll_to_y(scroll,previous,LV_ANIM_OFF);
  if(homeFocusMenu&&!homeCustomize){
    auto* next=firstApp;
    if(homePendingFocus)for(unsigned i=0;i<Tab5AppCatalogue::count();++i)if(homeAppButtons[i]&&homeAppButtonKeys[i]==homePendingFocus){next=homeAppButtons[i];break;}
    Tab5Focus.focusObject(next?next:lv_obj_get_child(target,0));if(firstApp||!homeAppsScanning){homeFocusMenu=false;homePendingFocus=0;}
  }
  else if(focusedKey&&!homeCustomize){for(unsigned i=0;i<Tab5AppCatalogue::count();++i)if(homeAppButtons[i]&&Tab5AppCatalogue::at(i)->key==focusedKey){Tab5Focus.focusObject(homeAppButtons[i]);break;}}
}
static void createHomePage(){
  homeCustomize=false;homePythonLibrary=homeMenu!=0;homeAppsScanning=false;homeAppsDirty=true;homeRenderPending=true;memset(homeAppButtons,0,sizeof(homeAppButtons));
  Tab5AppCatalogue::reset();
  static_assert(sizeof(HOME_CARDS)/sizeof(HOME_CARDS[0])==sizeof(HOME_IDS)/sizeof(HOME_IDS[0]),"home IDs must match native apps");
  static_assert(sizeof(HOME_CARDS)/sizeof(HOME_CARDS[0])==sizeof(HOME_SYMBOLS)/sizeof(HOME_SYMBOLS[0]),"home symbols must match native apps");
  for(unsigned i=0;i<sizeof(HOME_CARDS)/sizeof(HOME_CARDS[0]);++i)
    Tab5AppCatalogue::addBuiltin(HOME_IDS[i],HOME_CARDS[i].title,uint8_t(HOME_CARDS[i].action),HOME_SYMBOLS[i]);
  Tab5AppCatalogue::addBuiltin("elf-apps","Applications ELF",254,6);
  Tab5AppCatalogue::addBuiltin("python-apps","Applications Python",253,24);
  homePage=lv_obj_create(pageHost);lv_obj_set_size(homePage,WORK_W,PAGE_H);Tab5Shell::width(homePage);lv_obj_set_pos(homePage,0,0);
  lv_obj_set_style_border_width(homePage,0,0);lv_obj_set_style_radius(homePage,0,0);lv_obj_set_style_pad_all(homePage,0,0);
  lv_obj_set_style_bg_color(homePage,C_BG(),0);lv_obj_remove_flag(homePage,LV_OBJ_FLAG_SCROLLABLE);
  homeNativeView=lv_obj_create(homePage);lv_obj_remove_style_all(homeNativeView);lv_obj_set_size(homeNativeView,WORK_W,PAGE_H);Tab5Shell::width(homeNativeView);homeScrollbar(homeNativeView);
  homeDefaultGrid=lv_obj_create(homeNativeView);lv_obj_remove_style_all(homeDefaultGrid);lv_obj_set_size(homeDefaultGrid,WORK_W,1);Tab5Shell::width(homeDefaultGrid);lv_obj_remove_flag(homeDefaultGrid,LV_OBJ_FLAG_SCROLLABLE);
  homePythonView=lv_obj_create(homePage);lv_obj_remove_style_all(homePythonView);lv_obj_set_size(homePythonView,WORK_W,PAGE_H);Tab5Shell::width(homePythonView);lv_obj_remove_flag(homePythonView,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(homePythonView,LV_OBJ_FLAG_HIDDEN);
  auto* back=makeIconButton(homePythonView,LV_SYMBOL_LEFT,52,48);lv_obj_set_pos(back,24,12);Tab5Focus.describe(back,T("Retour aux réglages"));
  lv_obj_add_event_cb(back,[](lv_event_t*){homeCancelCustomize();},LV_EVENT_CLICKED,nullptr);
  homeCatalogueTitle=homeLabel(homePythonView,"Personnaliser l'accueil",94,24,WORK_W-330);Tab5Shell::width(homeCatalogueTitle,-330);
  homeSaveButton=makeButton(homePythonView,T("Enregistrer"),196,48);lv_obj_set_pos(homeSaveButton,WORK_W-220,12);Tab5Shell::x(homeSaveButton,-220);lv_obj_add_event_cb(homeSaveButton,homeSaveEvent,LV_EVENT_CLICKED,nullptr);
  homeLayoutChoice=lv_dropdown_create(homePythonView);lv_obj_set_pos(homeLayoutChoice,24,82);lv_obj_set_size(homeLayoutChoice,234,48);
  String layout=String(T("Grille"))+"\n"+T("Liste");lv_dropdown_set_options(homeLayoutChoice,layout.c_str());lv_obj_set_style_text_font(homeLayoutChoice,uiFont(),0);Tab5Focus.describe(homeLayoutChoice,T("Présentation"));
  lv_obj_add_event_cb(homeLayoutChoice,[](lv_event_t* e){Tab5AppCatalogue::choices().setList(lv_dropdown_get_selected(static_cast<lv_obj_t*>(lv_event_get_target(e)))==1);homeRequestRender();},LV_EVENT_VALUE_CHANGED,nullptr);
  homeDensityChoice=lv_dropdown_create(homePythonView);lv_obj_set_pos(homeDensityChoice,276,82);lv_obj_set_size(homeDensityChoice,250,48);
  String density=String(T("Taille normale"))+"\n"+T("Taille compacte")+"\n"+T("Grande taille");lv_dropdown_set_options(homeDensityChoice,density.c_str());lv_obj_set_style_text_font(homeDensityChoice,uiFont(),0);Tab5Focus.describe(homeDensityChoice,T("Taille des applications"));
  lv_obj_add_event_cb(homeDensityChoice,[](lv_event_t* e){Tab5AppCatalogue::choices().setDensity(lv_dropdown_get_selected(static_cast<lv_obj_t*>(lv_event_get_target(e))));homeRequestRender();},LV_EVENT_VALUE_CHANGED,nullptr);
  homeLabelsChoice=lv_checkbox_create(homePythonView);lv_checkbox_set_text(homeLabelsChoice,T("Libellés"));lv_obj_set_pos(homeLabelsChoice,550,92);lv_obj_set_style_text_font(homeLabelsChoice,uiFont(),0);Tab5Focus.describe(homeLabelsChoice,T("Afficher les libellés"));
  lv_obj_add_event_cb(homeLabelsChoice,[](lv_event_t* e){Tab5AppCatalogue::choices().setLabels(lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(e)),LV_STATE_CHECKED));homeRequestRender();},LV_EVENT_VALUE_CHANGED,nullptr);
  homeResetButton=makeButton(homePythonView,T("Réinitialiser"),196,48);lv_obj_set_pos(homeResetButton,WORK_W-220,82);Tab5Shell::x(homeResetButton,-220);
  lv_obj_add_event_cb(homeResetButton,[](lv_event_t*){Tab5AppCatalogue::choices()=Tab5AppCatalogue::Choices{};homeRequestRender();},LV_EVENT_CLICKED,nullptr);
  auto* scroll=lv_obj_create(homePythonView);lv_obj_remove_style_all(scroll);lv_obj_set_pos(scroll,0,150);lv_obj_set_size(scroll,WORK_W,PAGE_H-150);Tab5Shell::width(scroll);homeScrollbar(scroll);
  homeAppsGrid=lv_obj_create(scroll);lv_obj_remove_style_all(homeAppsGrid);lv_obj_set_size(homeAppsGrid,WORK_W,1);Tab5Shell::width(homeAppsGrid);lv_obj_remove_flag(homeAppsGrid,LV_OBJ_FLAG_SCROLLABLE);
  renderHomeApps();
}
static void serviceHomeApps(){
  static Tab5AppRefresh::Schedule refresh;
  static bool previousSd=false;static uint32_t previousUsb=0;
  static int previousResult=1;
  const uint32_t now=millis();
  // App launch, page changes and settings can cancel the catalogue externally.
  if(refresh.running&&!homeAppsScanning){refresh.finished(now);homeAppsDirty=true;}
  if(!homeAppsGrid||activePage!=Page::Home||Tab5Tools.scope()||keyboardScope()!=homePage){
    if(homeAppsScanning){Tab5AppCatalogue::endScan();homeAppsScanning=false;homeAppsDirty=true;refresh.finished(now);}
    return;
  }
  const uint32_t usb=Tab5UsbStorage::revision();
  if(previousSd!=sdReady||previousUsb!=usb){previousSd=sdReady;previousUsb=usb;homeAppsDirty=true;}
  refresh.observe(now,tab5FsRevision());
  if(!homeCustomize&&refresh.due(now))homeAppsDirty=true;
  const uint32_t before=Tab5AppCatalogue::revision();
  if(homeAppsDirty){
    homeAppsDirty=false;refresh.started(now);homeAppsScanning=Tab5AppCatalogue::beginScan(sdReady);
    if(!homeAppsScanning)refresh.finished(now);
  }
  if(homeAppsScanning){
    const int result=Tab5AppCatalogue::scanStep();
    if(result){homeAppsScanning=false;refresh.finished(now);
      if(result!=previousResult){
        if(result<0)toast("Lecture du stockage interrompue",2800);
        else if(result==2)toast("Autres applications disponibles dans l'Explorateur",2800);
      }
      previousResult=result;
    }
  }
  if(Tab5AppCatalogue::revision()!=before)homeRequestRender();
  if(homeRenderPending)renderHomeApps();
  if((sdReady||Tab5UsbStorage::mounted())&&!homeAppsScanning&&!homeCustomize){
    while(homeIconNext<Tab5AppCatalogue::count()){
      unsigned index=homeIconNext++;if(!homeIconBoxes[index])continue;
      Tab5AppCatalogue::attachIcon(homeIconBoxes[index],index);break;
    }
  }
}
