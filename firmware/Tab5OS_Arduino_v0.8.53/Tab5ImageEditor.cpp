#include "Tab5ImageEditor.h"
#include <misc/cache/instance/lv_image_cache.h>
#include "tools/native-apps/image-editor/image_core.h"
#include "Tab5UiStyle.h"
#include "Tab5TextInput.h"
#include "Tab5Focus.h"
#include "Tab5FilePicker.h"
#include "Tab5ImageWrite.h"
#include "Tab5ImageCodec.h"
#include "Tab5ImageText.h"
#include "Tab5ImageFormats.h"
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5ResourceManager.h"
#include "WriterUsb.h"
#include "Tab5Mouse.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <climits>

namespace Tab5ImageEditor {
namespace {
constexpr size_t MemoryLimit=18u*1024u*1024u;
constexpr int CanvasMaxW=2048,CanvasMaxH=1200;
constexpr uint32_t PaletteColors[]={0x181818,0xffffff,0xd32f2f,0xf57c00,0xfbc02d,0x388e3c,0x0097a7,0x1976d2,0x512da8,0xc2185b,0x795548,0x78909c};
constexpr size_t PaletteCount=sizeof(PaletteColors)/sizeof(PaletteColors[0]);
enum Tool { Brush,Eraser,Select,Move,Rectangle,Ellipse,Line,Text,Pick,Hand,Warp,Fill,ToolCount };
enum Action { None,New,Open,Save,Export,Quit,Undo,Redo,Clear,Deselect,Crop,ApplyAdjust,ApplyTransform,ApplySize,LayerAdd,LayerDuplicate,LayerDelete,LayerUp,LayerDown,LayerMerge,LayerRename,ExportJpeg,Fit,Actual,ZoomIn,ZoomOut,ShowColor,ShowLayer,ShowAdjust,ShowTransform,ShowFormat,ToolOptions,UseHand,UsePick,SwapColors,ShowBackground,ToggleOptions,SelectAll,InvertSelection,SoftenSelection,CopySelection,CutSelection,PasteSelection,DeleteSelection };
enum Dialog { NoDialog,ConfirmDiscard,InputPath,InputName,ConfirmOverwrite,NewSize };
enum Slider { Size,Opacity,Hardness,Diffusion,Red,Green,Blue,Bright,Contrast,Saturation,GainR,GainG,GainB,ScaleX,ScaleY,Shear,Thickness,TextScale,LayerOpacity,Strength,Tolerance,Feather,EdgeThreshold,Density,PatternScale,ReliefExtent,ReliefHeight,ReliefAzimuth,ReliefIntensity,SliderCount };
struct Allocation { size_t bytes; uint64_t alignment; };
struct State {
 IEDoc doc{};
 lv_obj_t *root=nullptr,*toolbar=nullptr,*workspace=nullptr,*canvas=nullptr,*selection=nullptr,*panel=nullptr,*body=nullptr,*tab=nullptr,*status=nullptr,*undo=nullptr,*redo=nullptr,*rail=nullptr,*zoomLabel=nullptr,*foreground=nullptr,*background=nullptr,*optionsButton=nullptr,*textInput=nullptr,*textLayout=nullptr;
 lv_obj_t* toolButtons[ToolCount]{};lv_obj_t* paletteButtons[PaletteCount]{};
 lv_obj_t *modal=nullptr,*dialogStatus=nullptr,*fields[3]{},*dialogSubmit=nullptr,*layerList=nullptr;
 lv_obj_t* sliders[SliderCount]{};lv_obj_t* sliderLabels[SliderCount]{};
 lv_image_dsc_t image{};
 uint16_t* pixels=nullptr;size_t canvasBytes=0,memoryBytes=0;
 int width=1100,height=610,canvasW=0,canvasH=0,canvasX=0,canvasY=0;
 int displayW=0,displayH=0,panelWidth=290;
 Tool tool=Brush;int tabIndex=0;
 IEJob* job=nullptr;IEPoint* lasso=nullptr;unsigned lassoCount=0;
 lv_obj_t *jobOverlay=nullptr,*jobBar=nullptr,*jobLabel=nullptr;unsigned jobPercent=101;
 int selectMethod=0,selectCombine=0,tolerance=32,feather=0,sampleMerged=1,edgeStop=0,edgeThreshold=48;
 int fillKind=0,fillRegion=0,fillDensity=100,fillScale=12,fillDirection=0;
 int reliefProfile=IE_RELIEF_CIRCULAR,reliefUnit=IE_RELIEF_PERCENT,reliefPixels=32,reliefPercent=100,reliefHeight=16,reliefAzimuth=225,reliefIntensity=100;
 IEBrush brush{IE_RGBA(24,24,24,255),2,100,100,0,0,0};
 int brightness=0,contrast=0,saturation=0,redGain=100,greenGain=100,blueGain=100,allLayers=0;
 int scaleX=100,scaleY=100,shear=0,turns=0,thickness=2,fill=1,outline=1,textScale=100;
 int red=24,green=24,blue=24,backRed=255,backGreen=255,backBlue=255,layerOpacity=255;
 uint32_t backgroundColor=0xffffff,gestureColor=0;
 bool panelVisible=false,editingBackground=false,hovering=false,textEditing=false,textDirty=false,finishingText=false,pointerKnown=false,textPending=false;
 int pointerX=0,pointerY=0;
 int hoverX=0,hoverY=0,textX=0,textY=0,textW=0,textH=0;uint32_t textColor=0,textCaret=UINT32_MAX,textSelectionStart=UINT32_MAX,textSelectionEnd=UINT32_MAX;uint8_t gestureButton=0;
 int zoom=100,viewDocW=0,viewDocH=0,toolbarHeight=50,ellipseCenter=0,ellipseCircle=0,warpStrength=50,warpMode=0;
 int32_t originX=0,originY=0,panOriginX=0,panOriginY=0;
 int panX=0,panY=0;bool panning=false,fitPending=true;uint8_t mouseButtons=0;
 bool shown=false,working=false,dragging=false,gesture=false,gestureDirty=false,closing=false;
 bool initialized=false,pathProject=false;int sx=0,sy=0,lx=0,ly=0;
 Dialog dialog=NoDialog;Action pending=None;
 char path[261]{},pendingPath[261]{},text[768]{},message[256]{};
 uint32_t palette=0,lastYield=0,foregroundStamp=UINT32_MAX,backgroundStamp=UINT32_MAX;
 const lv_font_t* font=nullptr;
 size_t renderedPixels=0;unsigned regionalRenders=0,fullRenders=0;
};
State s;
Hooks hooks;
lv_obj_t* host=nullptr;
const char* tr(const char* text){return hooks.translate?hooks.translate(text):text;}
lv_color_t color(unsigned role){return lv_color_hex(Tab5UiStyle::color(role));}
void copy(char* out,size_t capacity,const char* source){
 if(!capacity)return;source=source?source:"";size_t length=std::min(capacity-1,strlen(source));
 if(source[length])while(length&&(static_cast<unsigned char>(source[length])&0xc0)==0x80)--length;
 memcpy(out,source,length);out[length]=0;
}
uint32_t stamp(){uint32_t value=Tab5UiStyle::radius();for(unsigned i=0;i<9;++i)value=value*33+Tab5UiStyle::color(i);return value;}
void info(const char* message){copy(s.message,sizeof(s.message),message);if(s.status)lv_label_set_text(s.status,tr(message));}
void finishJob(bool cancel);void startSelection(int method);void refreshHover();void status();void panel();void swatches();void toolState();bool finishText(bool commit);void textChanged();void textCursor();void optionsVisible(bool value);void fitView();void zoomAt(int,int,int);void render(int x,int y,int width,int height);void renderAll();void layout();void action(Action);void cancelGesture();void closeDialog();void dialogSubmit();void request(Action);
void* allocate(void*,size_t bytes){
 if(!bytes||bytes>SIZE_MAX-sizeof(Allocation))return nullptr;
 const size_t total=bytes+sizeof(Allocation);
 if(total>MemoryLimit-s.memoryBytes)return nullptr;
 // Every model/frame allocation is admitted against all OS pending budgets.
 // This runs only at an operation boundary, never from rendering or a timer.
 auto reservation=tab5ReserveMemory(Tab5Resources::Owner::Image,{0,0,0,total,total},"Dessin");
 if(!reservation)return nullptr;
 auto* memory=static_cast<Allocation*>(heap_caps_malloc(total,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!memory)return nullptr;memory->bytes=total;s.memoryBytes+=total;reservation.allocated(0,total);return memory+1;
}
void release(void*,void* data){if(!data)return;auto* memory=static_cast<Allocation*>(data)-1;s.memoryBytes-=memory->bytes;heap_caps_free(memory);}
void cooperate(void*){
 // Yield to the scheduler, never re-enter LVGL or collect document memory.
 const uint32_t now=millis();if(uint32_t(now-s.lastYield)>=16){s.lastYield=now;delay(1);}
}
uint32_t paint(){return IE_RGBA(s.red,s.green,s.blue,255);}
uint32_t secondPaint(){return IE_RGBA(s.backgroundColor>>16,(s.backgroundColor>>8)&255,s.backgroundColor&255,255);}
bool supported(const char* path){return Tab5ImageFormats::supported(path); }
void enabled(lv_obj_t* object,bool value){if(!object)return;if(value)lv_obj_remove_state(object,LV_STATE_DISABLED);else lv_obj_add_state(object,LV_STATE_DISABLED);}
lv_obj_t* label(lv_obj_t* parent,const char* value,int x,int y,int width){auto* o=lv_label_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_label_set_text(o,tr(value));return o;}
lv_obj_t* button(lv_obj_t* parent,const char* value,int x,int y,int width,lv_event_cb_t callback,void* context=nullptr){
 auto* o=lv_button_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,width,42);Tab5UiStyle::button(o);
 lv_obj_set_style_pad_all(o,6,0);auto* l=lv_label_create(o);lv_obj_set_height(l,28);lv_label_set_text(l,tr(value));lv_obj_set_width(l,width-12);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(l);
 lv_obj_add_event_cb(o,callback,LV_EVENT_CLICKED,context);Tab5Focus.describe(o,tr(value));return o;
}
lv_obj_t* command(lv_obj_t* parent,const char* value,int x,int y,int width,Action a){return button(parent,value,x,y,width,[](lv_event_t* e){request(static_cast<Action>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e))));},reinterpret_cast<void*>(uintptr_t(a)));}
void stylePanel(lv_obj_t* object,unsigned role){lv_obj_set_style_bg_color(object,color(role),0);lv_obj_set_style_bg_opa(object,LV_OPA_COVER,0);lv_obj_set_style_border_color(object,color(3),0);lv_obj_set_style_border_width(object,0,0);lv_obj_set_style_pad_all(object,0,0);lv_obj_set_style_radius(object,0,0);lv_obj_set_style_text_color(object,color(4),0);}
void appearance(){
 if(!s.root)return;s.palette=stamp();s.font=hooks.font?hooks.font():lv_obj_get_style_text_font(host,LV_PART_MAIN);
 lv_obj_set_style_text_font(s.root,s.font,0);stylePanel(s.root,0);stylePanel(s.toolbar,0);stylePanel(s.panel,1);if(s.body)stylePanel(s.body,1);stylePanel(s.workspace,2);if(s.rail)stylePanel(s.rail,1);
 Tab5UiStyle::tree(s.root);if(s.modal){lv_obj_set_style_text_font(s.modal,s.font,0);Tab5UiStyle::tree(s.modal);}s.foregroundStamp=s.backgroundStamp=UINT32_MAX;swatches();
}
const char* toolNames[]={"Pinceau","Gomme","Sélection","Déplacer","Rectangle","Ellipse","Ligne","Texte","Pipette","Main","Déformation locale","Remplissage"};
int screenCoord(int value,int32_t origin){const int64_t n=(int64_t(value)*65536-origin)*s.zoom;const int64_t d=6553600;return int(n>=0?n/d:-((-n+d-1)/d));}
int screenX(int x){return screenCoord(x,s.originX);}
int screenY(int y){return screenCoord(y,s.originY);}
void swatches(){
 const uint32_t foreground=(s.red<<16)|(s.green<<8)|s.blue;
 if(s.foreground&&foreground!=s.foregroundStamp){lv_obj_set_style_bg_color(s.foreground,lv_color_hex(foreground),0);s.foregroundStamp=foreground;}
 if(s.background&&s.backgroundColor!=s.backgroundStamp){lv_obj_set_style_bg_color(s.background,lv_color_hex(s.backgroundColor),0);s.backgroundStamp=s.backgroundColor;}
}
void choosePalette(uint32_t c,bool secondary){
 if(!finishText(true))return;cancelGesture();s.editingBackground=secondary;
 if(secondary){s.backgroundColor=c;s.backRed=c>>16;s.backGreen=(c>>8)&255;s.backBlue=c&255;}
 else{s.red=c>>16;s.green=(c>>8)&255;s.blue=c&255;}
 panel();status();
}
void toolState(){for(int i=0;i<ToolCount;++i)if(s.toolButtons[i]){if(i==int(s.tool))lv_obj_add_state(s.toolButtons[i],LV_STATE_CHECKED);else lv_obj_remove_state(s.toolButtons[i],LV_STATE_CHECKED);}swatches();}
void shapeCoords(int& x0,int& y0,int& x1,int& y1){
 x0=s.sx;y0=s.sy;x1=s.lx;y1=s.ly;
 if(s.tool!=Ellipse)return;
 int dx=x1-x0,dy=y1-y0;
 if(s.ellipseCircle){const int d=std::min(abs(dx),abs(dy));dx=dx<0?-d:d;dy=dy<0?-d:d;}
 if(s.ellipseCenter){int rx=abs(dx),ry=abs(dy);rx=std::min(rx,std::min(x0,s.doc.current.width-1-x0));ry=std::min(ry,std::min(y0,s.doc.current.height-1-y0));if(s.ellipseCircle)rx=ry=std::min(rx,ry);x1=x0+rx;y1=y0+ry;x0-=rx;y0-=ry;}
 else {if(s.ellipseCircle){const int maxx=dx<0?x0:s.doc.current.width-1-x0,maxy=dy<0?y0:s.doc.current.height-1-y0;const int d=std::min(abs(dx),std::min(maxx,maxy));dx=dx<0?-d:d;dy=dy<0?-d:d;}x1=x0+dx;y1=y0+dy;}
}
void status(){
 if(!s.initialized)return;enabled(s.undo,s.doc.undo_count>0);enabled(s.redo,s.doc.redo_count>0);
 char suffix[160]{};if(s.hovering&&!s.dragging)snprintf(suffix,sizeof(suffix)," · (%d, %d)",s.hoverX,s.hoverY);if(s.dragging){int x0=s.sx,y0=s.sy,x1=s.lx,y1=s.ly;shapeCoords(x0,y0,x1,y1);double dx=x1-x0,dy=y1-y0;snprintf(suffix,sizeof(suffix)," · (%d, %d) → (%d, %d) · %d × %d · %s %.1f · %s %.1f°",x0,y0,x1,y1,abs(x1-x0)+1,abs(y1-y0)+1,tr("Longueur"),sqrt(dx*dx+dy*dy),tr("Angle"),atan2(dy,dx)*180.0/3.141592653589793);}
 char line[600];snprintf(line,sizeof(line),"%d × %d · %d%% · %s %d/%d · %s %u/%u%s%s%s%s",s.doc.current.width,s.doc.current.height,s.zoom,tr("Calque"),s.doc.current.active+1,s.doc.current.count,tr("Historique"),s.doc.undo_count,s.doc.redo_count,s.doc.dirty?" · *":"",suffix,s.message[0]&&!s.dragging?" — ":"",s.message[0]&&!s.dragging?tr(s.message):"");
 if(s.dragging)snprintf(line,sizeof(line),"%s %u/%u%s",tr("Historique"),s.doc.undo_count,s.doc.redo_count,suffix);
 else if(s.hovering)snprintf(line,sizeof(line),"(%d, %d) · %d × %d · %d%% · %s %u/%u%s",s.hoverX,s.hoverY,s.doc.current.width,s.doc.current.height,s.zoom,tr("Historique"),s.doc.undo_count,s.doc.redo_count,s.doc.dirty?" · *":"");
 if(s.status&&strcmp(lv_label_get_text(s.status),line))lv_label_set_text(s.status,line);
 if(s.zoomLabel){char z[16];snprintf(z,sizeof(z),"%d%%",s.zoom);if(strcmp(lv_label_get_text(s.zoomLabel),z))lv_label_set_text(s.zoomLabel,z);}swatches();
}
void previewDraw(lv_event_t* e){
 if(!s.initialized)return;auto* layer=lv_event_get_layer(e);lv_area_t a;lv_obj_get_coords(s.selection,&a);
 auto line=[&](int x0,int y0,int x1,int y1){lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.color=color(6);d.width=2;d.p1={lv_value_precise_t(a.x1+x0),lv_value_precise_t(a.y1+y0)};d.p2={lv_value_precise_t(a.x1+x1),lv_value_precise_t(a.y1+y1)};lv_draw_line(layer,&d);};
 auto rect=[&](int x0,int y0,int x1,int y1){line(x0,y0,x1,y0);line(x1,y0,x1,y1);line(x1,y1,x0,y1);line(x0,y1,x0,y0);};
 const auto r=s.doc.current.selection;if(r.enabled&&r.w>0&&r.h>0&&!s.doc.current.selection_mask)rect(screenX(r.x),screenY(r.y),screenX(r.x+r.w),screenY(r.y+r.h));
 if(s.textEditing&&s.textInput&&s.textLayout){
  auto* textLabel=lv_textarea_get_label(s.textInput);const uint32_t first=lv_label_get_text_selection_start(textLabel),last=lv_label_get_text_selection_end(textLabel),caret=lv_textarea_get_cursor_pos(s.textInput);
  const int h=Tab5ImageText::lineHeight(s.textLayout,s.textScale);const auto cursor=Tab5ImageText::position(s.textLayout,caret,s.textScale);
  line(screenX(s.textX+cursor.x),screenY(s.textY+cursor.y),screenX(s.textX+cursor.x),screenY(s.textY+cursor.y+h));
  if(first!=LV_DRAW_LABEL_NO_TXT_SEL&&last!=LV_DRAW_LABEL_NO_TXT_SEL){
   uint32_t count=0;for(const unsigned char* p=reinterpret_cast<const unsigned char*>(s.text);*p;++p)if((*p&0xc0)!=0x80)++count;
   for(uint32_t i=std::min(first,last);i<std::min(std::max(first,last),count);++i){const auto p=Tab5ImageText::position(s.textLayout,i,s.textScale),q=Tab5ImageText::position(s.textLayout,i+1,s.textScale);const int end=q.y==p.y?q.x:p.x+std::max(1,h/3);rect(screenX(s.textX+std::min<int>(p.x,end)),screenY(s.textY+p.y),screenX(s.textX+std::max<int>(p.x,end)),screenY(s.textY+p.y+h));}
  }
 }
 if(!s.dragging||(s.tool!=Select&&s.tool!=Rectangle&&s.tool!=Ellipse&&s.tool!=Line&&s.tool!=Move))return;
 int x0=s.sx,y0=s.sy,x1=s.lx,y1=s.ly;shapeCoords(x0,y0,x1,y1);
 if(s.tool==Move&&r.enabled){x0=r.x+s.lx-s.sx;y0=r.y+s.ly-s.sy;x1=x0+r.w;y1=y0+r.h;}
 x0=screenX(x0);y0=screenY(y0);x1=screenX(x1);y1=screenY(y1);
 if(s.tool==Select&&s.selectMethod==IE_SELECT_LASSO&&s.lassoCount){for(unsigned i=1;i<s.lassoCount;++i)line(screenX(s.lasso[i-1].x),screenY(s.lasso[i-1].y),screenX(s.lasso[i].x),screenY(s.lasso[i].y));if(s.lassoCount>2)line(screenX(s.lasso[s.lassoCount-1].x),screenY(s.lasso[s.lassoCount-1].y),screenX(s.lasso[0].x),screenY(s.lasso[0].y));return;}
 if(s.tool==Line){line(x0,y0,x1,y1);return;}
 if(s.tool!=Ellipse&&!(s.tool==Select&&s.selectMethod==IE_SELECT_ELLIPSE)){rect(x0,y0,x1,y1);return;}
 const double cx=(x0+x1)*0.5,cy=(y0+y1)*0.5,rx=abs(x1-x0)*0.5,ry=abs(y1-y0)*0.5;
 int px=int(cx+rx),py=int(cy);for(int i=1;i<=64;++i){double t=i*6.283185307179586/64;int nx=int(std::lround(cx+rx*cos(t))),ny=int(std::lround(cy+ry*sin(t)));line(px,py,nx,ny);px=nx;py=ny;}
}
void selection(){if(s.selection)lv_obj_invalidate(s.selection);}
void render(int x,int y,int width,int height){
 if(!s.pixels||!s.canvas||!s.displayW||!s.initialized)return;
 int x1=std::max(0,screenX(x)-2),y1=std::max(0,screenY(y)-2),x2=std::min(s.displayW,screenX(x+width)+3),y2=std::min(s.displayH,screenY(y+height)+3);if(x1>=x2||y1>=y2)return;
 s.renderedPixels+=ie_render_view_rgb565(&s.doc,s.pixels,s.displayW,s.displayW,s.displayH,s.originX,s.originY,s.zoom,x1,y1,x2-x1,y2-y1,0xeeeeee,0xcccccc,Tab5UiStyle::color(2));ie_render_selection_outline(&s.doc,s.pixels,s.displayW,s.displayW,s.displayH,s.originX,s.originY,s.zoom,x1,y1,x2-x1,y2-y1);++s.regionalRenders;
 lv_area_t a;lv_obj_get_coords(s.canvas,&a);a.x2=a.x1+x2-1;a.y2=a.y1+y2-1;a.x1+=x1;a.y1+=y1;lv_obj_invalidate_area(s.canvas,&a);
}
void renderAll(){if(!s.initialized)return;++s.fullRenders;if(s.pixels&&s.displayW){s.renderedPixels+=ie_render_view_rgb565(&s.doc,s.pixels,s.displayW,s.displayW,s.displayH,s.originX,s.originY,s.zoom,0,0,s.displayW,s.displayH,0xeeeeee,0xcccccc,Tab5UiStyle::color(2));ie_render_selection_outline(&s.doc,s.pixels,s.displayW,s.displayW,s.displayH,s.originX,s.originY,s.zoom,0,0,s.displayW,s.displayH);lv_obj_invalidate(s.canvas);}selection();refreshHover();status();}
int32_t boundOrigin(int64_t value){constexpr int64_t limit=int64_t(IE_MAX_DIMENSION+4096)*65536;return int32_t(std::clamp<int64_t>(value,-limit,limit));}
void fitView(){if(!s.displayW||!s.initialized){s.fitPending=true;return;}s.zoom=std::clamp(std::min(s.displayW*100/s.doc.current.width,s.displayH*100/s.doc.current.height),5,1600);s.originX=boundOrigin((int64_t(s.doc.current.width)*65536-int64_t(s.displayW)*6553600/s.zoom)/2);s.originY=boundOrigin((int64_t(s.doc.current.height)*65536-int64_t(s.displayH)*6553600/s.zoom)/2);s.fitPending=false;renderAll();}
void zoomAt(int value,int px,int py){if(!finishText(true))return;cancelGesture();value=std::clamp(value,5,1600);if(value==s.zoom)return;const int64_t ax=int64_t(s.originX)+int64_t(px)*6553600/s.zoom,ay=int64_t(s.originY)+int64_t(py)*6553600/s.zoom;s.zoom=value;s.originX=boundOrigin(ax-int64_t(px)*6553600/value);s.originY=boundOrigin(ay-int64_t(py)*6553600/value);renderAll();}
void result(int ok){if(!ok)info(ie_error(&s.doc));else s.message[0]=0;layout();renderAll();}
void cancelGesture(){
 if(s.lasso){release(nullptr,s.lasso);s.lasso=nullptr;}s.lassoCount=0;
 if(s.gesture){ie_cancel(&s.doc);s.doc.dirty=s.gestureDirty;}
 const bool changed=s.gesture,preview=s.dragging;s.gesture=s.dragging=s.panning=false;s.gestureButton=0;if(changed)renderAll();else if(preview)selection();status();
}
void panStart(int px,int py){if(!finishText(true))return;cancelGesture();s.panning=true;s.panX=px;s.panY=py;s.panOriginX=s.originX;s.panOriginY=s.originY;}
void panMove(int px,int py){const int32_t x=boundOrigin(int64_t(s.panOriginX)-int64_t(px-s.panX)*6553600/s.zoom),y=boundOrigin(int64_t(s.panOriginY)-int64_t(py-s.panY)*6553600/s.zoom);if(x==s.originX&&y==s.originY)return;s.originX=x;s.originY=y;renderAll();}
// Both mouse buttons and touch use the same gesture path. The colour belongs
// to the press, so a palette change cannot recolour an unfinished stroke.
bool inObject(lv_obj_t* object,int x,int y){if(!object||lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))return false;lv_area_t a;lv_obj_get_coords(object,&a);return x>=a.x1&&x<=a.x2&&y>=a.y1&&y<=a.y2;}
bool dropdownOpen(lv_obj_t* object){if(!object||lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))return false;if(lv_obj_check_type(object,&lv_dropdown_class)&&lv_dropdown_is_open(object))return true;for(unsigned i=0;i<lv_obj_get_child_count(object);++i)if(dropdownOpen(lv_obj_get_child(object,i)))return true;return false;}
bool canvasHit(int px,int py){lv_area_t a;lv_obj_get_coords(s.workspace,&a);return px>=0&&py>=0&&px<s.displayW&&py<s.displayH&&!(s.panelVisible&&inObject(s.panel,px+a.x1,py+a.y1))&&!dropdownOpen(s.root);}
void hover(int px,int py,bool available){
 const int x=ie_view_sample(px,s.originX,s.zoom),y=ie_view_sample(py,s.originY,s.zoom);const bool hit=available&&x>=0&&y>=0&&x<s.doc.current.width&&y<s.doc.current.height;
 if(hit!=s.hovering||(hit&&(x!=s.hoverX||y!=s.hoverY))){s.hovering=hit;s.hoverX=x;s.hoverY=y;status();}
}
void refreshHover(){if(!s.pointerKnown||!s.workspace||!s.initialized)return;lv_area_t a;lv_obj_get_coords(s.workspace,&a);const int x=s.pointerX-a.x1,y=s.pointerY-a.y1;hover(x,y,canvasHit(x,y));}

void restoreTextArea(){
 if(!s.textW||!s.textH||!s.doc.undo_count)return;const auto* original=s.doc.undo[s.doc.undo_count-1];
 const int x0=std::max(0,s.textX),y0=std::max(0,s.textY),x1=std::min(s.doc.current.width,s.textX+s.textW),y1=std::min(s.doc.current.height,s.textY+s.textH);
 if(x0>=x1)return;auto* pixels=s.doc.current.layers[s.doc.current.active].pixels;const auto* before=original->layers[s.doc.current.active].pixels;
 for(int y=y0;y<y1;++y){memcpy(pixels+size_t(y)*s.doc.current.width+x0,before+size_t(y)*s.doc.current.width+x0,size_t(x1-x0)*4);if((y&31)==0)cooperate(nullptr);}
}
void textCursor(){
 if(!s.textEditing||!s.textInput||!s.textLayout)return;const unsigned caret=lv_textarea_get_cursor_pos(s.textInput);s.textCaret=caret;auto* textLabel=lv_textarea_get_label(s.textInput);s.textSelectionStart=lv_label_get_text_selection_start(textLabel);s.textSelectionEnd=lv_label_get_text_selection_end(textLabel);
 const auto p=Tab5ImageText::position(s.textLayout,caret,s.textScale);
 lv_obj_set_pos(s.textInput,std::clamp(screenX(s.textX+p.x),0,std::max(0,s.displayW-2)),std::clamp(screenY(s.textY+p.y),0,std::max(0,s.displayH-2)));selection();
}
void textChanged(){
 if(!s.textEditing||!s.textInput||!s.textLayout)return;
 char pending[sizeof(s.text)];copy(pending,sizeof(pending),lv_textarea_get_text(s.textInput));
 lv_label_set_text(s.textLayout,pending);lv_obj_update_layout(s.textLayout);
 Tab5ImageText::Raster raster;
 if(!Tab5ImageText::prepare(raster,&s.doc,s.textLayout,pending,s.textX,s.textY,s.textScale)){
  lv_label_set_text(s.textLayout,s.text);lv_obj_update_layout(s.textLayout);s.textPending=true;info("Mémoire insuffisante pour le texte");textCursor();return;
 }
 const int oldW=s.textW,oldH=s.textH;restoreTextArea();copy(s.text,sizeof(s.text),pending);s.textPending=false;s.message[0]=0;
 s.textW=raster.textW;s.textH=raster.textH;Tab5ImageText::stamp(raster,&s.doc,s.textColor,s.brush.opacity);Tab5ImageText::release(raster,&s.doc);
 render(s.textX,s.textY,std::max(oldW,s.textW)+1,std::max(oldH,s.textH)+1);textCursor();status();
}
bool finishText(bool commit){
 if(!s.textEditing||s.finishingText)return true;
 if(commit&&s.textPending){textChanged();if(s.textPending)return false;}
 s.finishingText=true;const bool keep=commit&&s.text[0];const int x=s.textX,y=s.textY,w=s.textW,h=s.textH;
 s.textEditing=false;auto* input=s.textInput;s.textInput=nullptr;if(input&&Tab5Focus.focused()==input)Tab5Focus.clear();if(input)lv_obj_delete(input);if(s.textLayout){lv_obj_delete(s.textLayout);s.textLayout=nullptr;}
 if(keep)ie_commit(&s.doc);else{ie_cancel(&s.doc);s.doc.dirty=s.textDirty;}
 s.textW=s.textH=0;s.textPending=false;if(hooks.keyboard)hooks.keyboard(false,nullptr);s.finishingText=false;render(x,y,w+1,h+1);selection();status();return true;
}
void startText(int x,int y,uint32_t ink){
 if(!finishText(true))return;cancelGesture();s.textDirty=s.doc.dirty;if(!ie_gesture_begin(&s.doc)){info(ie_error(&s.doc));status();return;}
 s.textEditing=true;s.textPending=false;s.textX=x;s.textY=y;s.textW=s.textH=0;s.textColor=ink;s.text[0]=0;s.message[0]=0;
 s.textLayout=lv_label_create(s.workspace);lv_obj_remove_style_all(s.textLayout);lv_obj_set_style_text_font(s.textLayout,s.font,0);lv_obj_set_style_base_dir(s.textLayout,LV_BASE_DIR_AUTO,0);lv_obj_set_size(s.textLayout,LV_SIZE_CONTENT,LV_SIZE_CONTENT);lv_label_set_text(s.textLayout,"");lv_obj_add_flag(s.textLayout,LV_OBJ_FLAG_HIDDEN);
 s.textInput=lv_textarea_create(s.workspace);lv_obj_set_size(s.textInput,2,2);lv_obj_set_style_opa(s.textInput,LV_OPA_TRANSP,0);lv_obj_remove_flag(s.textInput,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(s.textInput,LV_OBJ_FLAG_SCROLL_ON_FOCUS);lv_textarea_set_one_line(s.textInput,true);lv_obj_set_size(s.textInput,2,2);lv_textarea_set_max_length(s.textInput,128);lv_textarea_set_text(s.textInput,"");
 Tab5Focus.describe(s.textInput,tr("Texte"));lv_obj_add_event_cb(s.textInput,[](lv_event_t*){textChanged();},LV_EVENT_VALUE_CHANGED,nullptr);lv_obj_add_event_cb(s.textInput,[](lv_event_t*){finishText(true);},LV_EVENT_READY,nullptr);lv_obj_add_event_cb(s.textInput,[](lv_event_t*){finishText(false);},LV_EVENT_CANCEL,nullptr);
 textCursor();Tab5Focus.editObject(s.textInput);if(hooks.keyboard)hooks.keyboard(!writing::usbConnected(),s.textInput);
 const int cy=screenY(y+Tab5ImageText::lineHeight(s.textLayout,s.textScale));if(cy>=s.displayH){s.originY=boundOrigin(int64_t(s.originY)+int64_t(cy-s.displayH+8)*6553600/s.zoom);renderAll();}textCursor();status();
}
void textCaretAt(int x,int y){
 if(!s.textInput||!s.textLayout)return;lv_point_t pos{(x-s.textX)*100/s.textScale,(y-s.textY)*100/s.textScale};const uint32_t index=lv_label_get_letter_on(s.textLayout,&pos,true);
 lv_textarea_set_cursor_pos(s.textInput,index);auto* label=lv_textarea_get_label(s.textInput);lv_label_set_text_selection_start(label,LV_DRAW_LABEL_NO_TXT_SEL);lv_label_set_text_selection_end(label,LV_DRAW_LABEL_NO_TXT_SEL);Tab5Focus.editObject(s.textInput);textCursor();
}
void finishJob(bool cancel){
 if(!s.job)return;auto* job=s.job;s.job=nullptr;if(cancel)ie_job_cancel(job);ie_job_destroy(job);s.working=false;
 if(s.jobOverlay){if(Tab5Focus.contains(s.jobOverlay,Tab5Focus.focused()))Tab5Focus.clear();lv_obj_delete(s.jobOverlay);}s.jobOverlay=s.jobBar=s.jobLabel=nullptr;
 if(cancel)info("Opération annulée ; image conservée");else if(*ie_error(&s.doc))info(ie_error(&s.doc));else s.message[0]=0;
 renderAll();status();
}
void startJob(IEJob* job,const char* title){
 if(!job){info(ie_error(&s.doc));status();return;}s.job=job;s.working=true;s.jobPercent=101;
 s.jobOverlay=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(s.jobOverlay);lv_obj_set_size(s.jobOverlay,lv_pct(100),lv_pct(100));lv_obj_set_style_bg_color(s.jobOverlay,color(0),0);lv_obj_set_style_bg_opa(s.jobOverlay,LV_OPA_70,0);lv_obj_remove_flag(s.jobOverlay,LV_OBJ_FLAG_SCROLLABLE);
 auto* card=lv_obj_create(s.jobOverlay);lv_obj_set_size(card,lv_pct(60),LV_SIZE_CONTENT);lv_obj_center(card);stylePanel(card,1);lv_obj_set_style_text_font(card,s.font,0);lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_border_color(card,color(3),0);lv_obj_set_style_radius(card,Tab5UiStyle::radius(),0);lv_obj_set_style_pad_all(card,20,0);lv_obj_set_flex_flow(card,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(card,18,0);
 auto* heading=label(card,title,0,0,500);lv_obj_set_width(heading,lv_pct(100));s.jobBar=lv_bar_create(card);lv_obj_set_size(s.jobBar,lv_pct(100),18);lv_bar_set_range(s.jobBar,0,100);lv_obj_set_style_bg_color(s.jobBar,color(3),LV_PART_MAIN);lv_obj_set_style_bg_color(s.jobBar,color(6),LV_PART_INDICATOR);
 s.jobLabel=label(card,"0 %",0,0,500);lv_obj_set_width(s.jobLabel,lv_pct(100));auto* cancel=button(card,"Annuler",0,0,180,[](lv_event_t*){finishJob(true);});Tab5Focus.focusInitial(s.jobOverlay,cancel);
}
void startSelection(int method){
 IESelectionOptions q{};q.method=method;q.combine=(method==IE_SELECT_EXISTING||method==IE_SELECT_INVERT)?IE_SELECT_REPLACE:s.selectCombine;q.x0=s.sx;q.y0=s.sy;q.x1=s.lx;q.y1=s.ly;q.tolerance=s.tolerance;q.feather=method==IE_SELECT_INVERT?0:s.feather;q.sample_merged=s.sampleMerged;q.edge=s.edgeStop?s.edgeThreshold:0;q.points=s.lasso;q.point_count=s.lassoCount;
 if(method==IE_SELECT_RECT&&q.combine==IE_SELECT_REPLACE&&!q.feather){ie_select(&s.doc,q.x0,q.y0,q.x1,q.y1);renderAll();return;}
 startJob(ie_selection_start(&s.doc,&q),"Sélection en cours…");
}
void startFill(int x,int y,uint32_t ink){
 IEFillOptions q{};q.kind=s.fillKind;q.region=s.fillRegion;q.x=x;q.y=y;q.tolerance=s.tolerance;q.feather=s.feather;q.sample_merged=s.sampleMerged;q.edge=s.edgeStop?s.edgeThreshold:0;q.opacity=s.brush.opacity;q.density=s.fillDensity;q.scale=s.fillScale;q.direction=s.fillDirection;q.foreground=ink;q.background=ink==paint()?secondPaint():paint();
 q.relief_profile=s.reliefProfile;q.relief_unit=s.reliefUnit;q.relief_extent=s.reliefUnit==IE_RELIEF_PERCENT?s.reliefPercent:s.reliefPixels;q.relief_height=s.reliefHeight;q.relief_azimuth=s.reliefAzimuth;q.relief_intensity=s.reliefIntensity;
 startJob(ie_fill_start(&s.doc,&q),"Remplissage en cours…");
}
void lassoPoint(int x,int y){
 if(!s.lasso)return;if(s.lassoCount&&abs(x-s.lasso[s.lassoCount-1].x)+abs(y-s.lasso[s.lassoCount-1].y)<2)return;
 if(s.lassoCount==2048){for(unsigned i=1;i<1024;++i)s.lasso[i]=s.lasso[2*i];s.lassoCount=1024;}
 s.lasso[s.lassoCount++]={x,y};
}
void gesturePointer(lv_event_code_t code,int px,int py,bool secondary){
 const uint8_t owner=secondary?2:1;const bool hit=canvasHit(px,py);hover(px,py,hit);
 if(code==LV_EVENT_PRESS_LOST){if(s.gestureButton==owner)cancelGesture();return;}
 if(code==LV_EVENT_PRESSED&&!hit)return;
 if(code!=LV_EVENT_PRESSED&&s.gestureButton!=owner)return;
 if(s.tool==Hand){if(code==LV_EVENT_PRESSED){panStart(px,py);s.gestureButton=owner;}else if(s.panning){panMove(px,py);if(code==LV_EVENT_RELEASED){s.panning=false;s.gestureButton=0;}}return;}
 int rawx=ie_view_sample(px,s.originX,s.zoom),rawy=ie_view_sample(py,s.originY,s.zoom);
 if(code==LV_EVENT_PRESSED&&(rawx<0||rawy<0||rawx>=s.doc.current.width||rawy>=s.doc.current.height))return;
 // A visible options pane owns its covered pixels even during a captured drag.
 lv_area_t a;lv_obj_get_coords(s.workspace,&a);if(code!=LV_EVENT_PRESSED&&s.panelVisible&&inObject(s.panel,px+a.x1,py+a.y1)){rawx=s.lx;rawy=s.ly;}
 const int x=std::clamp(rawx,0,s.doc.current.width-1),y=std::clamp(rawy,0,s.doc.current.height-1);
 if(code==LV_EVENT_PRESSED){
  if(s.tool==Text&&s.textEditing&&x>=s.textX&&y>=s.textY&&x<=s.textX+std::max(s.textW,1)&&y<s.textY+std::max(s.textH,Tab5ImageText::lineHeight(s.textLayout,s.textScale))){textCaretAt(x,y);return;}
  if(!finishText(true))return;cancelGesture();Tab5Focus.clear();s.gestureButton=owner;s.gestureColor=secondary?secondPaint():paint();s.sx=s.lx=x;s.sy=s.ly=y;s.message[0]=0;
  if(s.tool==Pick){auto c=ie_composite(&s.doc,x,y);if(secondary){s.backgroundColor=((c&255)<<16)|(c&0xff00)|((c>>16)&255);s.backRed=s.backgroundColor>>16;s.backGreen=(s.backgroundColor>>8)&255;s.backBlue=s.backgroundColor&255;}else{s.red=c&255;s.green=(c>>8)&255;s.blue=(c>>16)&255;}s.gestureButton=0;panel();status();return;}
  if(s.tool==Text){const auto ink=s.gestureColor;s.gestureButton=0;startText(x,y,ink);return;}
  if(s.tool==Fill){s.gestureButton=0;startFill(x,y,s.gestureColor);return;}
  if(s.tool==Select&&s.selectMethod>=IE_SELECT_CONTIGUOUS){s.gestureButton=0;startSelection(s.selectMethod);return;}
  if(s.tool==Select&&s.selectMethod==IE_SELECT_LASSO){s.lasso=static_cast<IEPoint*>(allocate(nullptr,2048*sizeof(IEPoint)));if(!s.lasso){s.gestureButton=0;info("Mémoire insuffisante pour la sélection ; image conservée");status();return;}lassoPoint(x,y);}
  s.dragging=true;
  if(s.tool==Brush||s.tool==Eraser||s.tool==Warp){s.gestureDirty=s.doc.dirty;const int ok=s.tool==Warp?ie_warp_begin(&s.doc,s.brush.size):ie_brush_begin(&s.doc);if(!ok){s.dragging=false;s.gestureButton=0;info(ie_error(&s.doc));status();return;}s.gesture=true;s.brush.color=s.gestureColor;s.brush.erase=s.tool==Eraser;if(s.tool!=Warp)ie_brush(&s.doc,x,y,x,y,&s.brush);else if(s.warpMode)ie_warp_brush(&s.doc,x,y,x,y,s.brush.size,s.warpStrength,s.warpMode);int r=s.brush.size/2+3;render(x-r,y-r,2*r+1,2*r+1);}
  else selection();status();
 }else if((code==LV_EVENT_PRESSING||code==LV_EVENT_RELEASED)&&s.dragging){
  const bool moved=x!=s.lx||y!=s.ly;
  if(moved&&(s.tool==Brush||s.tool==Eraser||s.tool==Warp)){if(s.tool==Warp)ie_warp_brush(&s.doc,s.lx,s.ly,x,y,s.brush.size,s.warpStrength,s.warpMode);else ie_brush(&s.doc,s.lx,s.ly,x,y,&s.brush);int r=s.brush.size/2+3;render(std::min(s.lx,x)-r,std::min(s.ly,y)-r,abs(s.lx-x)+2*r+1,abs(s.ly-y)+2*r+1);}
  if(moved&&s.tool==Select&&s.selectMethod==IE_SELECT_LASSO)lassoPoint(x,y);
  s.lx=x;s.ly=y;if(moved&&(s.tool!=Brush&&s.tool!=Eraser&&s.tool!=Warp))selection();
  if(code==LV_EVENT_RELEASED){
   if(s.tool==Select){startSelection(s.selectMethod);if(s.lasso){release(nullptr,s.lasso);s.lasso=nullptr;}s.lassoCount=0;}
   else if(s.tool==Move)result(ie_translate_selection(&s.doc,x-s.sx,y-s.sy));
   else if(s.tool==Rectangle||s.tool==Ellipse||s.tool==Line){int x0,y0,x1,y1;shapeCoords(x0,y0,x1,y1);if(ie_begin(&s.doc)){ie_shape(&s.doc,s.tool==Rectangle?IE_SHAPE_RECT:s.tool==Ellipse?IE_SHAPE_ELLIPSE:IE_SHAPE_LINE,x0,y0,x1,y1,s.gestureColor,s.brush.opacity,s.thickness,s.fill,s.outline);int r=s.thickness+2;render(std::min(x0,x1)-r,std::min(y0,y1)-r,abs(x0-x1)+2*r+1,abs(y0-y1)+2*r+1);}else info(ie_error(&s.doc));}
   if(s.gesture)ie_commit(&s.doc);s.gesture=s.dragging=false;s.gestureButton=0;if(s.tool!=Brush&&s.tool!=Eraser&&s.tool!=Warp)selection();
  }if(moved||code==LV_EVENT_RELEASED)status();
 }
}
void pointer(lv_event_t* event){
 if(s.working||s.modal||!s.shown||!s.initialized||s.gestureButton==2||(s.mouseButtons&4))return;
 auto code=lv_event_get_code(event);lv_indev_t* input=lv_indev_active();if(!input)return;lv_point_t p;lv_indev_get_point(input,&p);lv_area_t a;lv_obj_get_coords(s.workspace,&a);gesturePointer(code,p.x-a.x1,p.y-a.y1,false);
}
void layout(){
 if(!s.root||!s.initialized)return;
 const int w=std::max(600,s.width),h=std::max(280,s.height);s.panelWidth=std::clamp(w/4,230,310);
 lv_obj_set_size(s.root,w,h);lv_obj_set_width(s.toolbar,w);lv_obj_update_layout(s.toolbar);s.toolbarHeight=std::max(50,int(lv_obj_get_height(s.toolbar)));const int top=s.toolbarHeight+6,available=std::max(100,h-top-38),left=58;
 lv_obj_set_pos(s.rail,0,top);lv_obj_set_size(s.rail,52,available);
 lv_obj_set_pos(s.workspace,left,top);lv_obj_set_size(s.workspace,w-left-6,available);lv_obj_set_pos(s.panel,w-s.panelWidth,top);lv_obj_set_size(s.panel,s.panelWidth,available);lv_obj_set_pos(s.status,8,h-31);lv_obj_set_size(s.status,w-16,28);
 s.canvasW=std::min(CanvasMaxW,w-left-6);s.canvasH=std::min(CanvasMaxH,available);int pw=std::max(1,s.canvasW),ph=std::max(1,s.canvasH);const size_t bytes=size_t(pw)*ph*2;
 if(s.shown&&bytes!=s.canvasBytes){auto* next=static_cast<uint16_t*>(allocate(nullptr,bytes));if(next){lv_image_cache_drop(&s.image);auto* old=s.pixels;s.pixels=next;s.canvasBytes=bytes;release(nullptr,old);}else if(s.pixels){ph=std::min(ph,int(s.canvasBytes/(size_t(pw)*2)));if(ph<1){pw=std::min(pw,int(s.canvasBytes/2));ph=1;}info("Mémoire insuffisante pour l’aperçu ; image conservée");}else {s.displayW=s.displayH=0;lv_image_set_src(s.canvas,nullptr);info("Mémoire insuffisante pour l’aperçu ; image conservée");return;}}
 if(!s.pixels){s.displayW=s.displayH=0;return;}s.displayW=pw;s.displayH=ph;s.canvasX=s.canvasY=0;
 if(s.viewDocW!=s.doc.current.width||s.viewDocH!=s.doc.current.height){s.viewDocW=s.doc.current.width;s.viewDocH=s.doc.current.height;s.fitPending=true;}
 lv_image_cache_drop(&s.image);s.image.header.magic=LV_IMAGE_HEADER_MAGIC;s.image.header.flags=LV_IMAGE_FLAGS_MODIFIABLE;s.image.header.cf=LV_COLOR_FORMAT_RGB565;s.image.header.w=pw;s.image.header.h=ph;s.image.header.stride=pw*2;s.image.data_size=pw*ph*2;s.image.data=reinterpret_cast<uint8_t*>(s.pixels);lv_image_set_src(s.canvas,&s.image);lv_obj_set_pos(s.canvas,0,0);lv_obj_set_pos(s.selection,0,0);lv_obj_set_size(s.selection,pw,ph);if(s.fitPending)fitView();textCursor();
}
bool closeDropdown(lv_obj_t* parent){
 if(!parent)return false;
 if(lv_obj_check_type(parent,&lv_dropdown_class)&&lv_dropdown_is_open(parent)){lv_dropdown_close(parent);Tab5Focus.finishEditing();return true;}
 for(unsigned i=0;i<lv_obj_get_child_count(parent);++i)if(closeDropdown(lv_obj_get_child(parent,i)))return true;
 return false;
}
void closeDialog(){
 if(!s.modal)return;if(hooks.keyboard)hooks.keyboard(false,nullptr);
 if(Tab5Focus.contains(s.modal,Tab5Focus.focused()))Tab5Focus.clear();auto* old=s.modal;
 s.modal=nullptr;s.dialogStatus=s.dialogSubmit=nullptr;for(auto& o:s.fields)o=nullptr;s.dialog=NoDialog;lv_obj_delete(old);
 if(s.root&&s.shown)Tab5Focus.restore(s.root);
}
lv_obj_t* field(lv_obj_t* parent,const char* value,int y,bool numeric=false){
 auto* o=lv_textarea_create(parent);lv_obj_set_pos(o,0,y);lv_obj_set_size(o,lv_pct(100),48);lv_textarea_set_one_line(o,true);lv_textarea_set_max_length(o,numeric?4:260);if(numeric)lv_textarea_set_accepted_chars(o,"0123456789");lv_textarea_set_text(o,value);Tab5UiStyle::control(o);
 lv_obj_add_event_cb(o,[](lv_event_t* e){auto* o=lv_event_get_target_obj(e);Tab5Focus.editObject(o);if(hooks.keyboard)hooks.keyboard(!writing::usbConnected(),o);},LV_EVENT_CLICKED,nullptr);
 lv_obj_add_event_cb(o,[](lv_event_t* e){if(s.dialog==NewSize&&lv_event_get_target_obj(e)==s.fields[0]){Tab5Focus.editObject(s.fields[1]);if(hooks.keyboard)hooks.keyboard(!writing::usbConnected(),s.fields[1]);}else dialogSubmit();},LV_EVENT_READY,nullptr);return o;
}
void dialog(Dialog kind,Action pending,const char* title,const char* initial=nullptr){
 if(!finishText(true))return;closeDialog();cancelGesture();s.dialog=kind;s.pending=pending;Tab5Focus.remember(s.root);
 s.modal=lv_obj_create(lv_screen_active());
 lv_obj_add_event_cb(s.modal,[](lv_event_t* e){if(lv_event_get_target_obj(e)!=s.modal)return;s.modal=nullptr;s.dialogStatus=s.dialogSubmit=nullptr;for(auto& field:s.fields)field=nullptr;s.dialog=NoDialog;if(hooks.keyboard)hooks.keyboard(false,nullptr);},LV_EVENT_DELETE,nullptr);
 lv_obj_set_pos(s.modal,0,0);lv_obj_set_size(s.modal,lv_pct(100),lv_pct(100));lv_obj_set_style_pad_all(s.modal,0,0);lv_obj_set_style_bg_color(s.modal,lv_color_black(),0);lv_obj_set_style_bg_opa(s.modal,LV_OPA_50,0);lv_obj_set_style_border_width(s.modal,0,0);lv_obj_remove_flag(s.modal,LV_OBJ_FLAG_SCROLLABLE);if(s.font)lv_obj_set_style_text_font(s.modal,s.font,0);
 const int width=std::min(700,int(lv_display_get_horizontal_resolution(nullptr))-32);
 auto* box=lv_obj_create(s.modal);lv_obj_set_size(box,width,kind==NewSize?385:kind==ConfirmDiscard?250:300);lv_obj_align(box,LV_ALIGN_TOP_MID,0,40);stylePanel(box,1);lv_obj_set_style_pad_all(box,18,0);lv_obj_set_style_radius(box,Tab5UiStyle::radius(),0);lv_obj_set_scroll_dir(box,LV_DIR_VER);lv_obj_set_style_max_height(box,lv_pct(85),0);
 label(box,title,0,0,width-40);int y=70;
 if(kind==InputPath||kind==InputName){s.fields[0]=field(box,initial?initial:"",62);y=128;}
 if(kind==NewSize){char n[16];snprintf(n,sizeof(n),"%d",s.doc.current.width);label(box,"Largeur",0,52,width-40);s.fields[0]=field(box,n,82,true);snprintf(n,sizeof(n),"%d",s.doc.current.height);label(box,"Hauteur",0,140,width-40);s.fields[1]=field(box,n,170,true);y=228;}
 if(kind==ConfirmDiscard){label(box,"Des modifications ne sont pas enregistrées.",0,52,width-40);y=120;}
 if(kind==ConfirmOverwrite){label(box,s.pendingPath,0,62,width-40);y=130;}
 auto* cancel=command(box,"Annuler",0,y,(width-52)/2,None);
 s.dialogSubmit=button(box,kind==ConfirmDiscard?"Abandonner":kind==ConfirmOverwrite?"Remplacer":"Valider",(width-28)/2,y,(width-52)/2,[](lv_event_t*){dialogSubmit();});
 s.dialogStatus=label(box,"",0,y+55,width-40);Tab5Focus.focusInitial(s.modal,s.fields[0]?s.fields[0]:(kind==ConfirmDiscard||kind==ConfirmOverwrite)?cancel:s.dialogSubmit);
 if(s.fields[0]&&hooks.keyboard)hooks.keyboard(!writing::usbConnected(),s.fields[0]);
}
size_t readBytes(void* context,void* out,size_t bytes){auto* file=static_cast<File*>(context);return file->read(static_cast<uint8_t*>(out),bytes);}
using Writer=Tab5ImageWrite<fs::FS,File,true>;
struct WriteContext {Writer* stream;int handle;};
size_t writeBytes(void* context,const void* data,size_t bytes){auto& c=*static_cast<WriteContext*>(context);size_t done=0;while(done<bytes){size_t part=std::min<size_t>(4096,bytes-done);if(c.stream->chunk(c.handle,static_cast<const uint8_t*>(data)+done,part)!=int(part))break;done+=part;}return done;}
bool load(const char* path){
 if(!path||strnlen(path,sizeof(s.path))>=sizeof(s.path)){info("Chemin trop long");return false;}
 Tab5StorageUse storage(true);if(!storage){info("Stockage occupé");return false;}Writer::recover(tab5FS,path);File file=tab5FS.open(path,FILE_READ);if(!file||file.isDirectory()){info("Impossible d’ouvrir l’image");return false;}
 uint8_t header[12]{};const auto kind=Tab5ImageFormats::sniff(header,file.read(header,sizeof(header)));
 if(kind==Tab5ImageFormats::Kind::Unknown){file.close();info("Formats pris en charge : BMP, T5I, JPEG, PNG et WebP");return false;}
 if(!file.seek(0)){file.close();info("Impossible d’ouvrir l’image");return false;}
 s.working=true;Tab5ImageCodec::Options options{};int ok=0;
 switch(kind){case Tab5ImageFormats::Kind::Project:ok=ie_load_project(&s.doc,readBytes,&file);break;case Tab5ImageFormats::Kind::Jpeg:ok=Tab5ImageCodec::loadJpeg(&s.doc,readBytes,&file,options);break;case Tab5ImageFormats::Kind::Png:ok=Tab5ImageCodec::loadPng(&s.doc,readBytes,&file,options);break;case Tab5ImageFormats::Kind::Webp:ok=Tab5ImageCodec::loadWebp(&s.doc,readBytes,&file,options);break;case Tab5ImageFormats::Kind::Bmp:ok=ie_load_bmp(&s.doc,readBytes,&file);break;default:break;}file.close();s.working=false;
 if(ok){copy(s.path,sizeof(s.path),path);s.pathProject=kind==Tab5ImageFormats::Kind::Project;s.fitPending=true;s.hovering=false;}result(ok);panel();return ok;
}
void save(const char* path,bool exported){
 if(!Writer::valid(path)){info("Enregistrer dans /Picture avec l’extension .t5i, .bmp, .jpg ou .jpeg");return;}
 Tab5StorageUse storage(true);if(!storage){info("Stockage occupé");return;}
 Writer stream;int handle=stream.begin(tab5FS,path,FILE_WRITE);if(handle<0){info("Impossible d’enregistrer l’image");return;}
 s.working=true;WriteContext context{&stream,handle};const char* ext=strrchr(path,'.');const bool bmp=!strcasecmp(ext,".bmp"),jpeg=!strcasecmp(ext,".jpg")||!strcasecmp(ext,".jpeg");int ok=bmp?ie_save_bmp(&s.doc,writeBytes,&context):jpeg?Tab5ImageCodec::saveJpeg(&s.doc,writeBytes,&context,92):ie_save_project(&s.doc,writeBytes,&context);ok=stream.finish(tab5FS,handle,ok)==0&&ok;s.working=false;
 if(ok){if(!bmp&&!jpeg&&!exported){s.doc.dirty=0;copy(s.path,sizeof(s.path),path);s.pathProject=true;}info(bmp?"Image BMP exportée":jpeg?"Image JPEG exportée":"Projet enregistré");}else info("Échec d’enregistrement ; image conservée");status();
}
void choose(){if(!Tab5FilePicker::open("/Picture",nullptr,false,[](const char* path,void*){if(path)open(path);},nullptr))info("Impossible d’ouvrir l’explorateur");}
void dialogSubmit(){
 if(!s.modal||s.working)return;const Dialog kind=s.dialog;const Action pending=s.pending;
 if(kind==ConfirmDiscard){char path[261];copy(path,sizeof(path),s.pendingPath);closeDialog();if(pending==Quit)close(true);else if(pending==Open){if(path[0])load(path);else choose();}else if(pending==New)dialog(NewSize,New,"Nouvelle image");return;}
 if(kind==ConfirmOverwrite){char path[261];copy(path,sizeof(path),s.pendingPath);closeDialog();save(path,pending==Export||pending==ExportJpeg);return;}
 if(kind==InputPath){char path[261];const char* input=lv_textarea_get_text(s.fields[0]);if(strlen(input)>=sizeof(path)){lv_label_set_text(s.dialogStatus,tr("Chemin trop long"));return;}copy(path,sizeof(path),input);if(!Writer::valid(path)){lv_label_set_text(s.dialogStatus,tr("Utiliser /Picture/nom.t5i, /Picture/nom.bmp ou /Picture/nom.jpg"));return;}copy(s.pendingPath,sizeof(s.pendingPath),path);closeDialog();if(tab5FS.exists(path))dialog(ConfirmOverwrite,pending,"Remplacer le fichier existant ?");else save(path,pending==Export||pending==ExportJpeg);return;}
 if(kind==InputName){char name[24];copy(name,sizeof(name),lv_textarea_get_text(s.fields[0]));closeDialog();if(!name[0])return;if(ie_begin(&s.doc)){copy(s.doc.current.layers[s.doc.current.active].name,24,name);panel();status();}else info(ie_error(&s.doc));return;}

 if(kind==NewSize){int w=atoi(lv_textarea_get_text(s.fields[0])),h=atoi(lv_textarea_get_text(s.fields[1]));if(w<1||h<1||w>IE_MAX_DIMENSION||h>IE_MAX_DIMENSION){lv_label_set_text(s.dialogStatus,tr("Dimensions : 1 à 8192 pixels"));return;}closeDialog();s.working=true;int ok=pending==New?ie_new(&s.doc,w,h,0xffffffffu):ie_resize(&s.doc,w,h);s.working=false;if(ok&&pending==New){s.path[0]=0;s.pathProject=false;s.fitPending=true;}result(ok);panel();}
}
void request(Action a){
 if(a==None){closeDialog();return;}if(s.working)return;if(!finishText(true))return;if(a==Undo&&s.gesture){cancelGesture();return;}cancelGesture();
 if((a==New||a==Quit)&&s.doc.dirty){s.pendingPath[0]=0;dialog(ConfirmDiscard,a,"Image modifiée");return;}action(a);
}
void action(Action a){
 int ok=1;s.working=true;
 switch(a){
 case New:s.working=false;dialog(NewSize,New,"Nouvelle image");return;
 case Open:s.working=false;choose();return;
 case Save:case Export:case ExportJpeg:{s.working=false;const char* path=a==Export?"/Picture/image.bmp":a==ExportJpeg?"/Picture/image.jpg":s.pathProject&&Writer::valid(s.path)&&!strcasecmp(strrchr(s.path,'.'),".t5i")?s.path:"/Picture/image.t5i";dialog(InputPath,a,a==Export?"Exporter une image BMP":a==ExportJpeg?"Exporter une image JPEG":"Enregistrer le projet T5I",path);return;}
 case Fit:s.working=false;fitView();return;
 case Actual:case ZoomIn:case ZoomOut:s.working=false;zoomAt(a==Actual?100:a==ZoomIn?s.zoom*5/4+1:s.zoom*4/5,s.displayW/2,s.displayH/2);return;
 case ShowColor:case ShowBackground:case ShowLayer:case ShowAdjust:case ShowTransform:case ShowFormat:case ToolOptions:s.working=false;if(a==ShowColor||a==ShowBackground){s.editingBackground=a==ShowBackground;s.backRed=s.backgroundColor>>16;s.backGreen=(s.backgroundColor>>8)&255;s.backBlue=s.backgroundColor&255;}s.tabIndex=(a==ShowColor||a==ShowBackground)?2:a==ShowLayer?3:a==ShowAdjust?4:a==ShowTransform?5:a==ShowFormat?6:0;optionsVisible(true);panel();return;
 case ToggleOptions:s.working=false;optionsVisible(!s.panelVisible);return;
 case UseHand:case UsePick:s.working=false;s.tool=a==UseHand?Hand:Pick;s.tabIndex=0;panel();return;
 case SwapColors:{s.working=false;uint32_t old=(s.red<<16)|(s.green<<8)|s.blue;s.red=s.backgroundColor>>16;s.green=(s.backgroundColor>>8)&255;s.blue=s.backgroundColor&255;s.backgroundColor=old;s.backRed=old>>16;s.backGreen=(old>>8)&255;s.backBlue=old&255;panel();status();return;}
 case Quit:s.working=false;close(true);return;
 case Undo:ok=ie_undo(&s.doc);break;case Redo:ok=ie_redo(&s.doc);break;
 case SelectAll:ie_select(&s.doc,0,0,s.doc.current.width-1,s.doc.current.height-1);break;
 case InvertSelection:case SoftenSelection:s.working=false;startSelection(a==InvertSelection?IE_SELECT_INVERT:IE_SELECT_EXISTING);return;
 case CopySelection:s.working=false;if(ie_selection_copy(&s.doc))info("Sélection copiée");else info(ie_error(&s.doc));return;
 case CutSelection:ok=ie_selection_cut(&s.doc);break;
 case PasteSelection:{IERect clip{};if(!ie_clipboard_bounds(&s.doc,&clip)){s.working=false;info("Presse-papiers image vide");return;}const int cx=ie_view_sample(s.displayW/2,s.originX,s.zoom),cy=ie_view_sample(s.displayH/2,s.originY,s.zoom);ok=ie_selection_paste(&s.doc,std::clamp(cx-clip.w/2,0,std::max(0,s.doc.current.width-clip.w)),std::clamp(cy-clip.h/2,0,std::max(0,s.doc.current.height-clip.h)));if(ok){s.tool=Move;s.tabIndex=0;Tab5Focus.clear();}break;}
 case DeleteSelection:ok=ie_selection_delete(&s.doc);break;
 case Clear:ok=ie_clear(&s.doc);break;case Deselect:ie_deselect(&s.doc);break;
 case Crop:{auto r=s.doc.current.selection;if(r.enabled)ok=ie_crop(&s.doc,r.x,r.y,r.w,r.h);else {s.working=false;info("Sélectionner la zone à recadrer");status();return;}break;}
 case ApplyAdjust:ok=ie_adjust(&s.doc,s.brightness,s.contrast,s.saturation,s.redGain,s.greenGain,s.blueGain,s.allLayers);break;
 case ApplyTransform:ok=ie_transform(&s.doc,s.scaleX,s.scaleY,s.shear,s.turns);break;
 case ApplySize:s.working=false;dialog(NewSize,ApplySize,"Résolution de l’image");return;
 case LayerAdd:ok=ie_layer_add(&s.doc,tr("Calque"));break;case LayerDuplicate:ok=ie_layer_duplicate(&s.doc);break;case LayerDelete:ok=ie_layer_delete(&s.doc);break;
 case LayerUp:ok=ie_layer_move(&s.doc,1);break;case LayerDown:ok=ie_layer_move(&s.doc,-1);break;case LayerMerge:ok=ie_layer_merge_down(&s.doc);break;
 case LayerRename:s.working=false;dialog(InputName,LayerRename,"Renommer le calque",s.doc.current.layers[s.doc.current.active].name);return;

 default:break;
 }
 s.working=false;result(ok);if(s.tabIndex==3)panel();
}
int& sliderValue(Slider id){switch(id){case Size:return s.brush.size;case Opacity:return s.brush.opacity;case Hardness:return s.brush.hardness;case Diffusion:return s.brush.diffusion;case Red:return s.editingBackground?s.backRed:s.red;case Green:return s.editingBackground?s.backGreen:s.green;case Blue:return s.editingBackground?s.backBlue:s.blue;case Bright:return s.brightness;case Contrast:return s.contrast;case Saturation:return s.saturation;case GainR:return s.redGain;case GainG:return s.greenGain;case GainB:return s.blueGain;case ScaleX:return s.scaleX;case ScaleY:return s.scaleY;case Shear:return s.shear;case Thickness:return s.thickness;case TextScale:return s.textScale;case LayerOpacity:return s.layerOpacity;case Strength:return s.warpStrength;case Tolerance:return s.tolerance;case Feather:return s.feather;case EdgeThreshold:return s.edgeThreshold;case Density:return s.fillDensity;case PatternScale:return s.fillScale;case ReliefExtent:return s.reliefUnit==IE_RELIEF_PERCENT?s.reliefPercent:s.reliefPixels;case ReliefHeight:return s.reliefHeight;case ReliefAzimuth:return s.reliefAzimuth;case ReliefIntensity:return s.reliefIntensity;case SliderCount:break;}return s.brush.size;}
const char* sliderCaption(Slider id){switch(id){case Size:return "Taille";case Opacity:return "Opacité";case Hardness:return "Dureté";case Diffusion:return "Diffusion";case Red:case GainR:return "Rouge";case Green:case GainG:return "Vert";case Blue:case GainB:return "Bleu";case Bright:return "Luminosité";case Contrast:return "Contraste";case Saturation:return "Saturation";case ScaleX:return "Échelle horizontale";case ScaleY:return "Échelle verticale";case Shear:return "Inclinaison";case Thickness:return "Épaisseur du contour";case TextScale:return "Taille du texte · %";case LayerOpacity:return "Opacité du calque";case Strength:return "Force";case Tolerance:return "Tolérance";case Feather:return "Progressivité du bord";case EdgeThreshold:return "Seuil du contour";case Density:return "Densité";case PatternScale:return "Taille du motif";case ReliefExtent:return s.reliefUnit==IE_RELIEF_PERCENT?"Progression · %":"Progression · px";case ReliefHeight:return "Hauteur · px";case ReliefAzimuth:return "Azimut · °";case ReliefIntensity:return "Intensité · %";case SliderCount:break;}return "";}
void sliderLabel(lv_obj_t* object,Slider id){char text[24];snprintf(text,sizeof(text),"%d",sliderValue(id));if(object)lv_label_set_text(object,text);}
void slider(lv_obj_t* parent,Slider id,int y,int low,int high,bool precise=false){
 const int w=s.panelWidth-32;const char* shortCaption=id==Hardness?"Dureté":id==Thickness?"Contour":id==LayerOpacity?"Opacité":id==Feather?"Progressivité":sliderCaption(id);auto* caption=label(parent,shortCaption,0,y,w-52);lv_obj_set_height(caption,28);lv_label_set_long_mode(caption,LV_LABEL_LONG_DOT);
 auto* value=label(parent,"",w-48,y,48);lv_obj_set_height(value,28);lv_obj_set_style_text_align(value,LV_TEXT_ALIGN_RIGHT,0);s.sliderLabels[id]=value;sliderLabel(value,id);
 auto* o=lv_slider_create(parent);lv_obj_set_pos(o,precise?54:10,y+(precise?42:34));lv_obj_set_size(o,w-(precise?108:20),15);lv_slider_set_range(o,low,high);lv_slider_set_value(o,sliderValue(id),LV_ANIM_OFF);Tab5UiStyle::control(o);Tab5Focus.describe(o,tr(sliderCaption(id)));
 lv_obj_add_event_cb(o,[](lv_event_t* e){auto* slider=lv_event_get_target_obj(e);auto id=static_cast<Slider>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));const int value=lv_slider_get_value(slider);if(!finishText(true))return;cancelGesture();sliderValue(id)=value;if(s.editingBackground&&(id==Red||id==Green||id==Blue))s.backgroundColor=(s.backRed<<16)|(s.backGreen<<8)|s.backBlue;sliderLabel(s.sliderLabels[id],id);swatches();},LV_EVENT_VALUE_CHANGED,reinterpret_cast<void*>(uintptr_t(id)));
 s.sliders[id]=o;
 if(precise)for(int sign:{-1,1}){auto* step=button(parent,sign<0?"−":"+",sign<0?0:w-42,y+28,42,[](lv_event_t* e){auto* control=static_cast<lv_obj_t*>(lv_event_get_user_data(e));const int delta=lv_obj_get_x(lv_event_get_target_obj(e))==0?-1:1;const int value=lv_slider_get_value(control);lv_slider_set_value(control,value+delta,LV_ANIM_OFF);if(value!=lv_slider_get_value(control))lv_obj_send_event(control,LV_EVENT_VALUE_CHANGED,nullptr);},o);char desc[160];snprintf(desc,sizeof(desc),"%s %s",tr(sliderCaption(id)),sign<0?"−":"+");Tab5Focus.describe(step,desc);}
}
int checkbox(lv_obj_t* parent,const char* text,int y,int* value){
 auto* o=lv_checkbox_create(parent);lv_obj_set_pos(o,0,y);lv_obj_set_width(o,s.panelWidth-32);lv_checkbox_set_text(o,"");if(*value)lv_obj_add_state(o,LV_STATE_CHECKED);Tab5UiStyle::control(o);Tab5Focus.describe(o,tr(text));
 auto* caption=label(o,text,40,0,s.panelWidth-72);lv_obj_update_layout(caption);const int height=std::max(36,int(lv_obj_get_height(caption)));lv_obj_set_height(o,height);
 lv_obj_add_event_cb(o,[](lv_event_t* e){auto* value=static_cast<int*>(lv_event_get_user_data(e));const int checked=lv_obj_has_state(lv_event_get_target_obj(e),LV_STATE_CHECKED)?1:0;if(!finishText(true))return;cancelGesture();*value=checked;if(value==&s.edgeStop)panel();},LV_EVENT_VALUE_CHANGED,value);return height+16;
}
int choice(lv_obj_t* parent,const char* title,int y,int* value,const char* const* names,int count){
 const int w=s.panelWidth-32;auto* caption=label(parent,title,0,y,w);lv_obj_update_layout(caption);const int offset=int(lv_obj_get_height(caption))+8;auto* o=lv_dropdown_create(parent);lv_obj_set_pos(o,0,y+offset);lv_obj_set_size(o,w,42);char options[700]{};for(int i=0;i<count;++i){if(i)strncat(options,"\n",sizeof(options)-strlen(options)-1);strncat(options,tr(names[i]),sizeof(options)-strlen(options)-1);}lv_dropdown_set_options(o,options);lv_dropdown_set_selected(o,*value);Tab5UiStyle::control(o);Tab5Focus.describe(o,tr(title));lv_obj_add_event_cb(o,[](lv_event_t* e){auto* value=static_cast<int*>(lv_event_get_user_data(e));const int selected=lv_dropdown_get_selected(lv_event_get_target_obj(e));if(!finishText(true))return;cancelGesture();*value=selected;if(value==&s.selectMethod||value==&s.fillKind||value==&s.fillRegion||value==&s.reliefProfile||value==&s.reliefUnit)panel();},LV_EVENT_VALUE_CHANGED,value);return offset+58;
}
int wrapButton(lv_obj_t* o){auto* l=lv_obj_get_child(o,0);lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);lv_obj_set_height(l,LV_SIZE_CONTENT);lv_obj_update_layout(l);const int height=std::max(64,int(lv_obj_get_height(l))+12);lv_obj_set_height(o,height);lv_obj_center(l);return height+8;}
void panel(){
 if(!s.panel||!s.initialized)return;if(Tab5Focus.contains(s.panel,Tab5Focus.focused()))Tab5Focus.clear();lv_obj_clean(s.panel);s.body=s.tab=s.layerList=nullptr;for(auto& l:s.sliderLabels)l=nullptr;for(auto& o:s.sliders)o=nullptr;for(auto& o:s.paletteButtons)o=nullptr;
 if(s.tabIndex==1||s.tabIndex==7)s.tabIndex=0;
 const char* title=s.tabIndex==2?(s.editingBackground?"Couleur d’arrière-plan":"Couleur de premier plan"):s.tabIndex==3?"Calques":s.tabIndex==4?"Couleurs et luminosité":s.tabIndex==5?"Transformation":s.tabIndex==6?"Format et résolution":toolNames[s.tool];
 auto* hide=command(s.panel,"",s.panelWidth-42,4,36,ToggleOptions);auto* closeLabel=lv_obj_get_child(hide,0);lv_obj_set_style_text_font(closeLabel,&lv_font_montserrat_24,0);lv_label_set_text(closeLabel,LV_SYMBOL_CLOSE);Tab5Focus.describe(hide,tr("Masquer les options"));
 auto* heading=label(s.panel,title,12,8,s.panelWidth-64);lv_label_set_long_mode(heading,LV_LABEL_LONG_WRAP);lv_obj_update_layout(heading);int head=std::max(48,int(lv_obj_get_height(heading))+18);
 if(s.tabIndex){wrapButton(command(s.panel,"Options de l’outil",12,head,s.panelWidth-24,ToolOptions));head+=72;}
 auto* body=lv_obj_create(s.panel);s.body=body;lv_obj_set_pos(body,12,head);lv_obj_set_size(body,s.panelWidth-24,std::max(40,int(lv_obj_get_height(s.panel))-head-8));stylePanel(body,1);lv_obj_set_scroll_dir(body,LV_DIR_VER);lv_obj_set_style_pad_bottom(body,16,0);
 const int w=s.panelWidth-32,half=(w-8)/2;
 if(s.tabIndex==0){
  if(s.tool==Brush||s.tool==Eraser){const char* shapes[]={"Rond","Carré","Barre horizontale","Barre diagonale /","Barre diagonale \\","Barre verticale"};choice(body,"Forme du pinceau",0,&s.brush.square,shapes,6);slider(body,Size,90,1,256);slider(body,Opacity,156,0,100);slider(body,Hardness,222,0,100);slider(body,Diffusion,288,0,100);label(body,"Dureté : bord du trait. Diffusion : dispersion des points.",0,362,w);}
  else if(s.tool==Select){
   const char* methods[]={"Rectangle","Ellipse","Lasso libre","Baguette contiguë","Plage de couleurs"};const char* combinations[]={"Remplacer","Ajouter","Soustraire","Intersection"};int y=0;
   y+=choice(body,"Méthode de sélection",y,&s.selectMethod,methods,5);y+=choice(body,"Combiner la sélection",y,&s.selectCombine,combinations,4);
   slider(body,Feather,y,-32,32);y+=66;
   {auto* hint=label(body,"− : intérieur · + : autour du bord",0,y,w);lv_obj_update_layout(hint);y+=int(lv_obj_get_height(hint))+16;}
   if(s.selectMethod>=IE_SELECT_CONTIGUOUS){slider(body,Tolerance,y,0,255);y+=76;y+=checkbox(body,"Échantillonner les calques visibles",y,&s.sampleMerged);if(s.selectMethod==IE_SELECT_CONTIGUOUS){y+=checkbox(body,"Arrêter aux contours",y,&s.edgeStop);if(s.edgeStop){slider(body,EdgeThreshold,y,1,255);y+=76;}}}
   y+=wrapButton(command(body,"Tout sélectionner",0,y,w,SelectAll));y+=wrapButton(command(body,"Inverser la sélection",0,y,w,InvertSelection));y+=wrapButton(command(body,"Adoucir la sélection actuelle",0,y,w,SoftenSelection));wrapButton(command(body,"Tout désélectionner",0,y,w,Deselect));
  }
  else if(s.tool==Fill){
   const char* regions[]={"Zone contiguë","Couleurs similaires","Toute la sélection"};const char* kinds[]={"Couleur unie","Dégradé linéaire","Dégradé radial","Damier","Hachures","Points","Relief"};const char* directions[]={"Horizontal","Vertical","Diagonale descendante","Diagonale montante"};int y=0;
   y+=choice(body,"Zone à remplir",y,&s.fillRegion,regions,3);y+=choice(body,"Remplissage",y,&s.fillKind,kinds,7);
   if(s.fillKind==IE_FILL_LINEAR)y+=choice(body,"Direction",y,&s.fillDirection,directions,4);
   if(s.fillKind==IE_FILL_RELIEF){
    const char* profiles[]={"Conique","Sphérique","Extrusion"};const char* units[]={"Pixels","Proportion"};
    y+=choice(body,"Profil du relief",y,&s.reliefProfile,profiles,3);
    if(s.reliefProfile!=IE_RELIEF_EXTRUDE){y+=choice(body,"Unité de progression",y,&s.reliefUnit,units,2);slider(body,ReliefExtent,y,1,s.reliefUnit==IE_RELIEF_PERCENT?100:8192,true);y+=86;}
    slider(body,ReliefHeight,y,-256,256,true);y+=86;
    auto* hint=label(body,"− : creux · + : saillie",0,y,w);lv_obj_update_layout(hint);y+=int(lv_obj_get_height(hint))+16;
    slider(body,ReliefAzimuth,y,0,359,true);y+=86;slider(body,ReliefIntensity,y,0,200,true);y+=86;
   }else if(s.fillKind>=IE_FILL_CHECKER){slider(body,PatternScale,y,2,64);y+=76;}
   slider(body,Opacity,y,0,100);y+=76;slider(body,Density,y,0,100);y+=76;slider(body,Feather,y,-32,32);y+=66;
   {auto* hint=label(body,"− : intérieur · + : autour du bord",0,y,w);lv_obj_update_layout(hint);y+=int(lv_obj_get_height(hint))+16;}
   if(s.fillRegion!=IE_FILL_SELECTION){slider(body,Tolerance,y,0,255);y+=76;y+=checkbox(body,"Échantillonner les calques visibles",y,&s.sampleMerged);if(s.fillRegion==IE_FILL_CONTIGUOUS){y+=checkbox(body,"Arrêter aux contours",y,&s.edgeStop);if(s.edgeStop){slider(body,EdgeThreshold,y,1,255);y+=76;}}}
   y+=wrapButton(command(body,"Couleur de premier plan",0,y,w,ShowColor));wrapButton(command(body,"Couleur d’arrière-plan",0,y,w,ShowBackground));
  }
  else if(s.tool==Warp){const char* modes[]={"Pousser","Gonfler","Pincer"};choice(body,"Mode",0,&s.warpMode,modes,3);slider(body,Size,90,1,256);slider(body,Strength,156,0,100);label(body,"La déformation locale agit sur le calque actif et respecte la sélection.",0,232,w);}
  else if(s.tool==Line||s.tool==Rectangle||s.tool==Ellipse){slider(body,Opacity,0,0,100);slider(body,Thickness,66,1,64);if(s.tool!=Line){checkbox(body,"Forme pleine",140,&s.fill);checkbox(body,"Contour",186,&s.outline);}if(s.tool==Ellipse){checkbox(body,"Cercle",232,&s.ellipseCircle);checkbox(body,"Depuis le centre",278,&s.ellipseCenter);label(body,"Tracer depuis deux coins opposés, ou depuis le centre.",0,334,w);}}
  else if(s.tool==Hand){auto* plus=button(body,"+",0,0,half,[](lv_event_t*){request(ZoomIn);});auto* minus=button(body,"−",half+8,0,half,[](lv_event_t*){request(ZoomOut);});Tab5Focus.describe(plus,tr("Zoom avant"));Tab5Focus.describe(minus,tr("Zoom arrière"));wrapButton(command(body,"Ajuster à la fenêtre",0,52,w,Fit));wrapButton(command(body,"Taille réelle · 100 %",0,124,w,Actual));label(body,"Déplacer la vue sans modifier l’image.",0,202,w);}
  else if(s.tool==Text){slider(body,TextScale,0,25,400);slider(body,Opacity,66,0,100);label(body,"Police de l’OS · Entrée valide · Maj+Entrée : nouvelle ligne · Échap annule",0,144,w);}
  else if(s.tool==Pick){label(body,"Toucher une couleur dans l’image",0,0,w);command(body,"Couleur",0,84,w,ShowColor);}
  else {int y=0;for(const auto& item: {std::pair<const char*,Action>{"Copier la sélection",CopySelection},{"Couper la sélection",CutSelection},{"Coller la sélection",PasteSelection},{"Supprimer la sélection",DeleteSelection},{"Tout désélectionner",Deselect}})y+=wrapButton(command(body,item.first,0,y,w,item.second));label(body,"Glisser la sélection · Flèches : 1 px · Maj+flèches : 10 px",0,y,w);}
 }else if(s.tabIndex==2){
  for(size_t i=0;i<PaletteCount;++i){auto* o=lv_button_create(body);s.paletteButtons[i]=o;lv_obj_set_pos(o,i%6*(w/6),i/6*42);lv_obj_set_size(o,w/6-6,34);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,color(3),0);lv_obj_set_style_shadow_width(o,0,0);Tab5UiStyle::control(o);lv_obj_set_style_bg_color(o,lv_color_hex(PaletteColors[i]),0);lv_obj_set_style_bg_color(o,lv_color_hex(PaletteColors[i]),LV_STATE_PRESSED);char desc[32];snprintf(desc,sizeof(desc),"#%06X",unsigned(PaletteColors[i]));Tab5Focus.describe(o,desc);lv_obj_add_event_cb(o,[](lv_event_t* e){uint32_t c=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));choosePalette(c,Tab5Mouse::isInput(lv_event_get_indev(e))?false:s.editingBackground);},LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(PaletteColors[i])));}
  slider(body,Red,96,0,255);slider(body,Green,162,0,255);slider(body,Blue,228,0,255);command(body,"Pipette dans l’image",0,302,w,UsePick);wrapButton(command(body,"Permuter les couleurs",0,356,w,SwapColors));
 }else if(s.tabIndex==3){
  s.layerList=lv_dropdown_create(body);lv_obj_set_pos(s.layerList,0,0);lv_obj_set_size(s.layerList,w,44);char layers[400]{};for(int i=0;i<s.doc.current.count;++i){const char* name=s.doc.current.layers[i].name;if(!strcmp(name,"Image BMP")||!strcmp(name,"Image JPEG")||!strcmp(name,"Image PNG")||!strcmp(name,"Image WebP"))name=tr(name);char line[100];snprintf(line,sizeof(line),"%s%d · %s",i?"\n":"",i+1,name);strncat(layers,line,sizeof(layers)-strlen(layers)-1);}lv_dropdown_set_options(s.layerList,layers);lv_dropdown_set_selected(s.layerList,s.doc.current.active);Tab5UiStyle::control(s.layerList);lv_obj_add_event_cb(s.layerList,[](lv_event_t* e){const int selected=lv_dropdown_get_selected(lv_event_get_target_obj(e));if(!finishText(true))return;cancelGesture();s.doc.current.active=selected;panel();status();},LV_EVENT_VALUE_CHANGED,nullptr);
  command(body,"Ajouter",0,54,half,LayerAdd);command(body,"Dupliquer",half+8,54,half,LayerDuplicate);command(body,"Monter",0,104,half,LayerUp);command(body,"Descendre",half+8,104,half,LayerDown);command(body,"Fusionner",0,154,half,LayerMerge);command(body,"Supprimer",half+8,154,half,LayerDelete);command(body,"Renommer",0,204,w,LayerRename);s.layerOpacity=s.doc.current.layers[s.doc.current.active].opacity;slider(body,LayerOpacity,263,0,255);
  button(body,"Appliquer l’opacité",0,329,w,[](lv_event_t*){if(!finishText(true))return;cancelGesture();result(ie_layer_style(&s.doc,s.layerOpacity,s.doc.current.layers[s.doc.current.active].visible));});button(body,s.doc.current.layers[s.doc.current.active].visible?"Masquer ce calque":"Afficher ce calque",0,379,w,[](lv_event_t*){if(!finishText(true))return;cancelGesture();auto l=s.doc.current.layers[s.doc.current.active];result(ie_layer_style(&s.doc,l.opacity,!l.visible));panel();});
 }else if(s.tabIndex==4){auto* scope=label(body,s.doc.current.selection.enabled?"Zone : sélection actuelle":"Zone : calque entier",0,0,w);lv_obj_update_layout(scope);int y=lv_obj_get_height(scope)+18;slider(body,Bright,y,-100,100);slider(body,Contrast,y+66,-100,100);slider(body,Saturation,y+132,-100,100);slider(body,GainR,y+198,0,200);slider(body,GainG,y+264,0,200);slider(body,GainB,y+330,0,200);const int h=checkbox(body,"Tous les calques visibles",y+402,&s.allLayers);command(body,"Appliquer",0,y+402+h,w,ApplyAdjust);
 }else if(s.tabIndex==5){slider(body,ScaleX,0,25,400);slider(body,ScaleY,66,25,400);slider(body,Shear,132,-200,200);const char* angles[]={"0°","90°","180°","270°"};choice(body,"Rotation",204,&s.turns,angles,4);command(body,"Appliquer",0,296,w,ApplyTransform);label(body,"La transformation agit sur la sélection du calque actif, ou sur l’image entière sans sélection.",0,354,w);
 }else if(s.tabIndex==6){wrapButton(command(body,"Modifier la résolution",0,0,w,ApplySize));wrapButton(command(body,"Recadrer la sélection",0,74,w,Crop));command(body,"Exporter en BMP",0,148,w,Export);command(body,"Exporter en JPEG",0,202,w,ExportJpeg);command(body,"Enregistrer le projet",0,256,w,Save);label(body,"T5I conserve les calques. BMP et JPEG exportent l’image aplatie sur fond blanc.",0,316,w);}
 toolState();appearance();
}
void toolIcon(lv_event_t* e){
 auto* o=lv_event_get_target_obj(e);lv_area_t a;lv_obj_get_coords(o,&a);const int cx=(a.x1+a.x2)/2,cy=(a.y1+a.y2)/2;auto* layer=lv_event_get_layer(e);const int tool=int(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
 auto line=[&](int x0,int y0,int x1,int y1,int width=2){lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.color=color(4);d.width=width;d.round_start=d.round_end=1;d.p1={lv_value_precise_t(cx+x0),lv_value_precise_t(cy+y0)};d.p2={lv_value_precise_t(cx+x1),lv_value_precise_t(cy+y1)};lv_draw_line(layer,&d);};
 auto rect=[&](int x0,int y0,int x1,int y1){line(x0,y0,x1,y0);line(x1,y0,x1,y1);line(x1,y1,x0,y1);line(x0,y1,x0,y0);};
 if(tool==Brush){line(-4,4,11,-11,5);line(-7,5,-12,12,7);line(-12,12,-3,10,3);}
 else if(tool==Eraser){line(-12,3,0,-10,5);line(0,-10,11,0,5);line(11,0,-1,13,5);line(-1,13,-12,3,5);line(-7,7,5,7,2);line(-3,14,13,14);}
 else if(tool==Select){for(int q=-12;q<12;q+=6){line(q,-11,q+3,-11);line(q,11,q+3,11);line(-12,q,-12,q+3);line(12,q,12,q+3);}}
 else if(tool==Move){line(-13,0,13,0);line(0,-13,0,13);line(-13,0,-8,-5);line(-13,0,-8,5);line(13,0,8,-5);line(13,0,8,5);line(0,-13,-5,-8);line(0,-13,5,-8);line(0,13,-5,8);line(0,13,5,8);}
 else if(tool==Rectangle)rect(-12,-9,12,9);
 else if(tool==Ellipse){int x=12,y=0;for(int i=1;i<=24;++i){double t=i*6.283185307179586/24;int nx=int(lround(12*cos(t))),ny=int(lround(9*sin(t)));line(x,y,nx,ny);x=nx;y=ny;}}
 else if(tool==Line)line(-11,11,11,-11,3);
 else if(tool==Text){line(-11,-11,11,-11,3);line(0,-11,0,12,3);line(-6,12,6,12,3);}
 else if(tool==Pick){line(-9,9,7,-7,5);line(3,-11,11,-3,5);line(-12,12,-8,8);}
 else if(tool==Fill){line(-10,-2,0,-12);line(0,-12,12,0);line(12,0,2,10);line(2,10,-10,-2);line(-5,-6,-8,-11);line(-8,-11,-3,-14);line(9,6,13,12,4);line(-13,14,5,14);}
 else if(tool==Hand){line(-8,10,-13,1);line(-13,1,-10,-2);line(-10,-2,-5,3);line(-5,3,-5,-10);line(-5,-10,-2,-12);line(-2,-12,0,-10);line(0,-10,0,0);line(0,-8,3,-10);line(3,-10,5,-8);line(5,-8,5,1);line(5,-6,8,-8);line(8,-8,10,-5);line(10,-5,10,5);line(10,5,6,12);line(6,12,-8,10);}
 else {for(int row=-8;row<=8;row+=8){line(-13,row,-5,row+3);line(-5,row+3,4,row-3);line(4,row-3,13,row);}}
}
struct MenuItem{const char* label;Action action;};
struct Menu{const char* title;const MenuItem* items;int count;};
const MenuItem fileMenu[]={{"Nouveau",New},{"Importer une image",Open},{"Enregistrer le projet",Save},{"Exporter en BMP",Export},{"Exporter en JPEG",ExportJpeg},{"Fermer",Quit}};
const MenuItem editMenu[]={{"Annuler · Ctrl+Z",Undo},{"Rétablir · Ctrl+Y",Redo},{"Tout sélectionner",SelectAll},{"Inverser la sélection",InvertSelection},{"Tout désélectionner",Deselect},{"Copier la sélection · Ctrl+C",CopySelection},{"Couper la sélection · Ctrl+X",CutSelection},{"Coller la sélection · Ctrl+V",PasteSelection},{"Supprimer la sélection",DeleteSelection}};
const MenuItem viewMenu[]={{"Ajuster à la fenêtre",Fit},{"Taille réelle · 100 %",Actual},{"Zoom avant",ZoomIn},{"Zoom arrière",ZoomOut},{"Main",UseHand}};
const MenuItem imageMenu[]={{"Couleur",ShowColor},{"Couleurs et luminosité",ShowAdjust},{"Transformation",ShowTransform},{"Format et résolution",ShowFormat},{"Recadrer la sélection",Crop}};
const MenuItem layerMenu[]={{"Afficher les calques",ShowLayer},{"Ajouter",LayerAdd},{"Dupliquer",LayerDuplicate},{"Fusionner",LayerMerge},{"Supprimer",LayerDelete}};
template<size_t N> constexpr Menu menu(const char* title,const MenuItem (&items)[N]){return {title,items,int(N)};}
const Menu menus[]={menu("Fichier",fileMenu),menu("Édition",editMenu),menu("Affichage",viewMenu),menu("Image",imageMenu),menu("Calques",layerMenu)};
bool commandMenuKey(const writing::KeyEvent& event){
 using K=writing::Key;
 if(!s.toolbar)return false;
 for(size_t i=0;i<sizeof(menus)/sizeof(menus[0]);++i){
  auto* o=lv_obj_get_child(s.toolbar,i);if(!o||!lv_dropdown_is_open(o))continue;
  if(event.key==K::Enter){
   const int index=lv_dropdown_get_selected(o);lv_dropdown_close(o);Tab5Focus.finishEditing();
   if(index>=0&&index<menus[i].count)request(menus[i].items[index].action);
   return true;
  }
  const bool next=event.key==K::Down||event.key==K::Right,previous=event.key==K::Up||event.key==K::Left;
  if(next||previous||event.key==K::Home||event.key==K::End){
   const int current=lv_dropdown_get_selected(o),last=menus[i].count-1;
   const int selected=event.key==K::Home?0:event.key==K::End?last:std::clamp(current+(next?1:-1),0,last);
   if(!lv_dropdown_get_selected_highlight(o))lv_dropdown_set_selected_highlight(o,true);
   lv_dropdown_set_selected(o,selected);return true;
  }
  if(event.key==K::Tab){lv_dropdown_close(o);Tab5Focus.finishEditing();}
  return false;
 }
 return false;
}
int textWidth(const char* value){lv_point_t size;lv_text_get_size(&size,tr(value),s.font?s.font:LV_FONT_DEFAULT,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);return size.x;}
void toolbar(){
 if(Tab5Focus.contains(s.toolbar,Tab5Focus.focused()))Tab5Focus.clear();lv_obj_clean(s.toolbar);s.undo=s.redo=nullptr;s.foreground=s.background=s.zoomLabel=nullptr;s.foregroundStamp=s.backgroundStamp=UINT32_MAX;
 for(const auto& menu:menus){auto* o=lv_dropdown_create(s.toolbar);lv_obj_set_size(o,textWidth(menu.title)+52,42);char options[1200]{};for(int i=0;i<menu.count;++i){if(i)strncat(options,"\n",sizeof(options)-strlen(options)-1);strncat(options,tr(menu.items[i].label),sizeof(options)-strlen(options)-1);}lv_dropdown_set_options(o,options);lv_dropdown_set_text(o,tr(menu.title));lv_dropdown_set_selected_highlight(o,false);Tab5UiStyle::control(o);Tab5Focus.describe(o,tr(menu.title));lv_obj_add_event_cb(o,[](lv_event_t* e){auto* o=lv_event_get_target_obj(e);const auto& menu=*static_cast<const Menu*>(lv_event_get_user_data(e));const int index=lv_dropdown_get_selected(o);if(index>=0&&index<menu.count)request(menu.items[index].action);},LV_EVENT_VALUE_CHANGED,const_cast<Menu*>(&menu));lv_obj_add_event_cb(o,[](lv_event_t* e){auto* o=lv_event_get_target_obj(e);lv_dropdown_set_selected(o,0);lv_dropdown_set_selected_highlight(o,Tab5Focus.focused()==o&&Tab5Focus.controlling());},LV_EVENT_READY,nullptr);}
 auto* z=command(s.toolbar,"100%",0,0,textWidth("1600%")+24,Fit);s.zoomLabel=lv_obj_get_child(z,0);lv_label_set_long_mode(s.zoomLabel,LV_LABEL_LONG_CLIP);Tab5Focus.describe(z,tr("Ajuster à la fenêtre"));
 s.foreground=command(s.toolbar,"",0,0,34,ShowColor);Tab5Focus.describe(s.foreground,tr("Couleur de premier plan"));
 s.background=command(s.toolbar,"",0,0,34,ShowBackground);Tab5Focus.describe(s.background,tr("Couleur d’arrière-plan"));
 s.optionsButton=command(s.toolbar,"",0,0,42,ToggleOptions);auto* ol=lv_obj_get_child(s.optionsButton,0);lv_obj_set_style_text_font(ol,&lv_font_montserrat_24,0);lv_label_set_text(ol,LV_SYMBOL_SETTINGS);Tab5Focus.describe(s.optionsButton,tr(s.panelVisible?"Masquer les options":"Afficher les options"));if(s.panelVisible)lv_obj_add_state(s.optionsButton,LV_STATE_CHECKED);
 auto* quit=command(s.toolbar,"",0,0,40,Quit);auto* ql=lv_obj_get_child(quit,0);lv_obj_set_style_text_font(ql,&lv_font_montserrat_24,0);lv_label_set_text(ql,LV_SYMBOL_CLOSE);Tab5Focus.describe(quit,tr("Fermer"));swatches();
}
void optionsVisible(bool value){
 if(!s.panel)return;s.panelVisible=value;if(value){lv_obj_remove_flag(s.panel,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(s.panel);}else {while(closeDropdown(s.panel)){}if(Tab5Focus.contains(s.panel,Tab5Focus.focused()))Tab5Focus.clear();lv_obj_add_flag(s.panel,LV_OBJ_FLAG_HIDDEN);}
 if(s.optionsButton){if(value)lv_obj_add_state(s.optionsButton,LV_STATE_CHECKED);else lv_obj_remove_state(s.optionsButton,LV_STATE_CHECKED);Tab5Focus.describe(s.optionsButton,tr(value?"Masquer les options":"Afficher les options"));}refreshHover();
}
void build(){
 s.root=lv_obj_create(host);lv_obj_add_event_cb(s.root,[](lv_event_t* e){if(lv_event_get_target_obj(e)!=s.root||s.closing)return;s.textInput=nullptr;s.textLayout=nullptr;s.textEditing=false;s.root=nullptr;if(s.job){ie_job_destroy(s.job);s.job=nullptr;}if(s.jobOverlay){lv_obj_delete(s.jobOverlay);s.jobOverlay=nullptr;}if(s.lasso){release(nullptr,s.lasso);s.lasso=nullptr;}closeDialog();lv_image_cache_drop(&s.image);ie_destroy(&s.doc);release(nullptr,s.pixels);const int width=s.width,height=s.height;s=State{};s.width=width;s.height=height;},LV_EVENT_DELETE,nullptr);
 lv_obj_remove_flag(s.root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_pos(s.root,0,0);stylePanel(s.root,0);s.font=hooks.font?hooks.font():lv_obj_get_style_text_font(host,LV_PART_MAIN);lv_obj_set_style_text_font(s.root,s.font,0);
 s.toolbar=lv_obj_create(s.root);stylePanel(s.toolbar,0);lv_obj_remove_flag(s.toolbar,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_flex_flow(s.toolbar,LV_FLEX_FLOW_ROW_WRAP);lv_obj_set_style_pad_all(s.toolbar,4,0);lv_obj_set_style_pad_column(s.toolbar,6,0);lv_obj_set_style_pad_row(s.toolbar,4,0);lv_obj_set_height(s.toolbar,LV_SIZE_CONTENT);toolbar();
 s.rail=lv_obj_create(s.root);stylePanel(s.rail,1);lv_obj_set_scroll_dir(s.rail,LV_DIR_VER);for(int i=0;i<ToolCount;++i){auto* o=lv_button_create(s.rail);s.toolButtons[i]=o;lv_obj_set_pos(o,3,i*44);lv_obj_set_size(o,46,40);Tab5UiStyle::button(o);Tab5Focus.describe(o,tr(toolNames[i]));lv_obj_add_event_cb(o,toolIcon,LV_EVENT_DRAW_MAIN,reinterpret_cast<void*>(uintptr_t(i)));lv_obj_add_event_cb(o,[](lv_event_t* e){if(!finishText(true))return;cancelGesture();s.tool=static_cast<Tool>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));s.tabIndex=0;panel();status();},LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(i)));}
 s.workspace=lv_obj_create(s.root);stylePanel(s.workspace,2);lv_obj_remove_flag(s.workspace,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(s.workspace,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_flag(s.workspace,LV_OBJ_FLAG_PRESS_LOCK);lv_obj_add_event_cb(s.workspace,pointer,LV_EVENT_PRESSED,nullptr);lv_obj_add_event_cb(s.workspace,pointer,LV_EVENT_PRESSING,nullptr);lv_obj_add_event_cb(s.workspace,pointer,LV_EVENT_RELEASED,nullptr);lv_obj_add_event_cb(s.workspace,pointer,LV_EVENT_PRESS_LOST,nullptr);
 s.canvas=lv_image_create(s.workspace);lv_obj_remove_flag(s.canvas,LV_OBJ_FLAG_CLICKABLE);lv_image_set_antialias(s.canvas,false);
 s.selection=lv_obj_create(s.workspace);lv_obj_remove_style_all(s.selection);lv_obj_remove_flag(s.selection,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(s.selection,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(s.selection,previewDraw,LV_EVENT_DRAW_MAIN,nullptr);
 s.panel=lv_obj_create(s.root);stylePanel(s.panel,1);lv_obj_remove_flag(s.panel,LV_OBJ_FLAG_SCROLLABLE);s.status=label(s.root,"",8,0,s.width-16);lv_label_set_long_mode(s.status,LV_LABEL_LONG_DOT);optionsVisible(false);layout();panel();appearance();
}
}
void begin(lv_obj_t* parent,Hooks value){host=parent;hooks=value;}
bool open(const char* path){
 if(!host||s.working)return false;
 if(!s.initialized){
  if(!ie_init(&s.doc,allocate,release,nullptr,640,360))return false;
  ie_set_yield(&s.doc,cooperate,nullptr);s.doc.dirty=0;s.initialized=true;build();

 }
 show(true);
 if(!finishText(true))return false;
 if(path&&*path){if(strlen(path)>=sizeof(s.pendingPath)){info("Chemin trop long");status();return false;}if(!strcmp(path,s.path))return true;if(s.doc.dirty){copy(s.pendingPath,sizeof(s.pendingPath),path);dialog(ConfirmDiscard,Open,"Image modifiée");return false;}return load(path);}
 return true;
}
void show(bool value){
 if(!s.root)return;
 if(s.shown==value){if(value)appearance();return;}
 if(!value){
  finishText(true);cancelGesture();s.mouseButtons=0;s.hovering=false;s.pointerKnown=false;closeDialog();
  if(Tab5Focus.contains(s.root,Tab5Focus.focused()))Tab5Focus.remember(s.root);
  lv_obj_add_flag(s.root,LV_OBJ_FLAG_HIDDEN);if(s.jobOverlay)lv_obj_add_flag(s.jobOverlay,LV_OBJ_FLAG_HIDDEN);s.shown=false;
  // Only the derived preview is discarded at this explicit navigation step.
  // Layers, selection and undo/redo remain owned by the native app.
  lv_image_set_src(s.canvas,nullptr);lv_image_cache_drop(&s.image);release(nullptr,s.pixels);s.pixels=nullptr;
  s.canvasBytes=0;s.displayW=s.displayH=0;
 }else{
  s.shown=true;layout();renderAll();appearance();lv_obj_remove_flag(s.root,LV_OBJ_FLAG_HIDDEN);if(s.jobOverlay)lv_obj_remove_flag(s.jobOverlay,LV_OBJ_FLAG_HIDDEN);
 }
}
bool close(bool discard){
 if(s.working||s.closing)return false;if(!s.initialized)return true;if(!finishText(!discard))return false;
 if(!discard&&s.doc.dirty){show(true);dialog(ConfirmDiscard,Quit,"Image modifiée");return false;}
 s.closing=true;closeDialog();cancelGesture();if(s.root&&Tab5Focus.contains(s.root,Tab5Focus.focused()))Tab5Focus.clear();
 if(s.root)lv_obj_delete(s.root);s.root=nullptr;lv_image_cache_drop(&s.image);ie_destroy(&s.doc);release(nullptr,s.pixels);const int width=s.width,height=s.height;s=State{};s.width=width;s.height=height;
 if(hooks.closed)hooks.closed();return true;
}
void service(){if(s.job){const int result=ie_job_step(s.job,4096);const unsigned percent=ie_job_progress(s.job);if(s.jobBar&&percent!=s.jobPercent){s.jobPercent=percent;lv_bar_set_value(s.jobBar,percent,LV_ANIM_OFF);char text[24];snprintf(text,sizeof(text),"%u %%",percent);lv_label_set_text(s.jobLabel,text);}if(result)finishJob(false);return;}if(!visible())return;if(s.textEditing&&s.textInput){auto* l=lv_textarea_get_label(s.textInput);if(s.textCaret!=lv_textarea_get_cursor_pos(s.textInput)||s.textSelectionStart!=lv_label_get_text_selection_start(l)||s.textSelectionEnd!=lv_label_get_text_selection_end(l))textCursor();}const auto* font=hooks.font?hooks.font():lv_obj_get_style_text_font(host,LV_PART_MAIN);if(s.palette!=stamp()||font!=s.font){const bool fontChanged=font!=s.font;if(!finishText(true))return;cancelGesture();appearance();if(fontChanged){toolbar();layout();panel();}renderAll();}}
void resize(int width,int height){const bool changed=(width>0&&width!=s.width)||(height>0&&height!=s.height);if(width>0)s.width=width;if(height>0)s.height=height;if(s.root&&changed){cancelGesture();layout();panel();renderAll();}}
bool key(const writing::KeyEvent& event){
 if(!visible())return false;using K=writing::Key;
 if(s.job){if(event.key==K::Escape)finishJob(true);else if(s.jobOverlay)Tab5Focus.key(event,s.jobOverlay);return true;}
 if(s.modal){
  if(event.key==K::Escape){closeDialog();return true;}
  auto* focused=Tab5Focus.focused();if(!Tab5Focus.contains(s.modal,focused))Tab5Focus.focusInitial(s.modal,s.fields[0]?s.fields[0]:s.dialogSubmit);
  if(event.key==K::Enter&&!event.ctrl&&!event.alt&&!event.meta){
   focused=Tab5Focus.focused();
   if(s.dialog==NewSize&&focused==s.fields[0]){Tab5Focus.editObject(s.fields[1]);if(hooks.keyboard)hooks.keyboard(!writing::usbConnected(),s.fields[1]);return true;}
   if(focused==s.fields[0]||focused==s.fields[1]||focused==s.dialogSubmit){dialogSubmit();return true;}
  }
  Tab5Focus.key(event,s.modal);return true;
 }
 if(s.textEditing&&s.textInput){
  if(event.key==K::Escape){finishText(false);return true;}
  if(event.key==K::Enter){if(!event.shift){finishText(true);return true;}lv_textarea_set_one_line(s.textInput,false);Tab5TextInput::insert(s.textInput,"\n");lv_textarea_set_one_line(s.textInput,true);lv_obj_set_size(s.textInput,2,2);textCursor();return true;}
  if(event.key==K::Tab){return !finishText(true);}
  const bool used=Tab5TextInput::key(s.textInput,event);textCursor();return used;
 }
 if(commandMenuKey(event))return true;
 if(event.key==K::Escape){
  if(s.dragging||s.panning){cancelGesture();return true;}
  if(closeDropdown(s.root))return true;
  if(s.panelVisible){optionsVisible(false);return true;}
  if(Tab5Focus.contains(s.root,Tab5Focus.focused())&&Tab5Focus.key(event,s.root))return true;
  request(Quit);return true;
 }
 if(event.key==K::Text&&(event.ctrl||event.meta)){char c=event.text[0]|32;if(c=='+'||c=='=')request(ZoomIn);else if(c=='-')request(ZoomOut);else if(c=='0')request(Fit);else if(c=='1')request(Actual);else if(c=='z')request(event.shift?Redo:Undo);else if(c=='y')request(Redo);else if(c=='s')request(Save);else if(c=='o')request(Open);else if(c=='n')request(New);else if(c=='a')request(SelectAll);else if(c=='i')request(InvertSelection);else if(c=='c')request(CopySelection);else if(c=='x')request(CutSelection);else if(c=='v')request(PasteSelection);else return false;return true;}
 if(event.key==K::Delete){request(DeleteSelection);return true;}
 if(s.tool==Move&&s.doc.current.selection.enabled&&!event.ctrl&&!event.alt&&!event.meta&&(event.key==K::Left||event.key==K::Right||event.key==K::Up||event.key==K::Down)&&(!Tab5Focus.contains(s.root,Tab5Focus.focused())||Tab5Focus.focused()==s.canvas||Tab5Focus.focused()==s.workspace)){const int step=event.shift?10:1;result(ie_translate_selection(&s.doc,event.key==K::Left?-step:event.key==K::Right?step:0,event.key==K::Up?-step:event.key==K::Down?step:0));return true;}
 if(event.key==K::Text&&!event.ctrl&&!event.meta&&!event.alt){char c=event.text[0]|32;Tool tool=s.tool;if(c=='x'){request(SwapColors);return true;}if(c=='b')tool=Brush;else if(c=='e')tool=Eraser;else if(c=='s')tool=Select;else if(c=='v')tool=Move;else if(c=='t')tool=Text;else if(c=='i')tool=Pick;else if(c=='h'||c==' ')tool=Hand;else if(c=='l')tool=Line;else if(c=='r')tool=Rectangle;else if(c=='o')tool=Ellipse;else if(c=='w')tool=Warp;else if(c=='g')tool=Fill;else return false;if(!finishText(true))return true;cancelGesture();s.tool=tool;s.tabIndex=0;panel();return true;}
 return false;
}
void cancelPointer(){s.pointerKnown=false;cancelGesture();s.mouseButtons=0;if(s.hovering){s.hovering=false;status();}}
bool mouse(int x,int y,uint8_t buttons,int wheel){
 if(!visible()||s.working||s.modal){cancelPointer();return false;}
 s.pointerKnown=true;s.pointerX=x;s.pointerY=y;lv_area_t a;lv_obj_get_coords(s.workspace,&a);const int px=x-a.x1,py=y-a.y1;const bool inside=canvasHit(px,py);const uint8_t previous=s.mouseButtons;s.mouseButtons=buttons;hover(px,py,inside);
 // The colour palette owns secondary presses too; touch and keyboard keep
 // the selected colour panel, whereas mouse primary always chooses colour 1.
 if((buttons&2)&&!(previous&2)&&!(buttons&5)&&s.panelVisible&&s.tabIndex==2&&inObject(s.body,x,y)&&!dropdownOpen(s.root)){
  for(size_t i=0;i<PaletteCount;++i)if(inObject(s.paletteButtons[i],x,y)){choosePalette(PaletteColors[i],true);return true;}
 }
 if((buttons&4)&&!(previous&4)&&inside){panStart(px,py);s.gestureButton=4;}
 if((buttons&4)&&s.panning&&s.gestureButton==4)panMove(px,py);
 if((previous&4)&&!(buttons&4)&&s.panning&&s.gestureButton==4){panMove(px,py);s.panning=false;s.gestureButton=0;return true;}
 if(wheel&&inside){zoomAt(wheel>0?s.zoom*5/4+1:s.zoom*4/5,px,py);return true;}
 if((buttons&2)&&!(previous&2)&&!(buttons&5)&&inside&&!s.dragging)gesturePointer(LV_EVENT_PRESSED,px,py,true);
 else if(s.gestureButton==2){if(buttons&2)gesturePointer(LV_EVENT_PRESSING,px,py,true);else gesturePointer(LV_EVENT_RELEASED,px,py,true);}
 return inside||s.gestureButton==2||s.gestureButton==4||bool((previous&6)&&!(buttons&6));
}
lv_obj_t* scope(){return s.root;}
lv_obj_t* modalScope(){if(!visible())return nullptr;if(s.jobOverlay)return s.jobOverlay;return s.modal&&lv_obj_is_valid(s.modal)&&lv_obj_get_screen(s.modal)==lv_screen_active()?s.modal:nullptr;}
bool visible(){
 if(!s.root||!s.shown||!lv_obj_is_valid(s.root)||lv_obj_get_screen(s.root)!=lv_screen_active())return false;
 for(auto* object=s.root;object;object=lv_obj_get_parent(object))if(lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))return false;
 return true;
}
bool busy(){return s.working;}
bool dirty(){return s.initialized&&s.doc.dirty;}
}
