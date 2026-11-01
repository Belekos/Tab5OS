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
static unsigned checks=0,frames=0;
static void check(bool ok,const char*why){++checks;if(!ok){fprintf(stderr,"FAIL %u: %s\n",checks,why);std::exit(1);}}
static std::string shotDir;
static std::vector<uint8_t> pixels(1280u*720u*3u),fontData;
static lv_font_t *font22=nullptr,*font24=nullptr;static const lv_font_t* font=nullptr;
static lv_display_t* display;static lv_obj_t* screen;
static lv_obj_t *pageHost=nullptr,*filesPage=nullptr,*filePathLabel=nullptr,*fileList=nullptr;
static lv_obj_t *filePreviewTitle=nullptr,*filePreviewMeta=nullptr,*filePreviewScroll=nullptr,*filePreviewLabel=nullptr,*filePreviewImage=nullptr;
static lv_obj_t *folderPreviewHost=nullptr,*folderPreviewRows=nullptr,*folderPreviewPrevious=nullptr,*folderPreviewNext=nullptr,*folderPreviewPageLabel=nullptr;
static lv_obj_t *fileActionBar=nullptr,*fileOpenBtn=nullptr,*fileReadBtn=nullptr,*fileExecuteBtn=nullptr,*fileGitBtn=nullptr,*fileOpenWithBtn=nullptr,*fileGitHost=nullptr;
static lv_obj_t *fileParentButton=nullptr,*fileActionsMenuBtn=nullptr,*fileUsbButton=nullptr;
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
static void openFileGit(){}static void openSelectedWith(lv_obj_t*){}static void updateFileActionState(){}
static void upFolderEvent(lv_event_t*){}static void fileMenuEvent(lv_event_t*){}
enum class FileMenu {Create,Edit,Actions,View,Navigate,Sort,Git,Context,ImageOpenWith};
#include "helpers-extracted.inc"
#include "Tab5ExplorerUi.inc"
#include "Tab5ExplorerRowStyle.inc"
static void pump(){lv_tick_inc(20);lv_timer_handler();lv_obj_update_layout(screen);}
static void snapshot(const char*name){lv_obj_invalidate(screen);lv_refr_now(display);std::string path=shotDir+"/"+name+".ppm";FILE*f=fopen(path.c_str(),"wb");check(f,"snapshot opens");fprintf(f,"P6\n1280 720\n255\n");for(size_t i=0;i<pixels.size();i+=3){uint8_t rgb[]={pixels[i+2],pixels[i+1],pixels[i]};fwrite(rgb,1,3,f);}fclose(f);++frames;}
static bool color(lv_color_t a,lv_color_t b){return lv_color_to_u32(a)==lv_color_to_u32(b);}
static lv_obj_t*tree(const char*name,unsigned depth,bool dir,bool expanded,lv_obj_t**chevron=nullptr){auto*r=fileCreateTreeRow(fileList,name,depth,dir,expanded,C_TEXT(),chevron);fileStyleRow(r,false,C_TEXT());return r;}
static lv_obj_t*content(const char*name,bool dir){auto*r=fileCreateContentRow(folderPreviewRows,name,dir,C_TEXT());fileStyleRow(r,false,C_TEXT());return r;}
static void showFiles(){lv_obj_add_flag(filePreviewScroll,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(folderPreviewHost,LV_OBJ_FLAG_HIDDEN);lv_label_set_text(filePathLabel,"Carte SD");lv_label_set_text(filePreviewTitle,"/Images/Peintures");lv_label_set_text(filePreviewMeta,"8 éléments");lv_label_set_text(folderPreviewPageLabel,"1–8 / 8");}
static void setup(){lv_init();font22=lv_tiny_ttf_create_data(fontData.data(),fontData.size(),22);font24=lv_tiny_ttf_create_data(fontData.data(),fontData.size(),24);check(font22&&font24,"shipped OS font loaded22/24");font22->fallback=&lv_font_montserrat_22;font24->fallback=&lv_font_montserrat_24;font=font22;std::copy(std::begin(paletteLight),std::end(paletteLight),palette);Tab5UiStyle::configure(palette,8);display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});screen=lv_obj_create(nullptr);lv_screen_load(screen);lv_obj_set_style_bg_color(screen,C_BG(),0);lv_obj_set_style_text_font(screen,font,0);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);Tab5Focus.begin(screen,font);Tab5Focus.theme(C_SELECTED(),C_TEXT(),C_SURFACE());pageHost=lv_obj_create(screen);lv_obj_set_pos(pageHost,64,48);lv_obj_set_size(pageHost,WORK_W,PAGE_H);lv_obj_set_style_pad_all(pageHost,0,0);lv_obj_set_style_border_width(pageHost,0,0);lv_obj_remove_flag(pageHost,LV_OBJ_FLAG_SCROLLABLE);auto*title=lv_label_create(screen);lv_label_set_text(title,"Explorateur · Tab5OS");lv_obj_set_pos(title,20,12);}
int main(int argc,char**argv){check(argc==3,"arguments");shotDir=argv[1];std::ifstream f(argv[2],std::ios::binary);fontData=std::vector<uint8_t>(std::istreambuf_iterator<char>(f),{});setup();
 for(unsigned fontsize:{22u,24u}){
  font=fontsize==22?font22:font24;lv_obj_set_style_text_font(screen,font,0);createFilePage();showFiles();
  lv_obj_t*chevron=nullptr;auto*first=tree("Documents",0,true,false,&chevron);auto*images=tree("Images",0,true,true);tree("Peintures",1,true,true);tree("brouillon.t5i",2,false,false);tree("Coucher de soleil.jpg",2,false,false);tree("Photo originale.bmp",1,false,false);tree("Musique",0,true,false);tree("Projets",0,true,false);tree("USB",0,true,false);
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
  for(uint32_t i=1;i<lv_obj_get_child_count(toolbar);++i){auto*button=lv_obj_get_child(toolbar,i);auto*label=lv_obj_get_child(button,0);checkLine(label,button,"toolbar caption stays on one line");
    lv_area_t captionArea,arrowArea;lv_obj_get_coords(label,&captionArea);lv_obj_get_coords(lv_obj_get_child(button,1),&arrowArea);
    check(captionArea.x2<arrowArea.x1,"toolbar caption does not touch the dropdown arrow");
  }
  check(lv_obj_get_style_border_width(right[0],LV_PART_MAIN)==0,"right rows have no outline");check(lv_obj_get_style_radius(right[0],LV_PART_MAIN)==0,"right rows have no rounded cards");check(lv_obj_get_style_shadow_width(right[0],LV_PART_MAIN)==0,"right rows have no shadow");
  check(chevron&&lv_obj_get_style_border_width(chevron,LV_PART_MAIN)==0&&lv_obj_get_style_bg_opa(chevron,LV_PART_MAIN)==LV_OPA_TRANSP,"chevron has no button box");check(!lv_obj_has_flag(chevron,LV_OBJ_FLAG_EVENT_BUBBLE),"chevron click does not bubble into row");
  check(lv_obj_get_height(fileList)>PAGE_H*3/4,"left list has over75 percent page height");check(lv_obj_get_y(fileList)<=34,"left header occupies at most34 pixels");
  lv_area_t listArea,hostArea;lv_obj_get_coords(fileList,&listArea);lv_obj_get_coords(pageHost,&hostArea);check(listArea.y2<=hostArea.y2,"left list remains inside page");
  std::string shot="01-expanded-"+std::to_string(fontsize);snapshot(shot.c_str());
  // Production Focus is deliberately exercised: it must not preserve a row's
  // formerly selected background when the last selection moves to the right.
  Tab5Focus.focusObject(images);fileStyleRow(images,true,C_TEXT());fileStyleRow(images,false,C_TEXT());fileStyleRow(right[2],true,C_TEXT());pump();
  check(!color(lv_obj_get_style_bg_color(images,LV_PART_MAIN),C_SELECTED()),"former left selection not selected under focus");check(color(lv_obj_get_style_bg_color(right[2],LV_PART_MAIN),C_SELECTED()),"last right selection has selection fill");
  shot="02-right-selection-"+std::to_string(fontsize);snapshot(shot.c_str());
  for(unsigned cycle=0;cycle<256;++cycle){Tab5Focus.focusObject(images);fileStyleRow(images,true,C_TEXT());fileStyleRow(right[2],false,C_TEXT());Tab5Focus.remember(filesPage);Tab5Focus.focusObject(fileParentButton);fileStyleRow(images,false,C_TEXT());fileStyleRow(right[2],true,C_TEXT());Tab5Focus.restore(filesPage);pump();check(!color(lv_obj_get_style_bg_color(images,LV_PART_MAIN),C_SELECTED()),"restored focus cannot revive stale selection");check(color(lv_obj_get_style_bg_color(right[2],LV_PART_MAIN),C_SELECTED()),"right selection survives focus restore");}
  lv_obj_add_state(first,LV_STATE_PRESSED);fileStyleRow(first,false,C_TEXT());check(!color(lv_obj_get_style_bg_color(first,LV_PART_MAIN),C_SELECTED()),"pressed unselected left row is not a second selected row");lv_obj_remove_state(first,LV_STATE_PRESSED);
  Tab5Focus.clear();lv_obj_delete(filesPage);filesPage=nullptr;pump();
 }
 lv_obj_delete(pageHost);lv_tiny_ttf_destroy(font22);lv_tiny_ttf_destroy(font24);lv_deinit();printf("{\"passed\":true,\"checks\":%u,\"frames\":%u,\"focusRestoreCycles\":512,\"fonts\":[22,24]}\n",checks,frames);return 0;
}
