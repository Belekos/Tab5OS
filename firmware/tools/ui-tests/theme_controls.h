static bool bg(lv_obj_t* object,lv_part_t part,uint32_t value){return lv_color_to_u32(lv_obj_get_style_bg_color(object,part))==lv_color_to_u32(lv_color_hex(value));}
static void themeChecks(lv_obj_t* screen){
 const char* legacy[]={"#101010","#202020","#303030","#404040","#F0F0F0","#A0A0A0","#4488AA","#FF0000","#557799",nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
 uint32_t parsed[tab5theme::PaletteCount];
 check(tab5theme::readPalette([&](unsigned i){return legacy[i];},parsed),"legacy nine-role theme remains readable");
 check(parsed[11]==0x303030&&parsed[12]==0x4488aa,"legacy switches inherit their own theme relief and accent");
 legacy[9]="#345678";legacy[10]="#ABCDEF";
 check(tab5theme::readPalette([&](unsigned i){return legacy[i];},parsed)&&parsed[9]==0x345678&&parsed[10]==0xabcdef,"eleven-role pointer colors retained");
 legacy[11]="#000000";legacy[12]="#FFFFFF";
 check(tab5theme::readPalette([&](unsigned i){return legacy[i];},parsed)&&parsed[11]==0&&parsed[12]==0xffffff,"black OFF and white ON are explicit colors");
 check(parsed[13]==parsed[4]&&parsed[14]==parsed[1],"thirteen-role 0.8.20 themes preserve both former knob colors");
 legacy[13]="#FFFFFF";legacy[14]="#000000";
 check(tab5theme::readPalette([&](unsigned i){return legacy[i];},parsed)&&parsed[13]==0xffffff&&parsed[14]==0,"new knob colors accept independent white OFF and black ON");
 std::string serialized[tab5theme::PaletteCount];unsigned writes=0;
 tab5theme::writePalette(parsed,[&](const char* key,const char* value){for(unsigned i=0;i<tab5theme::PaletteCount;++i)if(!std::strcmp(key,tab5theme::roleKey(i))){serialized[i]=value;++writes;}});
 uint32_t restored[tab5theme::PaletteCount]{};
 check(writes==15&&serialized[11]=="#000000"&&serialized[12]=="#FFFFFF"&&serialized[13]=="#FFFFFF"&&serialized[14]=="#000000","saved palette contains all four independent switch keys including extreme colors");
 check(tab5theme::readPalette([&](unsigned i){return serialized[i].c_str();},restored)&&!std::memcmp(parsed,restored,sizeof(restored)),"saved fifteen-role theme survives reload unchanged");
 uint32_t saved[tab5theme::PaletteCount];std::memcpy(saved,parsed,sizeof(saved));legacy[12]="#xyz123";
 check(!tab5theme::readPalette([&](unsigned i){return legacy[i];},parsed)&&!std::memcmp(saved,parsed,sizeof(saved)),"malformed supplied switch color rejects theme without partial mutation");
 for(const char* invalid:{"","#","#12","#12345","#1234567","123456","#00000Z"}){uint32_t output=0x123456;check(!tab5theme::parseColor(invalid,output)&&output==0x123456,"strict color parsing preserves destination");}
 auto* toggle=lv_switch_create(screen);lv_obj_set_style_transition(toggle,nullptr,LV_STATE_ANY);
 // Existing Settings supplied its own checked main fill; both layers must be replaced.
 lv_obj_set_style_bg_color(toggle,lv_color_hex(0xff0000),LV_PART_MAIN|LV_STATE_CHECKED);
 lv_obj_set_style_bg_color(toggle,lv_color_hex(0xff0000),LV_PART_KNOB|LV_STATE_CHECKED);
 Tab5UiStyle::configure(saved,12,saved[11],saved[12],saved[13],saved[14]);Tab5UiStyle::control(toggle);
 check(bg(toggle,LV_PART_MAIN,0)&&bg(toggle,LV_PART_INDICATOR,0),"custom switch OFF paints both track layers");
 check(bg(toggle,LV_PART_KNOB,0xffffff),"OFF knob independently paints white on black track");
 lv_obj_add_state(toggle,LV_STATE_CHECKED);check(bg(toggle,LV_PART_MAIN,0xffffff)&&bg(toggle,LV_PART_INDICATOR,0xffffff),"custom switch ON replaces prior local Settings style");
 check(bg(toggle,LV_PART_KNOB,0),"ON knob independently paints black on white track replacing local Settings style");
 Tab5UiStyle::configure(saved,24,0x995511,0x11aa55,0xff0000,0x0000ff);Tab5UiStyle::tree(screen);
 check(lv_obj_has_state(toggle,LV_STATE_CHECKED)&&bg(toggle,LV_PART_MAIN,0x11aa55)&&bg(toggle,LV_PART_INDICATOR,0x11aa55),"live theme update preserves ON state and changes both layers");
 check(bg(toggle,LV_PART_KNOB,0x0000ff),"live theme update changes ON knob independently");
 lv_obj_remove_state(toggle,LV_STATE_CHECKED);check(bg(toggle,LV_PART_MAIN,0x995511)&&bg(toggle,LV_PART_INDICATOR,0x995511),"updated OFF color visible after toggling");
 check(bg(toggle,LV_PART_KNOB,0xff0000),"live theme update changes OFF knob independently");
 // The thumb edge survives every state, radius and low-contrast custom fill.
 for(auto fill:{0x000000u,0xffffffu,0x808080u,0x4488aau})for(unsigned radius:{0u,12u,24u}){
  Tab5UiStyle::configure(saved,radius,fill,fill,fill,fill);Tab5UiStyle::control(toggle);
  for(bool on:{false,true}){
   if(on)lv_obj_add_state(toggle,LV_STATE_CHECKED);else lv_obj_remove_state(toggle,LV_STATE_CHECKED);
   check(lv_obj_get_style_border_width(toggle,LV_PART_KNOB)==2&&lv_obj_get_style_border_opa(toggle,LV_PART_KNOB)==LV_OPA_COVER,"switch thumb has opaque2px edge in both states");
   check(lv_color_to_u32(lv_obj_get_style_border_color(toggle,LV_PART_KNOB))!=lv_color_to_u32(lv_color_hex(fill)),"thumb border remains distinguishable when its fill equals track");
   check(lv_obj_get_style_radius(toggle,LV_PART_KNOB)==radius,"thumb edge follows configured theme corner radius");
  }
 }
 Tab5UiStyle::configure(saved,24,0x995511,0x11aa55,0xff0000,0x0000ff);Tab5UiStyle::control(toggle);lv_obj_remove_state(toggle,LV_STATE_CHECKED);
 check(Tab5UiStyle::color(11)==0x995511&&Tab5UiStyle::color(12)==0x11aa55,"shared controls receive switch colors through palette accessor");
 check(Tab5UiStyle::color(13)==0xff0000&&Tab5UiStyle::color(14)==0x0000ff,"shared controls receive knob colors through palette accessor");
 Tab5UiStyle::configure(saved,12);Tab5UiStyle::tree(screen);
 check(bg(toggle,LV_PART_MAIN,saved[2])&&bg(toggle,LV_PART_KNOB,saved[4]),"legacy configure resets OFF to current theme relief and text");lv_obj_add_state(toggle,LV_STATE_CHECKED);
 check(bg(toggle,LV_PART_INDICATOR,saved[6])&&bg(toggle,LV_PART_KNOB,saved[1]),"legacy configure resets ON to current theme accent and surface");
 const char* names[]={"Graphite","Bleu ardoise","Ambre doux","Sauge","Vert lecture","Mon thème"};
 struct Catalogue{size_t count;const char** names;};Catalogue catalogue{5,names};
 auto* choices=lv_dropdown_create(screen);Tab5UiStyle::control(choices);
 lv_obj_add_event_cb(choices,[](lv_event_t* e){auto* c=static_cast<Catalogue*>(lv_event_get_user_data(e));Tab5ThemeChoices::populate(lv_event_get_current_target_obj(e),c->count,[&](size_t i){return c->names[i];},[](const char* name){return name;},"Thème actuel");},LV_EVENT_READY,&catalogue);
 lv_dropdown_open(choices);check(lv_dropdown_is_open(choices)&&lv_dropdown_get_option_count(choices)==6,"opening theme chooser loads all five built-ins without custom storage");
 for(unsigned i=0;i<5;++i){lv_dropdown_set_selected(choices,i+1);char selected[80];lv_dropdown_get_selected_str(choices,selected,sizeof(selected));check(!std::strcmp(selected,names[i]),"known theme is selectable");}
 lv_dropdown_close(choices);catalogue.count=6;lv_dropdown_open(choices);check(lv_dropdown_get_option_count(choices)==7,"reopening chooser discovers newly available custom theme");
 lv_dropdown_set_selected(choices,6);char selected[80];lv_dropdown_get_selected_str(choices,selected,sizeof(selected));check(!std::strcmp(selected,names[5]),"custom theme is selectable after READY reload");
 check(bg(lv_dropdown_get_list(choices),LV_PART_MAIN,saved[1]),"reloaded catalogue popup keeps active theme");lv_dropdown_close(choices);
 auto* preview=Tab5ThemePreview::create(screen);lv_obj_set_size(preview,1120,266);Tab5ThemePreview::update(preview,saved,tab5theme::PaletteCount);
 check(lv_obj_get_child_count(preview)==2,"theme preview contains two real LVGL switch controls");
 auto* previewOff=lv_obj_get_child(preview,0);auto* previewOn=lv_obj_get_child(preview,1);
 check(lv_obj_check_type(previewOff,&lv_switch_class)&&!lv_obj_has_state(previewOff,LV_STATE_CHECKED)&&bg(previewOff,LV_PART_MAIN,0)&&bg(previewOff,LV_PART_KNOB,0xffffff),"real preview OFF reflects independent track and knob colors");
 check(lv_obj_check_type(previewOn,&lv_switch_class)&&lv_obj_has_state(previewOn,LV_STATE_CHECKED)&&bg(previewOn,LV_PART_INDICATOR,0xffffff)&&bg(previewOn,LV_PART_KNOB,0),"real preview ON reflects independent track and knob colors");
 check(lv_obj_get_style_border_width(previewOff,LV_PART_KNOB)==2&&lv_obj_get_style_border_width(previewOn,LV_PART_KNOB)==2,"theme preview uses the same outlined thumbs as Settings");
 unsigned role=99;lv_obj_add_event_cb(preview,[](lv_event_t* e){*static_cast<unsigned*>(lv_event_get_user_data(e))=*static_cast<unsigned*>(lv_event_get_param(e));},LV_EVENT_VALUE_CHANGED,&role);
 for(unsigned i=1;i<=tab5theme::PaletteCount;++i){uint32_t key=LV_KEY_RIGHT;lv_obj_send_event(preview,LV_EVENT_KEY,&key);check(role==i%tab5theme::PaletteCount,"keyboard reaches every theme role including ON and OFF");}
 Tab5ThemePreview::update(preview,saved,9);uint32_t left=LV_KEY_LEFT;lv_obj_send_event(preview,LV_EVENT_KEY,&left);check(role==8,"legacy nine-role preview never selects outside its palette");
 uint32_t black[9]{};auto* blackPreview=Tab5ThemePreview::create(screen);Tab5ThemePreview::update(blackPreview,black,9);
 for(unsigned i=0;i<2;++i){auto* sw=lv_obj_get_child(blackPreview,i);check(bg(sw,LV_PART_MAIN,0)&&bg(sw,LV_PART_INDICATOR,0)&&bg(sw,LV_PART_KNOB,0),"first all-black legacy palette is applied to real preview controls");}
 lv_obj_delete(blackPreview);
 auto* bar=lv_obj_create(screen);lv_obj_remove_style_all(bar);lv_obj_set_size(bar,1280,48);lv_obj_set_style_pad_all(bar,0,0);
 auto* date=lv_label_create(bar);lv_obj_set_style_text_font(date,LV_FONT_DEFAULT,0);lv_obj_set_style_text_align(date,LV_TEXT_ALIGN_RIGHT,0);
 auto* audio=lv_obj_create(bar);lv_obj_set_pos(audio,550,8);lv_obj_set_height(audio,24);
 for(const char* text:{"sam. 10/10/26","Wednesday 30/12/2026","суббота 10/10/26","الأربعاء 30/12/2026","星期六 2026/10/10","2026年10月10日(土)","2026년 10월 10일 토요일"}){
  lv_label_set_text(date,text);Tab5TopBarLayout::date(date,audio,1280);lv_obj_update_layout(bar);
  lv_point_t measured;lv_text_get_size(&measured,text,LV_FONT_DEFAULT,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
  check(lv_obj_get_content_width(date)>=measured.x,"entire localized date fits measured content area");
  lv_area_t dateBounds,audioBounds;lv_obj_get_coords(date,&dateBounds);lv_obj_get_coords(audio,&audioBounds);
  check(audioBounds.x2+12<dateBounds.x1,"long date leaves gap to audio controls");
  check(dateBounds.x2<1066,"date stays before fixed status controls");
 }
 lv_obj_set_style_text_letter_space(date,3,0);lv_label_set_text(date,"Wednesday 30/12/2026");Tab5TopBarLayout::date(date,audio,1280);lv_obj_update_layout(bar);
 lv_point_t measured;lv_text_get_size(&measured,lv_label_get_text(date),LV_FONT_DEFAULT,3,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);check(lv_obj_get_content_width(date)>=measured.x,"date geometry responds to changed font spacing");
 lv_obj_delete(bar);lv_obj_delete(preview);lv_obj_delete(choices);lv_obj_delete(toggle);
}
