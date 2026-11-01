// Explorer layout. Included after toolbar and callbacks.
static void createFilePage() {
  filesPage = lv_obj_create(pageHost);
  lv_obj_set_size(filesPage, WORK_W, PAGE_H);Tab5Shell::width(filesPage,0);
  lv_obj_set_pos(filesPage, 0, 0);
  lv_obj_set_style_border_width(filesPage, 0, 0);
  lv_obj_set_style_radius(filesPage, 0, 0);
  lv_obj_set_style_pad_all(filesPage, 8, 0);
  lv_obj_set_style_bg_color(filesPage, C_BG(), 0);
  lv_obj_set_style_bg_opa(filesPage, LV_OPA_COVER, 0);
  lv_obj_remove_flag(filesPage, LV_OBJ_FLAG_SCROLLABLE);

  createFileToolbar(filesPage);

  // Left: folder-only tree, including the SD/USB volume rows.
  lv_obj_t* left = lv_obj_create(filesPage);
  lv_obj_set_size(left, FILE_LEFT_W, PAGE_H - 74);
  lv_obj_set_pos(left, 0, 58);
  lv_obj_set_style_bg_color(left, C_SURFACE(), 0);
  lv_obj_set_style_bg_opa(left, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(left, C_BORDER(), 0);
  lv_obj_set_style_border_width(left, 0, 0);
  lv_obj_set_style_radius(left, 8, 0);
  lv_obj_set_style_pad_all(left, 8, 0);
  lv_obj_remove_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  filePathLabel = lv_label_create(left);lv_label_set_text(filePathLabel,"");lv_obj_add_flag(filePathLabel,LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(filePathLabel,FILE_LEFT_W-32);
  lv_label_set_long_mode(filePathLabel,LV_LABEL_LONG_DOT);
  lv_obj_set_height(filePathLabel,28);
  lv_obj_set_style_text_font(filePathLabel,uiFont(),0);
  lv_obj_set_style_text_color(filePathLabel,C_MUTED(),0);
  lv_obj_set_pos(filePathLabel,0,2);

  fileList = lv_list_create(left);
  lv_obj_set_size(fileList, FILE_LEFT_W - 16, PAGE_H - 90);
  lv_obj_set_pos(fileList, 0, 0);
  lv_obj_set_scroll_dir(fileList,LV_DIR_VER);
  lv_obj_set_scrollbar_mode(fileList,LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_bg_color(fileList, C_SURFACE(), 0);
  lv_obj_set_style_border_width(fileList, 0, 0);
  lv_obj_set_style_radius(fileList, 0, 0);
  lv_obj_set_style_pad_all(fileList, 0, 0);
  lv_obj_set_style_pad_right(fileList, 8, 0);
  lv_obj_set_style_pad_row(fileList,0,0);
  lv_obj_set_flex_flow(fileList,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(fileList,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);

  filePaneDivider=lv_obj_create(filesPage);lv_obj_remove_style_all(filePaneDivider);
  lv_obj_set_pos(filePaneDivider,FILE_LEFT_W+4,58);lv_obj_set_size(filePaneDivider,1,PAGE_H-74);
  lv_obj_set_style_bg_color(filePaneDivider,C_BORDER(),0);lv_obj_set_style_bg_opa(filePaneDivider,LV_OPA_COVER,0);
  lv_obj_remove_flag(filePaneDivider,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));

  // Right: a full-width path, then metadata and contextual actions.
  lv_obj_t* right = lv_obj_create(filesPage);
  const int rightX = FILE_LEFT_W + 10;
  const int rightW = WORK_W - rightX - 16;
  lv_obj_set_size(right, rightW, PAGE_H - 74);Tab5Shell::width(right,-FILE_LEFT_W-26-0);
  lv_obj_set_pos(right, rightX, 58);
  lv_obj_set_style_bg_color(right, C_SURFACE(), 0);
  lv_obj_set_style_bg_opa(right, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(right, C_BORDER(), 0);
  lv_obj_set_style_border_width(right, 0, 0);
  lv_obj_set_style_radius(right, 8, 0);
  lv_obj_set_style_pad_all(right, 12, 0);
  lv_obj_remove_flag(right, LV_OBJ_FLAG_SCROLLABLE);

  filePreviewTitle = lv_label_create(right);
  lv_obj_set_width(filePreviewTitle, rightW - 24);Tab5Shell::width(filePreviewTitle,-FILE_LEFT_W-26-24);
  lv_label_set_long_mode(filePreviewTitle, LV_LABEL_LONG_DOT);
  lv_label_set_text(filePreviewTitle, "");
  lv_obj_set_style_text_font(filePreviewTitle, uiHeadingFont(), 0);
  lv_obj_set_height(filePreviewTitle,32);
  lv_obj_set_pos(filePreviewTitle, 0, 0);

  filePreviewMeta = lv_label_create(right);lv_label_set_text(filePreviewMeta,"");
  lv_obj_set_width(filePreviewMeta, rightW - 366);Tab5Shell::width(filePreviewMeta,-FILE_LEFT_W-26-366);
  lv_label_set_long_mode(filePreviewMeta, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(filePreviewMeta, C_MUTED(), 0);
  lv_obj_set_style_text_font(filePreviewMeta,uiFont(),0);
  lv_obj_set_height(filePreviewMeta,28);
  lv_obj_set_pos(filePreviewMeta, 0, 50);

  lv_obj_t* previewScroll = filePreviewScroll = lv_obj_create(right);lv_obj_add_flag(previewScroll,LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_size(previewScroll, rightW - 24, PAGE_H - 186);Tab5Shell::width(previewScroll,-FILE_LEFT_W-26-24);
  lv_obj_set_pos(previewScroll, 0, 88);
  lv_obj_set_style_bg_color(previewScroll, C_BG(), 0);
  lv_obj_set_style_bg_opa(previewScroll, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(previewScroll, 0, 0);
  lv_obj_set_style_radius(previewScroll, 6, 0);
  lv_obj_set_style_pad_all(previewScroll, 18, 0);
  lv_obj_set_scrollbar_mode(previewScroll, LV_SCROLLBAR_MODE_AUTO);

  filePreviewLabel = lv_label_create(previewScroll);
  lv_obj_set_width(filePreviewLabel, rightW - 66);Tab5Shell::width(filePreviewLabel,-FILE_LEFT_W-26-66);
  lv_label_set_long_mode(filePreviewLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(filePreviewLabel, documentFont(), 0);
  lv_obj_set_style_text_color(filePreviewLabel, C_TEXT(), 0);
  lv_obj_set_style_text_line_space(filePreviewLabel, 5, 0);
  lv_label_set_text(filePreviewLabel, "");

  folderPreviewHost=lv_obj_create(right);lv_obj_remove_style_all(folderPreviewHost);
  lv_obj_add_event_cb(folderPreviewHost,[](lv_event_t* e){if(lv_event_get_target_obj(e)==folderPreviewHost){folderPreviewHost=folderPreviewRows=folderPreviewPrevious=folderPreviewNext=folderPreviewPageLabel=nullptr;}},LV_EVENT_DELETE,nullptr);
  lv_obj_set_pos(folderPreviewHost,0,88);lv_obj_set_size(folderPreviewHost,rightW-24,PAGE_H-186);Tab5Shell::width(folderPreviewHost,-FILE_LEFT_W-26-24);
  lv_obj_remove_flag(folderPreviewHost,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(folderPreviewHost,LV_OBJ_FLAG_HIDDEN);
  auto* folderBar=lv_obj_create(folderPreviewHost);lv_obj_remove_style_all(folderBar);
  lv_obj_set_size(folderBar,lv_pct(100),40);lv_obj_align(folderBar,LV_ALIGN_BOTTOM_MID,0,0);lv_obj_remove_flag(folderBar,LV_OBJ_FLAG_SCROLLABLE);
  folderPreviewPrevious=makeIconButton(folderBar,LV_SYMBOL_LEFT,44,40);
  Tab5Focus.describe(folderPreviewPrevious,T("Page précédente"));
  lv_obj_add_event_cb(folderPreviewPrevious,folderPreviewPageEvent,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(-1)));
  folderPreviewNext=makeIconButton(folderBar,LV_SYMBOL_RIGHT,44,40);lv_obj_align(folderPreviewNext,LV_ALIGN_TOP_RIGHT,0,0);
  Tab5Focus.describe(folderPreviewNext,T("Page suivante"));
  lv_obj_add_event_cb(folderPreviewNext,folderPreviewPageEvent,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(1)));
  folderPreviewPageLabel=lv_label_create(folderBar);lv_label_set_text(folderPreviewPageLabel,"");lv_obj_center(folderPreviewPageLabel);
  lv_obj_set_style_text_font(folderPreviewPageLabel,uiFont(),0);
  lv_obj_set_style_text_color(folderPreviewPageLabel,C_MUTED(),0);
  folderPreviewRows=lv_obj_create(folderPreviewHost);lv_obj_remove_style_all(folderPreviewRows);
  lv_obj_set_pos(folderPreviewRows,0,0);lv_obj_set_width(folderPreviewRows,lv_pct(100));lv_obj_set_height(folderPreviewRows,PAGE_H-230);
  lv_obj_set_flex_flow(folderPreviewRows,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(folderPreviewRows,0,0);
  lv_obj_set_flex_align(folderPreviewRows,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
  lv_obj_add_flag(folderPreviewRows,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(folderPreviewRows,LV_DIR_VER);lv_obj_set_scrollbar_mode(folderPreviewRows,LV_SCROLLBAR_MODE_AUTO);lv_obj_set_style_pad_right(folderPreviewRows,8,0);

  filePreviewImage = lv_image_create(previewScroll);
  lv_obj_add_flag(filePreviewImage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(filePreviewImage, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_center(filePreviewImage);

  fileActionBar = lv_obj_create(right);
  lv_obj_remove_style_all(fileActionBar);
  lv_obj_set_size(fileActionBar, 330, 46);
  lv_obj_align(fileActionBar,LV_ALIGN_TOP_RIGHT,0,34);
  lv_obj_set_flex_flow(fileActionBar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(fileActionBar, LV_FLEX_ALIGN_END,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(fileActionBar,8,0);
  lv_obj_remove_flag(fileActionBar, LV_OBJ_FLAG_SCROLLABLE);

  fileOpenBtn = makeIconButton(fileActionBar, LV_SYMBOL_EDIT, 58, 46);
  lv_obj_add_event_cb(fileOpenBtn, [](lv_event_t* e){
    if(fileCanEditImage())fileMenuCommand(reinterpret_cast<void*>(uintptr_t(FileCommand::ViewImage)));
    else selectedOpenEvent(e);
  }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_state(fileOpenBtn, LV_STATE_DISABLED);

  fileDrawBtn = makeButton(fileActionBar,"",58,46);
  tab5DrawingIcon(fileDrawBtn,C_TEXT());
  Tab5Focus.describe(fileDrawBtn,T("Ouvrir avec Dessin"));
  lv_obj_add_event_cb(fileDrawBtn,[](lv_event_t*){
    fileMenuCommand(reinterpret_cast<void*>(uintptr_t(FileCommand::EditImage)));
  },LV_EVENT_CLICKED,nullptr);
  lv_obj_add_state(fileDrawBtn,LV_STATE_DISABLED);

  fileReadBtn = makeIconButton(fileActionBar, LV_SYMBOL_EYE_OPEN, 58, 46);
  lv_obj_add_event_cb(fileReadBtn, selectedReadEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_state(fileReadBtn, LV_STATE_DISABLED);

  fileExecuteBtn = makeIconButton(fileActionBar, LV_SYMBOL_PLAY, 58, 46);
  lv_obj_add_event_cb(fileExecuteBtn, selectedExecuteEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_state(fileExecuteBtn, LV_STATE_DISABLED);

  fileGitBtn=makeIconButton(fileActionBar,LV_SYMBOL_SHUFFLE,58,46);
  Tab5Focus.describe(fileGitBtn,T("Projet Git du dossier"));
  lv_obj_add_event_cb(fileGitBtn,[](lv_event_t*){openFileGit();},LV_EVENT_CLICKED,nullptr);

  fileOpenWithBtn=makeOpenWithButton(fileActionBar,58,46);
  Tab5Focus.describe(fileOpenWithBtn,T("Ouvrir avec…"));
  lv_obj_add_event_cb(fileOpenWithBtn,[](lv_event_t* e){openSelectedWith(lv_event_get_current_target_obj(e));},LV_EVENT_CLICKED,nullptr);

  fileGitHost=lv_obj_create(filesPage);lv_obj_remove_style_all(fileGitHost);
  lv_obj_set_pos(fileGitHost,rightX,58);lv_obj_set_size(fileGitHost,rightW,PAGE_H-74);Tab5Shell::width(fileGitHost,-FILE_LEFT_W-26);
  lv_obj_remove_flag(fileGitHost,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(fileGitHost,LV_OBJ_FLAG_HIDDEN);
  updateFileActionState();
}

