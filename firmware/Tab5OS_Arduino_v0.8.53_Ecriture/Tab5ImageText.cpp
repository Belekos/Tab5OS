#include "Tab5ImageText.h"
#include <algorithm>
#include <cstring>
#include <cstdint>

namespace Tab5ImageText {
namespace {
int scaled(int n,int scale){return int((int64_t(n)*scale+99)/100);}
}
lv_point_t position(lv_obj_t* layout,uint32_t index,int scale){
 lv_point_t p{};if(layout)lv_label_get_letter_pos(layout,index,&p);
 p.x=scaled(p.x,scale);p.y=scaled(p.y,scale);return p;
}
int lineHeight(lv_obj_t* layout,int scale){return scaled(lv_font_get_line_height(lv_obj_get_style_text_font(layout,LV_PART_MAIN)),scale);}
void release(Raster& r,IEDoc* doc){if(r.pixels)doc->release(doc->memory_context,r.pixels);if(r.row)doc->release(doc->memory_context,r.row);r=Raster{};}
bool prepare(Raster& r,IEDoc* doc,lv_obj_t* layout,const char* text,int x,int y,int scale){
 r=Raster{};if(!doc||!layout||!text||scale<25||scale>400)return false;
 const auto* font=lv_obj_get_style_text_font(layout,LV_PART_MAIN);lv_point_t measured{};
 lv_text_get_size(&measured,text,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
 // One transparent pixel around sampling limits avoids edge colour extension.
 const int nativeW=std::max(1,int(measured.x)),nativeH=std::max(1,int(measured.y));
 r.textW=scaled(nativeW,scale);r.textH=scaled(nativeH,scale);r.originX=x;r.originY=y;r.scale=scale;
 r.x=std::max(0,x);r.y=std::max(0,y);
 r.width=std::max(0,std::min(doc->current.width,x+r.textW)-r.x);r.height=std::max(0,std::min(doc->current.height,y+r.textH)-r.y);
 if(!*text||!r.width||!r.height)return true;
 r.sourceX=std::max(0,(r.x-x)*100/scale-1);r.sourceY=std::max(0,(r.y-y)*100/scale-1);
 r.sourceW=std::min(nativeW,((r.x-x+r.width)*100+scale-1)/scale+1)-r.sourceX;
 r.sourceH=std::min(nativeH,((r.y-y+r.height)*100+scale-1)/scale+1)-r.sourceY;
 r.stride=lv_draw_buf_width_to_stride(r.sourceW,LV_COLOR_FORMAT_A8);
 const size_t bytes=size_t(r.stride)*r.sourceH;
 r.pixels=static_cast<uint8_t*>(doc->alloc(doc->memory_context,bytes));
 r.row=static_cast<uint8_t*>(doc->alloc(doc->memory_context,r.width));
 if(!r.pixels||!r.row){release(r,doc);return false;}memset(r.pixels,0,bytes);
 auto* canvas=lv_canvas_create(lv_obj_get_parent(layout));if(!canvas){release(r,doc);return false;}
 lv_obj_add_flag(canvas,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(canvas,LV_OBJ_FLAG_CLICKABLE);
 lv_draw_buf_t buffer{};
 if(lv_draw_buf_init(&buffer,r.sourceW,r.sourceH,LV_COLOR_FORMAT_A8,r.stride,r.pixels,bytes)!=LV_RESULT_OK){lv_obj_delete(canvas);release(r,doc);return false;}
 lv_canvas_set_draw_buf(canvas,&buffer);lv_layer_t layer;lv_canvas_init_layer(canvas,&layer);
 lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.font=font;d.text=text;d.color=lv_color_white();d.opa=LV_OPA_COVER;d.bidi_dir=lv_obj_get_style_base_dir(layout,LV_PART_MAIN);d.align=LV_TEXT_ALIGN_LEFT;d.flag=LV_TEXT_FLAG_NONE;
 lv_area_t area{-r.sourceX,-r.sourceY,nativeW-r.sourceX-1,nativeH-r.sourceY-1};lv_draw_label(&layer,&d,&area);lv_canvas_finish_layer(canvas,&layer);lv_obj_delete(canvas);return true;
}
void stamp(const Raster& r,IEDoc* doc,uint32_t color,int opacity){
 if(!r.pixels||!r.row)return;
 auto sample=[&](int x,int y)->uint32_t{return x<0||y<0||x>=r.sourceW||y>=r.sourceH?0:r.pixels[size_t(y)*r.stride+x];};
 for(int y=0;y<r.height;++y){
  const int64_t fy=int64_t(r.y+y-r.originY)*100*256/r.scale-int64_t(r.sourceY)*256;const int sy=int(fy/256),wy=int(fy%256);
  for(int x=0;x<r.width;++x){const int64_t fx=int64_t(r.x+x-r.originX)*100*256/r.scale-int64_t(r.sourceX)*256;const int sx=int(fx/256),wx=int(fx%256);
   const uint32_t top=sample(sx,sy)*(256-wx)+sample(sx+1,sy)*wx,bottom=sample(sx,sy+1)*(256-wx)+sample(sx+1,sy+1)*wx;
   r.row[x]=uint8_t((top*(256-wy)+bottom*wy+32768)/65536);
  }
  ie_mask_stamp(doc,r.x,r.y+y,r.row,r.width,1,r.width,color,opacity);
  if((y&31)==0&&doc->yield)doc->yield(doc->yield_context);
 }
}
}
