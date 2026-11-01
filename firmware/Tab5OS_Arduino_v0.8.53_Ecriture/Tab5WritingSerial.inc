// Included inside Tab5WritingApp::Impl. The actual paged document owns TX;
// the old compatibility textarea is deliberately not involved.
bool serialVisible=false,serialListening=false,serialWasSending=false,serialModePending=false;
uint32_t serialPaintAt=0,serialPaintRevision=UINT32_MAX,serialDropAt=0;
Tab5Serial::TextCapture* serialCapture=nullptr;
lv_obj_t *serialPane=nullptr,*serialText=nullptr,*serialReceiveBox=nullptr,
 *serialListenButton=nullptr,*serialStopButton=nullptr,*serialSendButton=nullptr,
 *serialCancelButton=nullptr,*serialInsertButton=nullptr,*serialDataButton=nullptr,*serialClearButton=nullptr,
 *serialStatus=nullptr,*serialWarning=nullptr,*serialSource=nullptr;

void serialRange(size_t& from,size_t& to)const{
 if(documentSelection){from=std::min(selectionAnchor,selectionCaret);to=std::max(selectionAnchor,selectionCaret);}
 else {from=start+view.text.sourceAt(std::min(view.anchor,view.caret));to=start+view.text.sourceAt(std::max(view.anchor,view.caret));}
 from=std::min(from,document.size());to=std::min(to,document.size());
 if(from==to){from=0;to=document.size();}
}
void serialSend(){
 size_t from=0,to=0;serialRange(from,to);
 if(from==to){note("Aucun texte à envoyer.");return;}
 if(to-from>Tab5Serial::MaxSnapshot){note("Texte trop volumineux pour l’envoi série.");return;}
 if(Tab5Serial::sendSnapshot(to-from,[this,from](char* output,size_t n){document.copy(from,output,n);},Tab5Serial::Client::Editor))serialWasSending=true;
 else note("Série : envoi impossible ou déjà en cours.");
 serialService(millis(),true);
}
void serialStopSession(){
 if(serialListening){Tab5Serial::releaseReceive(Tab5Serial::Client::Editor);serialListening=false;if(serialCapture)serialCapture->finish();}
 Tab5Serial::cancelSend(Tab5Serial::Client::Editor);serialWasSending=false;
}
void serialToggle(){
 serialVisible=!serialVisible;
 if(serialVisible){consoleVisible=false;closeMenu(false);}
 lastLayoutHeight=-1;layout();serialService(millis(),true);
}
void serialAction(int action){
 using namespace Tab5Serial;
 if(action==SerialListen){
  if(!serialCapture){void* memory=writing::allocate(sizeof(TextCapture));if(memory)serialCapture=new(memory)TextCapture;}
  if(!serialCapture){note("Mémoire insuffisante pour la réception série.");return;}
  if(!claimReceive(Tab5Serial::Client::Editor)){note("Série occupée par une autre session.");return;}
  serialListening=true;serialDropAt=stats().dropped;
 }else if(action==SerialStop){releaseReceive(Tab5Serial::Client::Editor);serialListening=false;if(serialCapture)serialCapture->finish();}
 else if(action==SerialSend){serialSend();return;}
 else if(action==SerialCancel){cancelSend(Tab5Serial::Client::Editor);serialWasSending=false;note("Envoi annulé");}
 else if(action==SerialClear){if(serialCapture)serialCapture->clear();serialDropAt=stats().dropped;lv_label_set_text_selection_start(serialText,LV_DRAW_LABEL_NO_TXT_SEL);lv_label_set_text_selection_end(serialText,LV_DRAW_LABEL_NO_TXT_SEL);lv_label_set_text(serialText,"");serialPaintRevision=UINT32_MAX;lv_obj_scroll_to_y(serialReceiveBox,0,LV_ANIM_OFF);}
 else if(action==SerialInsert&&serialCapture&&serialCapture->size){
  if(documentSelection)replaceSelection(serialCapture->data);else view.insert(serialCapture->data);
 }else if(action==SerialData){auto next=config();next.mode=Mode::Data;if(requestConfigure(next))serialModePending=true;else note("Série : réglage impossible ou envoi en cours.");}
 serialService(millis(),true);
}
void serialBuild(){
 serialPane=lv_obj_create(body);panel(serialPane,true);lv_obj_add_style(serialPane,&referenceStyle,0);
 lv_obj_set_style_pad_all(serialPane,8,0);lv_obj_set_style_pad_row(serialPane,4,0);
 lv_obj_set_flex_flow(serialPane,LV_FLEX_FLOW_COLUMN);lv_obj_set_scroll_dir(serialPane,LV_DIR_VER);
 auto row=[&](){auto* item=lv_obj_create(serialPane);panel(item);lv_obj_set_size(item,lv_pct(100),LV_SIZE_CONTENT);lv_obj_set_style_min_height(item,36,0);lv_obj_set_flex_flow(item,LV_FLEX_FLOW_ROW_WRAP);lv_obj_set_style_pad_column(item,6,0);lv_obj_set_style_pad_row(item,4,0);return item;};
 auto control=[&](lv_obj_t* parent,const char* text,int action){auto* item=button(parent,text,action,100);lv_obj_set_height(item,36);Tab5UiStyle::button(item);Tab5Focus.describe(item,tr(text));return item;};
 auto* heading=row();auto* title=label(heading,"Série",0,0,300);lv_obj_set_flex_grow(title,1);lv_obj_set_style_text_font(title,font(20),0);
 auto* close=button(heading,LV_SYMBOL_MINUS,SerialToggle,42,true);lv_obj_set_height(close,32);Tab5Focus.describe(close,tr("Réduire"));
 serialStatus=label(serialPane,"",0,0,400);lv_obj_set_width(serialStatus,lv_pct(100));lv_label_set_long_mode(serialStatus,LV_LABEL_LONG_DOT);lv_obj_set_style_text_font(serialStatus,font(16),0);lv_obj_set_height(serialStatus,42);
 auto* actions=row();serialListenButton=control(actions,"Écouter",SerialListen);serialStopButton=control(actions,"Arrêter",SerialStop);serialDataButton=control(actions,"Mode Données",SerialData);
 actions=row();serialSendButton=control(actions,"Envoyer",SerialSend);serialCancelButton=control(actions,"Annuler",SerialCancel);Tab5Focus.describe(serialCancelButton,tr("Annuler l'envoi"));
 serialSource=label(serialPane,"",0,0,400);lv_obj_set_width(serialSource,lv_pct(100));lv_obj_set_style_text_font(serialSource,font(16),0);lv_obj_set_height(serialSource,20);lv_label_set_long_mode(serialSource,LV_LABEL_LONG_DOT);
 serialReceiveBox=lv_obj_create(serialPane);panel(serialReceiveBox,true);lv_obj_set_width(serialReceiveBox,lv_pct(100));lv_obj_set_height(serialReceiveBox,0);lv_obj_set_flex_grow(serialReceiveBox,1);lv_obj_set_style_min_height(serialReceiveBox,48,0);lv_obj_set_scroll_dir(serialReceiveBox,LV_DIR_VER);
 serialText=lv_label_create(serialReceiveBox);lv_label_set_text(serialText,"");lv_obj_set_width(serialText,lv_pct(100));lv_obj_set_style_text_font(serialText,font(18),0);Tab5TextInput::selectable(serialText);Tab5Focus.describe(serialText,tr("Réception série"));
 serialWarning=label(serialPane,"",0,0,400);lv_obj_set_width(serialWarning,lv_pct(100));lv_obj_set_style_text_font(serialWarning,font(16),0);lv_obj_set_height(serialWarning,20);lv_label_set_long_mode(serialWarning,LV_LABEL_LONG_DOT);
 actions=row();serialInsertButton=control(actions,"Insérer",SerialInsert);serialClearButton=control(actions,"Effacer",SerialClear);Tab5Focus.describe(serialClearButton,tr("Effacer la réception"));
 serialFitControls();
 lv_obj_add_flag(serialPane,LV_OBJ_FLAG_HIDDEN);
}
void serialFitControls(){
 // Size each translated caption; wrapping rows keep long translations within
 // the 438px pane instead of truncating them into equal-width columns.
 for(auto* item:{serialListenButton,serialStopButton,serialDataButton,serialSendButton,serialCancelButton,serialInsertButton,serialClearButton}){
  if(!item)continue;
  auto* caption=lv_obj_get_child(item,0);lv_point_t extent{};
  lv_text_get_size(&extent,lv_label_get_text(caption),font(20),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
  const int width=std::max(68,std::min(422,int(extent.x)+24));
  lv_obj_set_width(item,width);lv_obj_set_width(caption,width-24);
  lv_label_set_long_mode(caption,LV_LABEL_LONG_WRAP);
  lv_text_get_size(&extent,lv_label_get_text(caption),font(20),0,0,width-24,LV_TEXT_FLAG_NONE);
  lv_obj_set_height(item,std::max(36,int(extent.y)+12));
 }
}
void serialLayout(int mainWidth){
 if(!serialPane)return;lv_obj_set_pos(serialPane,mainWidth+8,0);lv_obj_set_size(serialPane,438,std::max(96,height-74));
 if(serialVisible)lv_obj_remove_flag(serialPane,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(serialPane,LV_OBJ_FLAG_HIDDEN);
}
void serialService(uint32_t now,bool force=false){
 if(!visible||!serialPane)return;
 auto state=Tab5Serial::stats();
 if(serialModePending&&!state.configuring){serialModePending=false;if(!state.ready||state.config.mode!=Tab5Serial::Mode::Data)note("Série : réglage impossible ou envoi en cours.");}
 if(serialListening&&serialCapture){
  if(state.receiver!=Tab5Serial::Client::Editor||state.config.mode!=Tab5Serial::Mode::Data){serialListening=false;serialCapture->finish();}
  else {char bytes[512];for(unsigned chunk=0;chunk<4;++chunk){size_t n=Tab5Serial::read(bytes,sizeof(bytes),Tab5Serial::Client::Editor);if(!n)break;serialCapture->feed(bytes,n);}}
 }
 if(!force&&uint32_t(now-serialPaintAt)<250)return;serialPaintAt=now;
 const bool sending=state.pending&&state.sender==Tab5Serial::Client::Editor;
 if(serialWasSending&&!sending){serialWasSending=false;note("Envoi terminé");}
 auto enabled=[](lv_obj_t* obj,bool on){if(on)lv_obj_remove_state(obj,LV_STATE_DISABLED);else lv_obj_add_state(obj,LV_STATE_DISABLED);};
 auto* indicator=menuButtons[MenuView-MenuFile];
 if(indicator){if(serialListening||sending)lv_obj_add_state(indicator,LV_STATE_CHECKED);else lv_obj_remove_state(indicator,LV_STATE_CHECKED);}
 if(!serialVisible)return;
 if(force)serialFitControls();
 bool data=state.ready&&!state.configuring&&state.config.mode==Tab5Serial::Mode::Data;
 enabled(serialListenButton,data&&!serialListening&&state.receiver==Tab5Serial::Client::None);
 enabled(serialStopButton,serialListening);enabled(serialDataButton,!state.configuring&&!state.pending&&state.config.mode!=Tab5Serial::Mode::Data);
 if(state.config.mode==Tab5Serial::Mode::Data&&!state.configuring)lv_obj_add_flag(serialDataButton,LV_OBJ_FLAG_HIDDEN);else lv_obj_remove_flag(serialDataButton,LV_OBJ_FLAG_HIDDEN);
 size_t from=0,to=0;serialRange(from,to);enabled(serialSendButton,data&&!state.pending&&to>from&&to-from<=Tab5Serial::MaxSnapshot);enabled(serialCancelButton,sending);enabled(serialInsertButton,serialCapture&&serialCapture->size);
 enabled(serialClearButton,serialCapture&&serialCapture->size);
 char info[256];snprintf(info,sizeof(info),"%s · %s · %s\nRX %lu  TX %lu  +%lu",state.config.port==Tab5Serial::Port::Usb?"USB-C":"RS485",tr(state.config.mode==Tab5Serial::Mode::Data?"Données":"Débogage"),tr(serialListening?"En écoute":state.connected?"Connecté":"Déconnecté"),static_cast<unsigned long>(state.received),static_cast<unsigned long>(state.sent),static_cast<unsigned long>(state.pending));
 if(strcmp(lv_label_get_text(serialStatus),info))lv_label_set_text(serialStatus,info);
 snprintf(info,sizeof(info),"%s · %lu %s",tr((documentSelection&&selectionAnchor!=selectionCaret)||view.anchor!=view.caret?"Sélection":"Document entier"),static_cast<unsigned long>(to-from),tr("octets"));if(strcmp(lv_label_get_text(serialSource),info))lv_label_set_text(serialSource,info);
 const uint32_t dropped=state.dropped-serialDropAt;
 size_t previewAt=serialCapture&&serialCapture->size>4096?serialCapture->size-4096:0;
 while(serialCapture&&previewAt<serialCapture->size&&(uint8_t(serialCapture->data[previewAt])&0xc0)==0x80)++previewAt;
 const char* warning=serialCapture&&(serialCapture->trimmed||dropped)?"Réception tronquée":serialCapture&&serialCapture->invalid?"Octets non textuels remplacés":previewAt?"Affichage partiel":"";
 if(*warning)snprintf(info,sizeof(info),"%s · %lu",tr(warning),static_cast<unsigned long>(serialCapture->trimmed||dropped?serialCapture->trimmed+dropped:serialCapture->invalid?serialCapture->invalid:previewAt));else info[0]=0;
 if(strcmp(lv_label_get_text(serialWarning),info))lv_label_set_text(serialWarning,info);
 if(serialCapture&&serialCapture->revision!=serialPaintRevision&&!Tab5TextInput::hasSelection(serialText)){
  serialPaintRevision=serialCapture->revision;
  if(strcmp(lv_label_get_text(serialText),serialCapture->data+previewAt)){
   lv_obj_update_layout(serialReceiveBox);bool tail=lv_obj_get_scroll_bottom(serialReceiveBox)<24;
   lv_label_set_text(serialText,serialCapture->data+previewAt);
   if(tail)lv_obj_scroll_to_y(serialReceiveBox,LV_COORD_MAX,LV_ANIM_OFF);
  }
 }
}
