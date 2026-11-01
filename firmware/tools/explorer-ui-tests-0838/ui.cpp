#include <lvgl.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
#include "Tab5Icons.h"
static unsigned checks=0,frames=0,flushes=0;
static void check(bool ok,const char*why){++checks;if(!ok){fprintf(stderr,"FAIL %u: %s\n",checks,why);std::exit(1);}}
static std::string shotDir;
static std::vector<uint8_t> pixels(1280u*720u*3u),fontData;
static lv_font_t *font22=nullptr,*font24=nullptr;static const lv_font_t* font=nullptr;
static lv_display_t* display;static lv_obj_t* screen;
static lv_obj_t *pageHost=nullptr,*filesPage=nullptr,*filePathLabel=nullptr,*fileList=nullptr;
static lv_obj_t *filePreviewTitle=nullptr,*filePreviewMeta=nullptr,*filePreviewScroll=nullptr,*filePreviewLabel=nullptr,*filePreviewImage=nullptr;
static lv_obj_t *folderPreviewHost=nullptr,*folderPreviewRows=nullptr,*folderPreviewPrevious=nullptr,*folderPreviewNext=nullptr,*folderPreviewPageLabel=nullptr;
static lv_obj_t *fileActionBar=nullptr,*fileOpenBtn=nullptr,*fileReadBtn=nullptr,*fileExecuteBtn=nullptr,*fileGitBtn=nullptr,*fileOpenWithBtn=nullptr,*fileGitHost=nullptr;
static lv_obj_t *filePaneDivider=nullptr,*fileParentButton=nullptr,*fileActionsMenuBtn=nullptr,*fileUsbButton=nullptr,*fileSearchClear=nullptr;
static constexpr int WORK_W=1216,PAGE_H=672,FILE_LEFT_W=380;
static struct {int cornerRadius=8;} settings;
static const uint32_t paletteLight[]={0xf4f4f0,0xffffff,0xe8e9e6,0xb9bcb6,0x222824,0x69716a,0x427b57,0xffffff,0xd9eadc};
static uint32_t palette[9];
static lv_color_t C_BG(){return lv_color_hex(palette[0]);}static lv_color_t C_SURFACE(){return lv_color_hex(palette[1]);}
static lv_color_t C_RAISED(){return lv_color_hex(palette[2]);}static lv_color_t C_BORDER(){return lv_color_hex(palette[3]);}
static lv_color_t C_TEXT(){return lv_color_hex(palette[4]);}static lv_color_t C_MUTED(){return lv_color_hex(palette[5]);}
static lv_color_t C_ACTIVE(){return lv_color_hex(palette[6]);}static lv_color_t C_SELECTED(){return lv_color_hex(palette[8]);}
static const lv_font_t*uiFont(){return font;}static const lv_font_t*uiHeadingFont(){return font;}static const lv_font_t*documentFont(){return font;}
static const char*T(const char*s){return s;}
namespace Tab5Shell {static void width(lv_obj_t*,int){}}
static void folderPreviewPageEvent(lv_event_t*){}static void selectedOpenEvent(lv_event_t*){}
static void selectedReadEvent(lv_event_t*){}static void selectedExecuteEvent(lv_event_t*){}
static void openFileGit(){}static void openSelectedWith(lv_obj_t*){}
// Command availability belongs to the interaction harness. These visible sets
// inject representative empty/folder/text/image states to exercise panel layout.
static unsigned fixtureSelection=0;
static void updateFileActionState(){
  const bool file=fixtureSelection==2||fixtureSelection==3;
  auto visible=[](lv_obj_t*obj,bool show){if(!obj)return;if(show)lv_obj_remove_flag(obj,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);};
  visible(fileOpenBtn,file);visible(fileReadBtn,fixtureSelection==2);visible(fileExecuteBtn,false);visible(fileGitBtn,fixtureSelection==4);visible(fileOpenWithBtn,file);
  for(auto*obj:{fileOpenBtn,fileReadBtn,fileOpenWithBtn})if(obj)lv_obj_remove_state(obj,LV_STATE_DISABLED);
  if(fileOpenBtn)lv_label_set_text(lv_obj_get_child(fileOpenBtn,0),fixtureSelection==3?LV_SYMBOL_IMAGE:LV_SYMBOL_EDIT);
  if(fileGitBtn){if(fixtureSelection)lv_obj_remove_state(fileGitBtn,LV_STATE_DISABLED);else lv_obj_add_state(fileGitBtn,LV_STATE_DISABLED);}
}
static void upFolderEvent(lv_event_t*){}static void fileMenuEvent(lv_event_t*){}static void fileSearchEvent(lv_event_t*){}
enum class FileMenu {Create,Edit,Actions,View,Sort,Git,Context,ImageOpenWith};
#include "helpers-extracted.inc"
#include "Tab5ExplorerUi.inc"
#include "Tab5ExplorerRowStyle.inc"
static void pump(){lv_tick_inc(20);lv_timer_handler();lv_obj_update_layout(screen);}
static void snapshot(const char*name){lv_obj_invalidate(screen);lv_refr_now(display);std::string path=shotDir+"/"+name+".ppm";FILE*f=fopen(path.c_str(),"wb");check(f,"snapshot opens");fprintf(f,"P6\n1280 720\n255\n");for(size_t i=0;i<pixels.size();i+=3){uint8_t rgb[]={pixels[i+2],pixels[i+1],pixels[i]};fwrite(rgb,1,3,f);}fclose(f);++frames;}
static bool color(lv_color_t a,lv_color_t b){return lv_color_to_u32(a)==lv_color_to_u32(b);}
static lv_obj_t*tree(const char*name,unsigned depth,bool expanded,bool children,lv_obj_t**chevron=nullptr){
  lv_obj_t*toggle=nullptr;auto*r=fileCreateTreeRow(fileList,name,depth,true,expanded,C_TEXT(),&toggle);
  if(toggle&&!children)lv_obj_add_flag(toggle,LV_OBJ_FLAG_HIDDEN);
  if(chevron)*chevron=toggle;fileStyleRow(r,false,C_TEXT());return r;
}
static lv_obj_t*content(const char*name,bool dir){auto*r=fileCreateContentRow(folderPreviewRows,name,dir,C_TEXT());fileStyleRow(r,false,C_TEXT());return r;}
static void showFiles(){fixtureSelection=1;updateFileActionState();lv_obj_add_flag(filePreviewScroll,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(folderPreviewHost,LV_OBJ_FLAG_HIDDEN);lv_label_set_text(filePathLabel,"");lv_label_set_text(filePreviewTitle,"/Images/Paintings");lv_label_set_text(filePreviewMeta,"8 éléments");lv_label_set_text(folderPreviewPageLabel,"1–8 / 8");lv_obj_add_flag(folderPreviewPrevious,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(folderPreviewNext,LV_OBJ_FLAG_HIDDEN);}
static void setup(){lv_init();font22=lv_tiny_ttf_create_data(fontData.data(),fontData.size(),22);font24=lv_tiny_ttf_create_data(fontData.data(),fontData.size(),24);check(font22&&font24,"shipped OS font loaded22/24");font22->fallback=&lv_font_montserrat_22;font24->fallback=&lv_font_montserrat_24;font=font22;std::copy(std::begin(paletteLight),std::end(paletteLight),palette);Tab5UiStyle::configure(palette,8);display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*,uint8_t*){++flushes;lv_display_flush_ready(d);});screen=lv_obj_create(nullptr);lv_screen_load(screen);lv_obj_set_style_bg_color(screen,C_BG(),0);lv_obj_set_style_text_font(screen,font,0);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);Tab5Focus.begin(screen,font);Tab5Focus.theme(C_SELECTED(),C_TEXT(),C_SURFACE());pageHost=lv_obj_create(screen);lv_obj_set_pos(pageHost,64,48);lv_obj_set_size(pageHost,WORK_W,PAGE_H);lv_obj_set_style_pad_all(pageHost,0,0);lv_obj_set_style_border_width(pageHost,0,0);lv_obj_remove_flag(pageHost,LV_OBJ_FLAG_SCROLLABLE);auto*title=lv_label_create(screen);lv_label_set_text(title,"Explorateur · Tab5OS");lv_obj_set_pos(title,20,12);}
int main(int argc,char**argv){check(argc==3,"arguments");shotDir=argv[1];std::ifstream f(argv[2],std::ios::binary);fontData=std::vector<uint8_t>(std::istreambuf_iterator<char>(f),{});setup();
 for(unsigned fontsize:{22u,24u}){
  font=fontsize==22?font22:font24;lv_obj_set_style_text_font(screen,font,0);fixtureSelection=0;createFilePage();pump();
  check(lv_obj_get_child_count(fileList)==0,"page construction itself does not invent directory entries");
  check(lv_obj_get_child_count(folderPreviewRows)==0,"page construction leaves storage population to navigation");
  check(std::strlen(lv_label_get_text(filePathLabel))==0,"left caption has no obsolete root title or LVGL placeholder");
  check(std::strlen(lv_label_get_text(filePreviewTitle))==0,"page construction has no fabricated right path");
  check(std::strlen(lv_label_get_text(filePreviewMeta))==0,"startup metadata is empty");
  check(std::strlen(lv_label_get_text(filePreviewLabel))==0,"startup preview text is empty");
  check(lv_obj_has_flag(filePreviewImage,LV_OBJ_FLAG_HIDDEN),"startup image is hidden");
  lv_obj_t* divider=nullptr;
  for(uint32_t i=0;i<lv_obj_get_child_count(filesPage);++i){auto*child=lv_obj_get_child(filesPage,i);if(lv_obj_get_width(child)==1&&lv_obj_get_height(child)>PAGE_H/2){check(!divider,"exactly one thin pane divider");divider=child;}}
  check(divider,"one-pixel separator exists between the two panes");
  lv_area_t dividerArea,leftArea,rightArea;lv_obj_get_coords(divider,&dividerArea);lv_obj_get_coords(lv_obj_get_parent(fileList),&leftArea);lv_obj_get_coords(lv_obj_get_parent(filePreviewTitle),&rightArea);
  check(dividerArea.x1>leftArea.x2&&dividerArea.x2<rightArea.x1,"pane separator sits inside the gap without clipping content");
  check(color(lv_obj_get_style_bg_color(divider,LV_PART_MAIN),C_BORDER()),"separator uses the neutral border color");
  check(!lv_obj_has_flag(divider,LV_OBJ_FLAG_CLICKABLE),"separator is not an input target");
  check(lv_obj_has_flag(filePathLabel,LV_OBJ_FLAG_HIDDEN),"obsolete left caption occupies no visible header");
  // Initial contents are a fixture at the storage boundary; actual first-open
  // navigation and volume presence are exercised by the interaction harness.
  tree("SD",0,true,true);tree("Documents",1,false,false);tree("Images",1,false,true);tree("Music",1,false,false);tree("Projects",1,false,false);tree("USB",0,false,true);
  for(auto name:{"Documents","Images","Music","Projects"})content(name,true);
  content("README.md",false);content("launcher.bin",false);
  lv_label_set_text(filePreviewTitle,"/");lv_label_set_text(filePreviewMeta,"6 éléments");
  lv_label_set_text(folderPreviewPageLabel,"1–6 / 6");lv_obj_add_flag(folderPreviewPrevious,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(folderPreviewNext,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(folderPreviewHost,LV_OBJ_FLAG_HIDDEN);lv_obj_add_state(fileParentButton,LV_STATE_DISABLED);pump();
  check(lv_obj_has_flag(fileGitBtn,LV_OBJ_FLAG_HIDDEN),"Git shortcut hidden without a configured folder selection");
  std::string shot="00-root-open-"+std::to_string(fontsize);snapshot(shot.c_str());
  lv_obj_clean(fileList);lv_obj_clean(folderPreviewRows);lv_obj_remove_state(fileParentButton,LV_STATE_DISABLED);showFiles();
  lv_obj_t*chevron=nullptr;tree("SD",0,true,true);auto*first=tree("Documents",1,false,false,&chevron);tree("Images",1,true,true);auto*images=tree("Paintings",2,false,true);tree("Music",1,false,false);tree("Projects",1,false,false);tree("USB",0,false,true);
  std::vector<lv_obj_t*>right;for(auto name:{"Archives","Brouillons","Coucher de soleil.jpg","Étude couleur.png","Grand format.bmp","Paysage.t5i","Portrait.webp","Scène de nuit.jpeg"})right.push_back(content(name,right.size()<2));
  fileStyleRow(images,true,C_TEXT());pump();
  // A fixed row/button must never let its text wrap into the adjacent control.
  auto checkLine=[&](lv_obj_t* label,lv_obj_t* control,const char*why){
    check(lv_obj_get_height(label)<=font->line_height,why);
    lv_area_t textArea,controlArea;lv_obj_get_coords(label,&textArea);lv_obj_get_coords(control,&controlArea);
    check(textArea.y1>=controlArea.y1&&textArea.y2<=controlArea.y2,"one-line caption remains vertically inside its control");
  };
  for(uint32_t i=0;i<lv_obj_get_child_count(fileList);++i){auto*row=lv_obj_get_child(fileList,i);checkLine(lv_obj_get_child(row,1),row,"tree file name never wraps over the next row");}
  for(auto*row:right)checkLine(lv_obj_get_child(row,1),row,"right file name never wraps over the next row");
  auto*toolbar=lv_obj_get_parent(fileParentButton);
  check(lv_obj_get_child_count(toolbar)==7,"toolbar has parent, four menus, sort and search icons");
  for(uint32_t i=1;i<=4;++i){auto*button=lv_obj_get_child(toolbar,i);auto*label=lv_obj_get_child(button,0);checkLine(label,button,"toolbar caption stays on one line");
    lv_area_t captionArea,arrowArea;lv_obj_get_coords(label,&captionArea);lv_obj_get_coords(lv_obj_get_child(button,1),&arrowArea);
    check(captionArea.x2<arrowArea.x1,"toolbar caption does not touch the dropdown arrow");
    lv_point_t measured;lv_text_get_size(&measured,lv_label_get_text(label),font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    check(measured.x<=lv_obj_get_width(label),"toolbar caption fits without truncation at the shipped font size");
  }
  for(uint32_t i=5;i<7;++i)check(lv_obj_get_width(lv_obj_get_child(toolbar,i))<=58,"sort and search are compact icon controls");
  const char*names[]={"Fichier","Édition","Actions","Affichage"};
  for(unsigned i=0;i<4;++i)check(std::strcmp(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(toolbar,i+1),0)),names[i])==0,"only the four intended text menus remain");
  check(lv_obj_get_style_border_width(right[0],LV_PART_MAIN)==0,"right rows have no outline");check(lv_obj_get_style_radius(right[0],LV_PART_MAIN)==0,"right rows have no rounded cards");check(lv_obj_get_style_shadow_width(right[0],LV_PART_MAIN)==0,"right rows have no shadow");
  check(chevron&&lv_obj_get_style_border_width(chevron,LV_PART_MAIN)==0&&lv_obj_get_style_bg_opa(chevron,LV_PART_MAIN)==LV_OPA_TRANSP,"chevron has no button box");check(!lv_obj_has_flag(chevron,LV_OBJ_FLAG_EVENT_BUBBLE),"chevron click does not bubble into row");
  check(lv_obj_has_flag(chevron,LV_OBJ_FLAG_HIDDEN),"folder without subdirectories has no visible chevron");
  check(!lv_obj_has_flag(lv_obj_get_child(images,2),LV_OBJ_FLAG_HIDDEN),"folder with subdirectories retains its chevron");
  check(lv_obj_get_height(fileList)>PAGE_H*0.85,"left tree uses the freed caption space");check(lv_obj_get_y(fileList)==0,"left tree starts at the top of its pane");
  lv_area_t listArea,hostArea;lv_obj_get_coords(fileList,&listArea);lv_obj_get_coords(pageHost,&hostArea);check(listArea.y2<=hostArea.y2,"left list remains inside page");
  shot="01-expanded-"+std::to_string(fontsize);snapshot(shot.c_str());
  // Production Focus is deliberately exercised: it must not preserve a row's
  // formerly selected background when the last selection moves to the right.
  fixtureSelection=3;updateFileActionState();Tab5Focus.focusObject(images);fileStyleRow(images,true,C_TEXT());fileStyleRow(images,false,C_TEXT());fileStyleRow(right[2],true,C_TEXT());pump();
  check(!color(lv_obj_get_style_bg_color(images,LV_PART_MAIN),C_SELECTED()),"former left selection not selected under focus");check(color(lv_obj_get_style_bg_color(right[2],LV_PART_MAIN),C_SELECTED()),"last right selection has selection fill");
  shot="02-right-selection-"+std::to_string(fontsize);snapshot(shot.c_str());
  for(unsigned cycle=0;cycle<16;++cycle){Tab5Focus.focusObject(images);fileStyleRow(images,true,C_TEXT());fileStyleRow(right[2],false,C_TEXT());Tab5Focus.remember(filesPage);Tab5Focus.focusObject(fileParentButton);fileStyleRow(images,false,C_TEXT());fileStyleRow(right[2],true,C_TEXT());Tab5Focus.restore(filesPage);pump();check(!color(lv_obj_get_style_bg_color(images,LV_PART_MAIN),C_SELECTED()),"restored focus cannot revive stale selection");check(color(lv_obj_get_style_bg_color(right[2],LV_PART_MAIN),C_SELECTED()),"right selection survives focus restore");}
  lv_obj_add_state(first,LV_STATE_PRESSED);fileStyleRow(first,false,C_TEXT());check(!color(lv_obj_get_style_bg_color(first,LV_PART_MAIN),C_SELECTED()),"pressed unselected left row is not a second selected row");lv_obj_remove_state(first,LV_STATE_PRESSED);
  // Search results use the production two-line row geometry. Only the data
  // source is injected; gestures/search/session behavior has a separate harness.
  Tab5Focus.clear();lv_obj_clean(folderPreviewRows);fixtureSelection=0;updateFileActionState();
  lv_label_set_text(filePreviewTitle,"/");lv_label_set_text(filePreviewMeta,"couleur · 3");
  lv_label_set_text(folderPreviewPageLabel,"1–3 / 3");
  const char* searchNames[]={"Étude couleur.png","Couleurs","Palette couleur.txt"};
  const char* paths[]={"/Images/Paintings","/Documents","/Projects/Tab5OS"};
  for(unsigned i=0;i<3;++i){
    const std::string label=std::string(searchNames[i])+"\n"+paths[i];auto*row=content(label.c_str(),i==1);
    lv_obj_set_height(lv_obj_get_child(row,1),2*lv_font_get_line_height(uiFont()));lv_obj_set_height(row,2*lv_font_get_line_height(uiFont())+12);
  }
  pump();lv_area_t previewTitleArea,previewArea;lv_obj_get_coords(filePreviewTitle,&previewTitleArea);lv_obj_get_coords(folderPreviewHost,&previewArea);
  check(previewTitleArea.y2<previewArea.y1,"search root path does not overlap search results");
  lv_area_t actionArea,metaArea;lv_obj_get_coords(fileActionBar,&actionArea);lv_obj_get_coords(filePreviewMeta,&metaArea);
  check(previewTitleArea.y2<actionArea.y1,"file actions cannot obscure the full-width path");
  check(metaArea.x2<actionArea.x1,"file actions do not cover search metadata");
  check(actionArea.y2<previewArea.y1,"file actions do not cover search results");
  lv_point_t querySize;lv_text_get_size(&querySize,lv_label_get_text(filePreviewMeta),font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
  check(querySize.x<=lv_obj_get_width(filePreviewMeta),"query and result count fit in search metadata");
  for(uint32_t i=0;i<lv_obj_get_child_count(folderPreviewRows);++i){auto*row=lv_obj_get_child(folderPreviewRows,i);auto*label=lv_obj_get_child(row,1);lv_area_t labelArea,rowArea;lv_obj_get_coords(label,&labelArea);lv_obj_get_coords(row,&rowArea);
    check(labelArea.y1>=rowArea.y1&&labelArea.y2<=rowArea.y2,"search name and parent path fit in their row");
  }
  shot="03-search-results-"+std::to_string(fontsize);snapshot(shot.c_str());
  // A configured Git folder alone exposes the contextual shortcut.
  lv_obj_add_flag(filePreviewScroll,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(folderPreviewHost,LV_OBJ_FLAG_HIDDEN);
  Tab5Focus.clear();lv_obj_clean(fileList);lv_obj_clean(folderPreviewRows);
  tree("SD",0,true,true);tree("Documents",1,false,false);tree("Images",1,false,true);tree("Music",1,false,false);tree("Projects",1,true,true);auto*project=tree("Tab5OS",2,false,true);tree("USB",0,false,true);fileStyleRow(project,true,C_TEXT());
  content("include",true);content("src",true);content("CMakeLists.txt",false);content("README.md",false);
  fixtureSelection=4;updateFileActionState();lv_label_set_text(filePreviewTitle,"/Projects/Tab5OS");lv_label_set_text(filePreviewMeta,"4 éléments");lv_label_set_text(folderPreviewPageLabel,"1–4 / 4");pump();
  check(!lv_obj_has_flag(fileGitBtn,LV_OBJ_FLAG_HIDDEN),"configured folder fixture shows its Git shortcut");
  check(lv_obj_has_flag(fileOpenBtn,LV_OBJ_FLAG_HIDDEN)&&lv_obj_has_flag(fileOpenWithBtn,LV_OBJ_FLAG_HIDDEN),"Git folder fixture does not expose file-only shortcuts");
  shot="04-git-folder-"+std::to_string(fontsize);snapshot(shot.c_str());
  pump();const unsigned idleFlushes=flushes;for(unsigned i=0;i<25;++i)pump();
  check(flushes==idleFlushes,"idle explorer does not continuously redraw or flicker");
  Tab5Focus.clear();lv_obj_delete(filesPage);filesPage=nullptr;pump();
 }
 lv_obj_delete(pageHost);lv_tiny_ttf_destroy(font22);lv_tiny_ttf_destroy(font24);lv_deinit();printf("{\"passed\":true,\"checks\":%u,\"frames\":%u,\"focusRestoreCycles\":32,\"fonts\":[22,24]}\n",checks,frames);return 0;
}
