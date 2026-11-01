// Read layered projects through the same validated importer as Dessin.
// The transient model never shares/mutates an open drawing or its undo history.
static bool loadProjectImageCanvas(File& file,int maxW,int maxH,
                                    M5Canvas*& canvas,lv_image_dsc_t& dsc,
                                    int& naturalW,int& naturalH,String& error) {
  struct Memory { size_t used=0; } memory;
  struct Allocation { size_t bytes; uint64_t alignment; };
  auto allocate=[](void* context,size_t bytes)->void*{
    auto& state=*static_cast<Memory*>(context);
    constexpr size_t limit=18U*1024U*1024U;
    if(!bytes||bytes>limit-sizeof(Allocation)||bytes+sizeof(Allocation)>limit-state.used)return nullptr;
    const size_t total=bytes+sizeof(Allocation);
    auto reservation=tab5ReserveMemory(Tab5Resources::Owner::Files,{0,0,0,total,total},"Visualisateur");
    if(!reservation)return nullptr;
    auto* block=static_cast<Allocation*>(heap_caps_malloc(total,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!block)return nullptr;
    block->bytes=total;state.used+=total;reservation.allocated(0,total);return block+1;
  };
  auto release=[](void* context,void* data){
    if(!data)return;
    auto& state=*static_cast<Memory*>(context);
    auto* block=static_cast<Allocation*>(data)-1;state.used-=block->bytes;heap_caps_free(block);
  };
  auto read=[](void* context,void* output,size_t bytes)->size_t{
    return static_cast<File*>(context)->read(static_cast<uint8_t*>(output),bytes);
  };
  IEDoc doc{};
  if(!ie_init(&doc,allocate,release,&memory,1,1)){error=T(ie_error(&doc));ie_destroy(&doc);return false;}
  uint32_t lastYield=millis();
  ie_set_yield(&doc,[](void* context){auto& last=*static_cast<uint32_t*>(context);const uint32_t now=millis();if(uint32_t(now-last)>=16){last=now;delay(1);}},&lastYield);
  const bool loaded=ie_load_project(&doc,read,&file);
  if(!loaded){error=T(ie_error(&doc));ie_destroy(&doc);return false;}
  naturalW=doc.current.width;naturalH=doc.current.height;
  float scale=1.0f;
  if(maxW>0&&naturalW>maxW)scale=static_cast<float>(maxW)/naturalW;
  if(maxH>0&&naturalH*scale>maxH)scale=static_cast<float>(maxH)/naturalH;
  const double bytes=static_cast<double>(naturalW)*naturalH*2.0;
  if(bytes*scale*scale>MAX_IMAGE_CANVAS_BYTES)scale=sqrtf(static_cast<float>(MAX_IMAGE_CANVAS_BYTES/bytes));
  const int width=max(1,static_cast<int>(naturalW*scale));
  const int height=max(1,static_cast<int>(naturalH*scale));
  const size_t canvasBytes=static_cast<size_t>(width)*height*2;
  auto reservation=tab5ReserveMemory(Tab5Resources::Owner::Files,{0,0,0,canvasBytes,canvasBytes},"Visualisateur");
  if(!reservation){error=T("Mémoire insuffisante");ie_destroy(&doc);return false;}
  canvas=new M5Canvas(&M5.Display);
  if(canvas){canvas->setPsram(true);canvas->setColorDepth(16);}
  if(!canvas||!canvas->createSprite(width,height)){
    delete canvas;canvas=nullptr;error=T("Canvas image indisponible");ie_destroy(&doc);return false;
  }
  reservation.allocated(0,canvasBytes);
  const size_t rendered=ie_render_scaled_rgb565(&doc,static_cast<uint16_t*>(canvas->getBuffer()),width,width,height,
      0,0,naturalW,naturalH,0xffffff,0xe0e0e0);
  ie_destroy(&doc);
  if(rendered!=static_cast<size_t>(width)*height){
    canvas->deleteSprite();delete canvas;canvas=nullptr;error=T("Format image non pris en charge");return false;
  }
  dsc={};dsc.header.magic=LV_IMAGE_HEADER_MAGIC;dsc.header.cf=LV_COLOR_FORMAT_RGB565;
  dsc.header.w=width;dsc.header.h=height;dsc.header.stride=width*2;
  dsc.data_size=canvasBytes;dsc.data=static_cast<const uint8_t*>(canvas->getBuffer());
  return true;
}

// Viewing WebP uses the existing bounded RGB565 decoder. Viewing an image does
// not require allocating an editable RGBA document at its original resolution.
static bool loadWebpImageCanvas(File& file,int maxW,int maxH,M5Canvas*& canvas,
                               lv_image_dsc_t& dsc,int& naturalW,int& naturalH,String& error) {
  auto reservation=tab5ReserveMemory(Tab5Resources::Owner::Files,
      {0,0,0,12U*1024U*1024U,4U*1024U*1024U},"Visualisateur");
  if(!reservation){error=T("Mémoire insuffisante");return false;}
  Tab5ImageDecode::Image image;
  Tab5ImageDecode::Source source{&file,file.size(),
      [](void* context,uint8_t* output,size_t bytes)->size_t{return static_cast<File*>(context)->read(output,bytes);},
      [](void* context,size_t position)->bool{return static_cast<File*>(context)->seek(position);}};
  Tab5ImageDecode::Callbacks callbacks;
  callbacks.allocationContext=&reservation;
  callbacks.allocated=[](void* context,size_t bytes){static_cast<Tab5Resources::Reservation*>(context)->allocated(0,bytes);};
  const char* reason=nullptr;
  const bool loaded=Tab5WebP::load(source,image,reason,callbacks,1024U*1024U);
  reservation.release();
  if(!loaded){error=T(reason?reason:"Format image non pris en charge");return false;}
  naturalW=image.sourceWidth;naturalH=image.sourceHeight;
  float scale=1.0f;
  if(maxW>0&&image.width>unsigned(maxW))scale=static_cast<float>(maxW)/image.width;
  if(maxH>0&&image.height*scale>maxH)scale=static_cast<float>(maxH)/image.height;
  const int width=max(1,static_cast<int>(image.width*scale));
  const int height=max(1,static_cast<int>(image.height*scale));
  const size_t bytes=static_cast<size_t>(width)*height*2;
  auto canvasReservation=tab5ReserveMemory(Tab5Resources::Owner::Files,{0,0,0,bytes,bytes},"Visualisateur");
  if(!canvasReservation){error=T("Mémoire insuffisante");return false;}
  canvas=new M5Canvas(&M5.Display);
  if(canvas){canvas->setPsram(true);canvas->setColorDepth(16);}
  if(!canvas||!canvas->createSprite(width,height)){
    delete canvas;canvas=nullptr;error=T("Canvas image indisponible");return false;
  }
  canvasReservation.allocated(0,bytes);
  auto* out=static_cast<uint16_t*>(canvas->getBuffer());
  for(int y=0;y<height;++y){
    const auto* row=image.pixels+size_t(y)*image.height/height*image.width;
    for(int x=0;x<width;++x)out[size_t(y)*width+x]=row[size_t(x)*image.width/width];
    if(!(y&31))delay(1);
  }
  dsc={};dsc.header.magic=LV_IMAGE_HEADER_MAGIC;dsc.header.cf=LV_COLOR_FORMAT_RGB565;
  dsc.header.w=width;dsc.header.h=height;dsc.header.stride=width*2;
  dsc.data_size=bytes;dsc.data=static_cast<const uint8_t*>(canvas->getBuffer());
  return true;
}
