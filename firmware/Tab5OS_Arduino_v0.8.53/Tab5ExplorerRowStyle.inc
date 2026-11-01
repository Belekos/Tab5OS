// Explorer rows are controls, but not raised buttons. In particular Tab5Focus
// must not turn a previously focused row into a second filled selection.
static lv_style_t fileRowBaseStyle,fileRowSelectedStyle,fileRowPressedStyle,fileRowFocusStyle;
static bool fileRowStylesReady=false;
static void filePrepareRowStyles() {
  if(!fileRowStylesReady){
    lv_style_init(&fileRowBaseStyle);lv_style_init(&fileRowSelectedStyle);
    lv_style_init(&fileRowPressedStyle);lv_style_init(&fileRowFocusStyle);
    lv_style_set_radius(&fileRowBaseStyle,0);lv_style_set_border_width(&fileRowBaseStyle,0);
    lv_style_set_outline_width(&fileRowBaseStyle,0);lv_style_set_shadow_width(&fileRowBaseStyle,0);
    lv_style_set_bg_opa(&fileRowBaseStyle,LV_OPA_COVER);
    lv_style_set_pad_hor(&fileRowBaseStyle,8);lv_style_set_pad_ver(&fileRowBaseStyle,4);
    lv_style_set_border_width(&fileRowFocusStyle,2);
    lv_style_set_border_side(&fileRowFocusStyle,LV_BORDER_SIDE_LEFT);
    fileRowStylesReady=true;
  }
  static uint32_t surface=UINT32_MAX,selection=UINT32_MAX,active=UINT32_MAX,muted=UINT32_MAX;
  static const lv_font_t* font=nullptr;
  const uint32_t nextSurface=lv_color_to_u32(C_SURFACE()),nextSelection=lv_color_to_u32(C_SELECTED()),
      nextActive=lv_color_to_u32(C_ACTIVE()),nextMuted=lv_color_to_u32(C_MUTED());
  if(surface==nextSurface&&selection==nextSelection&&active==nextActive&&muted==nextMuted&&font==uiFont())return;
  surface=nextSurface;selection=nextSelection;active=nextActive;muted=nextMuted;font=uiFont();
  // Reapply shared properties only when the palette or font actually changes.
  lv_style_set_bg_color(&fileRowBaseStyle,C_SURFACE());
  lv_style_set_text_font(&fileRowBaseStyle,uiFont());
  lv_style_set_bg_color(&fileRowSelectedStyle,C_SELECTED());
  lv_style_set_bg_color(&fileRowPressedStyle,C_ACTIVE());
  lv_style_set_border_color(&fileRowFocusStyle,C_MUTED());
}
static void fileStyleRow(lv_obj_t* row,bool selected,lv_color_t textColor) {
  if(!row)return;
  const auto focus=lv_style_selector_t(LV_STATE_FOCUSED|LV_STATE_FOCUS_KEY);
  // Tab5Focus applies a local focus style. Keep it synchronized when the active
  // selection moves to the other pane, without depending on focus moving too.
  lv_obj_set_style_bg_color(row,selected?C_SELECTED():C_SURFACE(),0);
  lv_obj_set_style_bg_color(row,selected?C_SELECTED():C_SURFACE(),focus);
  lv_obj_set_style_bg_color(row,selected?C_SELECTED():C_ACTIVE(),focus|LV_STATE_PRESSED);
  lv_obj_set_style_text_color(row,textColor,0);
  lv_obj_set_style_text_color(row,textColor,focus);
  if(selected)lv_obj_add_state(row,LV_STATE_CHECKED);
  else lv_obj_remove_state(row,LV_STATE_CHECKED);
  // Icon and label remain stable child 0/1 for selection and accessibility.
  for(uint32_t i=0;i<2&&i<lv_obj_get_child_count(row);++i)
    lv_obj_set_style_text_color(lv_obj_get_child(row,i),textColor,0);
}

static lv_obj_t* fileCreateFlatRow(lv_obj_t* parent,const char* name,
                                  bool directory,lv_color_t textColor) {
  filePrepareRowStyles();
  auto* row=lv_obj_create(parent);lv_obj_remove_style_all(row);
  lv_obj_add_style(row,&fileRowBaseStyle,LV_STATE_DEFAULT);
  lv_obj_add_style(row,&fileRowSelectedStyle,LV_STATE_CHECKED);
  lv_obj_add_style(row,&fileRowPressedStyle,LV_STATE_PRESSED);
  lv_obj_add_style(row,&fileRowFocusStyle,LV_STATE_FOCUS_KEY);
  lv_obj_set_width(row,lv_pct(100));lv_obj_set_height(row,48);
  lv_obj_add_flag(row,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_USER_1));
  lv_obj_remove_flag(row,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE|
      LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_EVENT_BUBBLE));
  auto* icon=lv_label_create(row);lv_label_set_text(icon,directory?LV_SYMBOL_DIRECTORY:LV_SYMBOL_FILE);
  lv_obj_set_style_text_font(icon,&lv_font_montserrat_24,0);
  lv_obj_set_width(icon,28);lv_obj_align(icon,LV_ALIGN_LEFT_MID,0,0);
  lv_obj_remove_flag(icon,LV_OBJ_FLAG_CLICKABLE);
  auto* label=lv_label_create(row);lv_label_set_text(label,name?name:"");
  lv_obj_set_style_text_font(label,uiFont(),0);lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
  lv_obj_set_height(label,lv_font_get_line_height(uiFont()));
  lv_obj_set_width(label,lv_pct(100));
  lv_obj_align(label,LV_ALIGN_LEFT_MID,40,0);lv_obj_remove_flag(label,LV_OBJ_FLAG_CLICKABLE);
  fileStyleRow(row,false,textColor);
  return row;
}

static lv_obj_t* fileCreateTreeRow(lv_obj_t* parent,const char* name,uint16_t depth,
    bool directory,bool expanded,lv_color_t textColor,lv_obj_t** chevron) {
  auto* row=fileCreateFlatRow(parent,name,directory,textColor);
  // Bound only the visual indentation, never the model depth or file path.
  const int indent=depth<10?int(depth)*16:160;
  lv_obj_set_style_pad_left(row,indent+40,0);
  lv_obj_set_style_pad_right(row,8,0);
  auto* icon=lv_obj_get_child(row,0);auto* label=lv_obj_get_child(row,1);
  lv_obj_align(icon,LV_ALIGN_LEFT_MID,0,0);
  lv_obj_align(label,LV_ALIGN_LEFT_MID,36,0);
  lv_obj_set_width(label,FILE_LEFT_W-118-indent);
  if(chevron)*chevron=nullptr;
  if(directory){
    auto* toggle=lv_button_create(row);lv_obj_remove_style_all(toggle);
    lv_obj_set_size(toggle,32,42);lv_obj_align(toggle,LV_ALIGN_LEFT_MID,-36,0);
    lv_obj_remove_flag(toggle,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE|
        LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_EVENT_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK));
    lv_obj_set_style_bg_opa(toggle,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(toggle,0,0);lv_obj_set_style_outline_width(toggle,0,0);
    lv_obj_set_style_shadow_width(toggle,0,0);lv_obj_set_style_radius(toggle,0,0);
    lv_obj_set_style_bg_color(toggle,C_ACTIVE(),LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(toggle,LV_OPA_COVER,LV_STATE_PRESSED);
    auto* glyph=lv_label_create(toggle);lv_label_set_text(glyph,expanded?LV_SYMBOL_DOWN:LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(glyph,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(glyph,C_MUTED(),0);lv_obj_center(glyph);
    lv_obj_remove_flag(glyph,LV_OBJ_FLAG_CLICKABLE);
    if(chevron)*chevron=toggle;
  }
  return row;
}

static lv_obj_t* fileCreateContentRow(lv_obj_t* parent,const char* name,
                                     bool directory,lv_color_t textColor) {
  auto* row=fileCreateFlatRow(parent,name,directory,textColor);
  auto* label=lv_obj_get_child(row,1);
  // Flex assigns the full remaining width, including after dock width changes.
  lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row,12,0);
  lv_obj_set_width(label,0);lv_obj_set_flex_grow(label,1);
  return row;
}
