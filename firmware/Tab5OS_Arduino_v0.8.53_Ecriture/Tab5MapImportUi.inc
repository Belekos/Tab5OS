// Included after the map overlay helper. All state and SD access stay on the UI
// task; downloads use the existing map worker, OS network lease and PSRAM quota.
static Tab5MapImport::Plan mapImportPlan;
static lv_obj_t *mapImportSource=nullptr,*mapImportLow=nullptr,*mapImportHigh=nullptr,
    *mapImportRate=nullptr,*mapImportSummary=nullptr,*mapImportStart=nullptr,
    *mapImportPause=nullptr,*mapImportProgress=nullptr,*mapImportRateValue=nullptr;
static bool mapImportRunning=false,mapImportWaiting=false,mapImportHaveTile=false,
    mapImportRestore=false;
static uint8_t mapImportZ=0,mapImportView=0;
static uint32_t mapImportX=0,mapImportY=0,mapImportDone=0,mapImportStored=0,
    mapImportLastUi=0,mapImportWaitStart=0;
static double mapImportLatitude=0,mapImportLongitude=0;
static int mapImportWidth=0,mapImportHeight=0;
static const char* mapImportMessage="";

static void updateMapImportUi() {
  if(!mapImportSummary)return;
  const size_t style=Tab5Map.selectedStyle();
  const int low=lv_slider_get_value(mapImportLow),high=lv_slider_get_value(mapImportHigh);
  if(!mapImportRunning){
    mapImportPlan.prepare(mapImportLatitude,mapImportLongitude,mapImportView,mapImportWidth,mapImportHeight,
        low,high,Tab5Map.styleMaxZoom(style),Tab5Map.styleZoomShift(style));
  }
  char zooms[96];
  if(mapImportRunning||mapImportDone)snprintf(zooms,sizeof(zooms),"z%d – z%d   ·   %lu / %llu",low,high,
      static_cast<unsigned long>(mapImportDone),static_cast<unsigned long long>(mapImportPlan.count));
  else snprintf(zooms,sizeof(zooms),"z%d – z%d   ·   %llu",low,high,static_cast<unsigned long long>(mapImportPlan.count));
  String summary=String(T("Zone affichée"))+"\n"+zooms+" "+T("tuiles");
  if(!Tab5Map.styleImportAllowed(style))summary+="\n"+String(T("Cette source autorise la consultation uniquement. Choisir une autre source pour importer."));
  else if(mapImportPlan.count>50000)summary+="\n"+String(T("Zone trop grande : réduire les détails ou rapprocher la carte."));
  else if(mapImportMessage[0])summary+="\n"+String(T(mapImportMessage));
  lv_label_set_text(mapImportSummary,summary.c_str());
  const unsigned requested=lv_slider_get_value(mapImportRate);
  const unsigned effective=Tab5Map.importEffectiveRateTenths(style,requested);
  char rate[64];snprintf(rate,sizeof(rate),"%u.%u / s",requested/10,requested%10);
  String rateText=rate;
  if(effective<requested){snprintf(rate,sizeof(rate)," : %u.%u / s",effective/10,effective%10);rateText+=String(" · ")+T("Limite de la source")+rate;}
  lv_label_set_text(mapImportRateValue,rateText.c_str());
  lv_bar_set_value(mapImportProgress,mapImportPlan.count?int(uint64_t(mapImportDone)*1000/mapImportPlan.count):0,LV_ANIM_OFF);
  const bool allowed=Tab5Map.styleImportAllowed(style)&&mapImportPlan.count&&mapImportPlan.count<=50000;
  if(mapImportRunning || !allowed)lv_obj_add_state(mapImportStart,LV_STATE_DISABLED);else lv_obj_remove_state(mapImportStart,LV_STATE_DISABLED);
  if(mapImportRunning)lv_obj_remove_flag(mapImportPause,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(mapImportPause,LV_OBJ_FLAG_HIDDEN);
  for(auto* control:{mapImportSource,mapImportLow,mapImportHigh,mapImportRate}){
    if(mapImportRunning)lv_obj_add_state(control,LV_STATE_DISABLED);else lv_obj_remove_state(control,LV_STATE_DISABLED);
  }
}
static void pauseMapImport(const char* message) {
  mapImportRunning=mapImportWaiting=mapImportHaveTile=false;mapImportMessage=message;
  ++mapGeneration;Tab5Map.beginGeneration(mapGeneration);updateMapImportUi();
}
static void mapImportDeleted(lv_event_t* event) {
  if(lv_event_get_target_obj(event)!=lv_event_get_current_target_obj(event))return;
  // Handles Close, Escape, navigation and app teardown identically. No retained
  // widget pointer is touched after its ancestor begins deletion.
  mapDownloadOverlay=nullptr;
  mapImportSource=mapImportLow=mapImportHigh=mapImportRate=mapImportSummary=nullptr;
  mapImportStart=mapImportPause=mapImportProgress=mapImportRateValue=nullptr;
  mapImportRunning=mapImportWaiting=mapImportHaveTile=false;
  ++mapGeneration;Tab5Map.beginGeneration(mapGeneration);mapImportRestore=true;
}
static void mapImportRateEvent(lv_event_t* event) {
  const auto code=lv_event_get_code(event);
  if(code!=LV_EVENT_VALUE_CHANGED&&code!=LV_EVENT_RELEASED)return;
  // This setter is only reached on release or keyboard changes. Drag previews
  // need not write NVS repeatedly; the effective engine limit is set at Start.
  if(code==LV_EVENT_RELEASED || !lv_indev_active() || lv_indev_get_type(lv_indev_active())!=LV_INDEV_TYPE_POINTER)
    if(!Tab5Map.setImportRateTenths(lv_slider_get_value(mapImportRate)))toast("Réglage non enregistré");
  updateMapImportUi();
}
static void mapImportOptionEvent(lv_event_t* event) {
  auto* target=lv_event_get_target_obj(event);mapImportMessage="";mapImportDone=mapImportStored=0;
  if(target==mapImportSource){
    Tab5Map.selectStyle(lv_dropdown_get_selected(mapImportSource));
    for(auto* slider:{mapImportLow,mapImportHigh})lv_slider_set_range(slider,
        Tab5Map.styleMinZoom(Tab5Map.selectedStyle()),Tab5Map.styleViewMaxZoom(Tab5Map.selectedStyle()));
  }
  if(lv_slider_get_value(mapImportLow)>lv_slider_get_value(mapImportHigh)){
    if(target==mapImportLow)lv_slider_set_value(mapImportHigh,lv_slider_get_value(mapImportLow),LV_ANIM_OFF);
    else lv_slider_set_value(mapImportLow,lv_slider_get_value(mapImportHigh),LV_ANIM_OFF);
  }
  updateMapImportUi();
}
static void mapImportStartEvent(lv_event_t*) {
  bool connected=false;
  if(!sdReady||!Tab5Map.storageAvailable()){toast("microSD absente");return;}
  const bool allowed=mapDownloadsAllowed(&connected);
  if(!connected){toast("Wi-Fi disponible requis pour actualiser les tuiles",2600);return;}
  if(!allowed){toast("Téléchargement indisponible : ressources occupées",2600);return;}
  if(!Tab5Map.setImportRateTenths(lv_slider_get_value(mapImportRate)))toast("Réglage non enregistré");
  updateMapImportUi();
  if(!Tab5Map.styleImportAllowed(Tab5Map.selectedStyle())||!mapImportPlan.count||mapImportPlan.count>50000)return;
  ++mapGeneration;Tab5Map.beginGeneration(mapGeneration);mapRasterCount=mapRasterIndex=0;
  mapImportPlan.reset();mapImportDone=mapImportStored=0;mapImportHaveTile=mapImportWaiting=false;
  mapImportRunning=true;mapImportMessage="Téléchargement en cours";updateMapImportUi();
}
static bool consumeMapImportResult(Tab5MapTileResult& result) {
  if(!mapImportRunning||result.generation!=mapGeneration)return false;
  mapImportWaiting=false;
  if(!result.success){
    pauseMapImport(result.httpCode==403||result.httpCode==429 ? "Le serveur limite les téléchargements. Réessayer plus tard." : "Téléchargement interrompu. Les tuiles enregistrées sont conservées.");
  }else if(!Tab5Map.storeTile(result)){
    pauseMapImport("Enregistrement impossible. Les tuiles enregistrées sont conservées.");
  }else{++mapImportDone;++mapImportStored;mapImportHaveTile=false;}
  return true;
}
static void serviceMapImport(bool storageBusy) {
  if(mapImportRestore){mapImportRestore=false;if(activePage==Page::Map){clampMapView();updateMapAttribution();renderMapViewport(false);}}
  if(!mapImportRunning || !mapDownloadOverlay)return;
  if(!sdReady||!Tab5Map.storageAvailable()){pauseMapImport("microSD absente");return;}
  if(mapImportWaiting)return;
  if(storageBusy)return;
  // At most four cache probes per UI pass; never scan a whole region in LVGL.
  for(unsigned i=0;i<4&&!mapImportWaiting;++i){
    if(!mapImportHaveTile){
      if(!mapImportPlan.next(mapImportZ,mapImportX,mapImportY)){
        pauseMapImport("Téléchargement terminé");return;
      }
      mapImportHaveTile=true;mapImportWaitStart=millis();
    }
    if(Tab5Map.cachedTileExists(mapImportZ,mapImportX,mapImportY)){++mapImportDone;mapImportHaveTile=false;continue;}
    if(Tab5Map.requestTile(mapImportZ,mapImportX,mapImportY,mapGeneration,true))mapImportWaiting=true;
    else if(uint32_t(millis()-mapImportWaitStart)>3000){pauseMapImport("Téléchargement indisponible : ressources occupées");return;}
  }
  if(uint32_t(millis()-mapImportLastUi)>=250){mapImportLastUi=millis();updateMapImportUi();}
}
static void mapDownloadEvent(lv_event_t*) {
  if(mapDownloadOverlay)return;
  mapImportLatitude=mapLatitude;mapImportLongitude=mapLongitude;mapImportView=mapZoom;
  mapImportWidth=MAP_VIEW_W;mapImportHeight=MAP_VIEW_H;mapImportMessage="";mapImportDone=mapImportStored=0;
  auto* box=createMapOverlay(mapDownloadOverlay,"Télécharger la zone",960,630);
  lv_obj_add_event_cb(mapDownloadOverlay,mapImportDeleted,LV_EVENT_DELETE,nullptr);
  auto caption=[&](const char* text,int y){auto* o=lv_label_create(box);lv_label_set_text(o,T(text));lv_obj_set_pos(o,4,y);lv_obj_set_width(o,260);lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_color(o,C_TEXT(),0);return o;};
  caption("Source",62);mapImportSource=lv_dropdown_create(box);lv_obj_set_pos(mapImportSource,280,50);lv_obj_set_size(mapImportSource,624,52);styleSettingsDropdown(mapImportSource);
  String styles;for(size_t i=0;i<Tab5Map.styleCount();++i){if(i)styles+='\n';styles+=Tab5Map.styleName(i);}
  lv_dropdown_set_options(mapImportSource,styles.c_str());lv_dropdown_set_selected(mapImportSource,Tab5Map.selectedStyle());
  lv_obj_add_event_cb(mapImportSource,mapImportOptionEvent,LV_EVENT_VALUE_CHANGED,nullptr);
  caption("Zoom minimum",125);caption("Zoom maximum",180);caption("Requêtes par seconde",241);
  auto slider=[&](int y,int low,int high,int value){auto* o=lv_slider_create(box);lv_obj_set_pos(o,292,y);lv_obj_set_size(o,594,22);styleSettingsSlider(o);lv_slider_set_range(o,low,high);lv_slider_set_value(o,value,LV_ANIM_OFF);return o;};
  const auto minimum=Tab5Map.styleMinZoom(Tab5Map.selectedStyle());
  const auto maximum=Tab5Map.styleViewMaxZoom(Tab5Map.selectedStyle());
  mapImportLow=slider(128,minimum,maximum,mapZoom>3?mapZoom-3:0);mapImportHigh=slider(183,minimum,maximum,mapZoom);
  mapImportRate=slider(246,1,40,Tab5Map.importRateTenths());
  for(auto* o:{mapImportLow,mapImportHigh})lv_obj_add_event_cb(o,mapImportOptionEvent,LV_EVENT_VALUE_CHANGED,nullptr);
  lv_obj_add_event_cb(mapImportRate,mapImportRateEvent,LV_EVENT_ALL,nullptr);
  mapImportRateValue=lv_label_create(box);lv_obj_set_pos(mapImportRateValue,280,284);lv_obj_set_size(mapImportRateValue,624,48);lv_label_set_long_mode(mapImportRateValue,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_color(mapImportRateValue,C_TEXT(),0);
  mapImportSummary=lv_label_create(box);lv_obj_set_pos(mapImportSummary,4,343);lv_obj_set_size(mapImportSummary,902,148);lv_label_set_long_mode(mapImportSummary,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_color(mapImportSummary,C_TEXT(),0);
  mapImportProgress=lv_bar_create(box);lv_obj_set_pos(mapImportProgress,4,503);lv_obj_set_size(mapImportProgress,900,12);lv_bar_set_range(mapImportProgress,0,1000);
  lv_obj_set_style_bg_color(mapImportProgress,C_BORDER(),LV_PART_MAIN);lv_obj_set_style_bg_color(mapImportProgress,C_ACTIVE(),LV_PART_INDICATOR);
  auto* close=makeButton(box,"Fermer",180,52);lv_obj_set_pos(close,0,540);
  lv_obj_add_event_cb(close,[](lv_event_t*){auto* old=mapDownloadOverlay;mapDownloadOverlay=nullptr;if(old)lv_obj_delete(old);},LV_EVENT_CLICKED,nullptr);
  mapImportPause=makeButton(box,"Pause",220,52);lv_obj_set_pos(mapImportPause,250,540);lv_obj_add_event_cb(mapImportPause,[](lv_event_t*){pauseMapImport("En pause");},LV_EVENT_CLICKED,nullptr);
  mapImportStart=makeButton(box,"Télécharger / reprendre",388,52);lv_obj_set_pos(mapImportStart,520,540);
  lv_obj_add_event_cb(mapImportStart,mapImportStartEvent,LV_EVENT_CLICKED,nullptr);
  Tab5Focus.describe(mapImportRate,T("Requêtes par seconde"));
  Tab5Focus.describe(mapImportLow,T("Zoom minimum"));Tab5Focus.describe(mapImportHigh,T("Zoom maximum"));
  Tab5Focus.describe(mapImportSource,T("Source"));
  ++mapGeneration;Tab5Map.beginGeneration(mapGeneration);mapRasterCount=mapRasterIndex=0;
  updateMapImportUi();Tab5Focus.focusInitial(mapDownloadOverlay);
}
