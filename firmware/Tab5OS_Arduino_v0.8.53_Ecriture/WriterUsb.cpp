#include "WriterUsb.h"
#include "Tab5UsbStorage.h"
#include "Tab5UsbPrinter.h"
#include "WriterHid.h"
#include "WriterMouse.h"
#include <esp_heap_caps.h>
#include <new>
#include <Preferences.h>

#include <Arduino.h>
#include <M5Unified.h>
#include <esp_intr_alloc.h>
#include <freertos/queue.h>
#include <usb/usb_host.h>
#include <hal/usb_dwc_ll.h>

#include <atomic>
#include <algorithm>
#include <cstring>

#include "src/usb_hid/hid_host.h"

namespace writing {
static bool highSpeedActive=false;
namespace {
struct Report {
  HidKeys keys;
  bool disconnected;uint8_t slot=0;
};
struct DeviceEvent {
  hid_host_device_handle_t handle;
  bool connected;
};
QueueHandle_t reports = nullptr, devices = nullptr, mouseReports = nullptr;
struct Interface {hid_host_device_handle_t handle=nullptr;HidMouseDescriptor mouse;bool isMouse=false,bootMouse=false,activeMouse=false;uint8_t buttons=0;HidGamepadDescriptor* pad=nullptr;bool xinput=false,helpDown=false;HidKeyboardDescriptor* keys=nullptr;bool bootKeyboard=false;HidKeys held;};
constexpr unsigned MaxInterfaces=8;
// Disconnect acknowledgements cannot be dropped when the event queue is full.
// Each occupied slot keeps its handle until the worker has closed it.
std::atomic<bool> detachPending[MaxInterfaces],recoverPending[MaxInterfaces];
uint32_t recoverAt[MaxInterfaces]{};
std::atomic<uint32_t> gameButtons{0};
Interface* interfaces=nullptr;
struct PadState{std::atomic<uint32_t> identity{0},buttons{0},directions{0},mapping{0x0a090201},extended{0x06050403},deviceId{0},reports{0},rejected{0},errors{0},restarts{0},rawSize{0},interfaceNumber{0},axisMask{0};std::atomic<int32_t> axes[4];std::atomic<uint8_t> raw[16];std::atomic<bool> xinput{false};};
PadState pads[MaxInterfaces];std::atomic<bool> helpPressed{false},padActivity{false};
void releasePad(Interface& device){unsigned i=&device-interfaces;pads[i].identity=0;pads[i].buttons=0;pads[i].directions=0;pads[i].deviceId=0;pads[i].reports=0;pads[i].rejected=0;pads[i].errors=0;pads[i].restarts=0;pads[i].rawSize=0;pads[i].axisMask=0;for(auto& a:pads[i].axes)a=0;pads[i].xinput=false;recoverPending[i]=false;recoverAt[i]=0;device.helpDown=false;}
void destroyPad(Interface& device){releasePad(device);if(device.pad){device.pad->~HidGamepadDescriptor();heap_caps_free(device.pad);device.pad=nullptr;}}

std::atomic<unsigned> mouseCount{0};
TaskHandle_t libraryTask = nullptr, deviceTask = nullptr;
std::atomic<bool> connected{false}, overflow{false};
bool attempted = false, started = false;
std::atomic<const char*> status{"USB non démarré"};
KeyDecoder decoder;
HidKeys previous;HidKeyboardMux keyboardMux;
std::atomic<uint32_t> keyboardHeld[MaxInterfaces];std::atomic<bool> keyActive[MaxInterfaces];
// Mouse callbacks run before usbPoll; publish modifiers directly with each HID report.
std::atomic<uint8_t> keyboardModifiers[MaxInterfaces]{};
uint8_t pending[256] = {}, modifiers = 0, repeating = 0;
CommandLatch commandLatch;
size_t pendingIndex = 0, pendingCount = 0;
uint32_t repeatAt = 0, helpAt = 0;bool helpSeen=false;
bool acceptHelp(uint32_t now){if(helpSeen&&uint32_t(now-helpAt)<120)return false;helpAt=now;helpSeen=true;return true;}

void publishKeyboard(Interface& device,bool disconnected=false){
  unsigned index=&device-interfaces;
  Report report{};report.slot=index;report.keys=device.held;report.disconnected=disconnected;
  keyboardModifiers[index]=disconnected?0:report.keys.modifiers;
  keyActive[index]=!disconnected;
  uint32_t held=0;const uint8_t usages[]={79,80,82,81,29,27,42,40};
  if(!disconnected){for(unsigned i=0;i<8;++i)if(report.keys.down(usages[i]))held|=1u<<i;if(report.keys.down(26))held|=1u<<4;if(report.keys.down(41))held|=1u<<8;const uint8_t extra[]={22,4,20,8};for(unsigned j=0;j<4;++j)if(report.keys.down(extra[j]))held|=1u<<(11+j);}
  keyboardHeld[index]=held;held=0;bool present=false;
  for(unsigned i=0;i<MaxInterfaces;++i){held|=keyboardHeld[i].load();present|=keyActive[i].load();}
  connected=present;gameButtons=held;
  if(xQueueSend(reports,&report,0)!=pdTRUE)overflow.store(true);
}
void destroyKeyboard(Interface& device){if(device.keys){publishKeyboard(device,true);device.keys->~HidKeyboardDescriptor();heap_caps_free(device.keys);device.keys=nullptr;}device.held={};device.bootKeyboard=false;}

void libraryLoop(void*) {
  for (;;) {
    uint32_t flags = 0;
    if(!highSpeedActive&&!USB_DWC_LL_GET_HW(0)->hcfg_reg.fslssupp)usb_dwc_ll_hcfg_set_fsls_supp_only(USB_DWC_LL_GET_HW(0));
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
  }
}
void interfaceEvent(hid_host_device_handle_t handle, hid_host_interface_event_t e, void* arg) {
  auto* device=static_cast<Interface*>(arg);
  if (e == HID_HOST_INTERFACE_EVENT_INPUT_REPORT) {
    uint8_t raw[512];
    size_t size = 0;
    Report report{};
    if(device&&(device->pad||device->xinput)&&hid_host_device_get_raw_input_report_data(handle,raw,sizeof(raw),&size)==ESP_OK){
      auto& state=pads[device-interfaces];GamepadReport pad;const uint32_t id=state.deviceId.load();
      state.rawSize=uint32_t(size);for(unsigned i=0;i<16;++i)state.raw[i]=i<size?raw[i]:0;
      // Restore the descriptor-first HID path used by the first integration.
      // A modern 9-byte ID-prefixed packet must not be mistaken for the old
      // 8BitDo layout merely because its device has the same VID/PID.
      bool decoded=false;
      if(!device->xinput&&device->pad->gamepad()&&!device->pad->matchesReportSize(raw,size))
        decoded=decodeLegacy8BitDoGamepad(id&0xffff,id>>16,raw,size,pad);
      if(!decoded)decoded=device->xinput?decodeXInputGamepad(raw,size,pad):device->pad->decode(raw,size,pad);
      if(decoded){state.reports.fetch_add(1);if(state.buttons.load()!=pad.buttons||state.directions.load()!=pad.directions)padActivity=true;state.buttons=pad.buttons;state.directions=pad.directions;state.axisMask=pad.axisMask;for(unsigned a=0;a<4;++a)state.axes[a]=pad.axes[a];if(pad.help&&!device->helpDown)helpPressed=true;device->helpDown=pad.help;}
      else state.rejected.fetch_add(1);
    }
    if(device&&device->isMouse&&hid_host_device_get_raw_input_report_data(handle,raw,sizeof(raw),&size)==ESP_OK){
      MouseReport mouse;if(device->bootMouse?HidMouseDescriptor::boot(raw,size,mouse):device->mouse.decode(raw,size,mouse)){
        device->buttons=(device->buttons&~mouse.buttonMask)|(mouse.buttons&mouse.buttonMask);
        if(!device->activeMouse&&(mouse.x||mouse.y||mouse.wheel||mouse.horizontal||device->buttons)){device->activeMouse=true;mouseCount.fetch_add(1);}
        mouse.buttons=0;for(unsigned i=0;i<MaxInterfaces;++i)mouse.buttons|=interfaces[i].buttons;
        if(device->activeMouse&&xQueueSend(mouseReports,&mouse,0)!=pdTRUE){xQueueReset(mouseReports);xQueueSend(mouseReports,&mouse,0);}
      }
    }
    if (device && device->keys && hid_host_device_get_raw_input_report_data(handle, raw, sizeof(raw), &size) == ESP_OK &&
        (device->bootKeyboard ? HidKeyboardDescriptor::boot(raw, size, device->held) : device->keys->decode(raw, size, device->held))) {
      publishKeyboard(*device);
    }
  } else if (e == HID_HOST_INTERFACE_EVENT_DISCONNECTED) {
    if(device)releasePad(*device);
    if(device&&device->keys){device->held={};publishKeyboard(*device,e==HID_HOST_INTERFACE_EVENT_DISCONNECTED);}
    if(device&&device->isMouse){if(device->activeMouse)mouseCount.fetch_sub(1);device->activeMouse=false;device->isMouse=false;device->buttons=0;MouseReport mouse{};for(unsigned i=0;i<MaxInterfaces;++i)mouse.buttons|=interfaces[i].buttons;
      // Preserve the latest release even if another mouse remains connected.
      if(xQueueSend(mouseReports,&mouse,0)!=pdTRUE){xQueueReset(mouseReports);xQueueSend(mouseReports,&mouse,0);}}
    if(device)detachPending[device-interfaces]=true;
  } else if (e == HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR) {
    if(device){auto& state=pads[device-interfaces];state.buttons=0;state.directions=0;state.axisMask=0;for(auto& a:state.axes)a=0;state.errors.fetch_add(1);recoverPending[device-interfaces]=true;device->helpDown=false;}
    if(device&&device->keys){device->held={};publishKeyboard(*device,e==HID_HOST_INTERFACE_EVENT_DISCONNECTED);}
    if(device&&device->isMouse){device->buttons=0;MouseReport mouse{};for(unsigned i=0;i<MaxInterfaces;++i)mouse.buttons|=interfaces[i].buttons;
      if(xQueueSend(mouseReports,&mouse,0)!=pdTRUE){xQueueReset(mouseReports);xQueueSend(mouseReports,&mouse,0);}}
  }
}
void driverEvent(hid_host_device_handle_t handle, hid_host_driver_event_t, void*) {
  DeviceEvent event{handle, true};
  if (xQueueSend(devices, &event, 0) != pdTRUE) overflow.store(true);
}
void serviceRecovery(){
    // Never restart inside the USB event callback: halt/flush and optional
    // XInput control requests require that task to keep processing events.
    for(unsigned i=0;i<MaxInterfaces;++i)if(!detachPending[i].load()&&recoverPending[i].load()&&int32_t(millis()-recoverAt[i])>=0){
      auto& slot=interfaces[i];if(!slot.handle){recoverPending[i]=false;continue;}
      recoverPending[i]=false;
      // A previous retry may have stopped successfully but failed to submit.
      // In that case stop reports READY and start is still the right action.
      hid_host_device_stop(slot.handle);
      if(hid_host_device_start(slot.handle)==ESP_OK)pads[i].restarts.fetch_add(1);
      else if(!detachPending[i].load()){recoverPending[i]=true;recoverAt[i]=millis()+1000;}
    }
}
void deviceLoop(void*) {
  DeviceEvent e;
  for (;;) {
    Tab5UsbStorage::serviceWorker();
    for(unsigned i=0;i<MaxInterfaces;++i)if(detachPending[i].exchange(false)){
      auto& slot=interfaces[i];
      if(slot.handle&&hid_host_device_close(slot.handle)!=ESP_OK){detachPending[i]=true;continue;}
      destroyPad(slot);destroyKeyboard(slot);slot=Interface{};
    }
    serviceRecovery();
    if (xQueueReceive(devices, &e, pdMS_TO_TICKS(100)) != pdTRUE) continue;
    if (!e.connected) {
      hid_host_device_close(e.handle);
      for(unsigned i=0;i<MaxInterfaces;++i)if(interfaces[i].handle==e.handle){destroyPad(interfaces[i]);destroyKeyboard(interfaces[i]);interfaces[i]=Interface{};}
      continue;
    }
    hid_host_dev_params_t params{};
    if(hid_host_device_get_params(e.handle,&params)!=ESP_OK)continue;
    Interface* slot=nullptr;for(unsigned i=0;i<MaxInterfaces;++i)if(!interfaces[i].handle){slot=&interfaces[i];break;}if(!slot)continue;
    slot->handle=e.handle;hid_host_device_config_t cfg{};cfg.callback=interfaceEvent;cfg.callback_arg=slot;
    if(hid_host_device_open(e.handle,&cfg)!=ESP_OK){slot->handle=nullptr;continue;}
    slot->xinput=params.xinput;pads[slot-interfaces].interfaceNumber=params.iface_num;
    size_t length=0;const uint8_t* raw=params.xinput?nullptr:hid_host_get_report_descriptor(e.handle,&length);
    slot->isMouse=slot->mouse.parse(raw,length);
    auto* memory=params.xinput?nullptr:heap_caps_malloc(sizeof(HidGamepadDescriptor),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(memory){slot->pad=new(memory)HidGamepadDescriptor;if(!slot->pad->parse(raw,length)){slot->pad->~HidGamepadDescriptor();heap_caps_free(slot->pad);slot->pad=nullptr;}}
    if(slot->xinput||(slot->pad&&slot->pad->gamepad())){
      uint32_t identity=2166136261u;for(size_t i=0;i<length;++i)identity=(identity^raw[i])*16777619u;
      // A separate identity prevents XInput and HID mappings from overwriting
      // each other when a controller keeps its VID/PID across mode changes.
      if(slot->xinput)identity=(identity^0x58494e50u)*16777619u;
      auto* info=static_cast<hid_host_dev_info_t*>(heap_caps_malloc(sizeof(hid_host_dev_info_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
      if(info){if(hid_host_get_device_info(e.handle,info)==ESP_OK){identity=(identity^info->VID)*16777619u;identity=(identity^info->PID)*16777619u;pads[slot-interfaces].deviceId=uint32_t(info->VID)|(uint32_t(info->PID)<<16);}heap_caps_free(info);}
      pads[slot-interfaces].xinput=slot->xinput;
      char key[12];snprintf(key,sizeof(key),"%08lx",(unsigned long)identity);Preferences preferences;uint32_t mapping=0x0a090201,extended=0x06050403;
      const uint32_t model=pads[slot-interfaces].deviceId.load();
      if(!slot->xinput&&(model&0xffff)==0x2dc8&&((model>>16)==0x6000||(model>>16)==0x6001)){
        mapping=0x0c0b0201;extended=0x08070504; // SN30/SF30 Pro D-input HID numbering.
      }
      if(preferences.begin("gamepads",true)){mapping=preferences.getUInt(key,mapping);snprintf(key,sizeof(key),"e%08lx",(unsigned long)identity);extended=preferences.getUInt(key,extended);preferences.end();}
      bool valid=true;for(unsigned i=0;i<4;++i){unsigned b=(mapping>>(8*i))&255;if(!b||b>32)valid=false;}
      pads[slot-interfaces].mapping=valid?mapping:0x0a090201;valid=true;for(unsigned i=0;i<4;++i){unsigned b=(extended>>(8*i))&255;if(!b||b>32)valid=false;}pads[slot-interfaces].extended=valid?extended:0x06050403;pads[slot-interfaces].identity=identity?identity:1;
    }
    bool key=false,bootKey=!params.xinput&&params.sub_class==HID_SUBCLASS_BOOT_INTERFACE&&params.proto==HID_PROTOCOL_KEYBOARD;
    auto* keyMemory=params.xinput?nullptr:heap_caps_malloc(sizeof(HidKeyboardDescriptor),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(keyMemory){slot->keys=new(keyMemory)HidKeyboardDescriptor;key=slot->keys->parse(raw,length)||bootKey;}
    // A composite receiver may put keyboard and mouse reports in one interface.
    // Keep Report protocol whenever it carries a mouse report descriptor.
    if(key&&!slot->isMouse&&!slot->pad&&bootKey)bootKey=hid_class_request_set_protocol(e.handle,HID_REPORT_PROTOCOL_BOOT)==ESP_OK;
    else bootKey=false;
    if(key&&!bootKey&&!slot->keys->parse(raw,length))key=false;
    if(!slot->isMouse&&params.sub_class==HID_SUBCLASS_BOOT_INTERFACE&&params.proto==HID_PROTOCOL_MOUSE){
      slot->bootMouse=hid_class_request_set_protocol(e.handle,HID_REPORT_PROTOCOL_BOOT)==ESP_OK;slot->isMouse=slot->bootMouse;
    }else if(!bootKey&&params.sub_class==HID_SUBCLASS_BOOT_INTERFACE)hid_class_request_set_protocol(e.handle,HID_REPORT_PROTOCOL_REPORT);
    if(!key)destroyKeyboard(*slot);
    if(!key&&!slot->isMouse&&!slot->pad&&!slot->xinput){hid_host_device_close(e.handle);detachPending[slot-interfaces]=true;continue;}
    if(key)slot->bootKeyboard=bootKey;
    if(!slot->xinput)hid_class_request_set_idle(e.handle,0,0);
    if(hid_host_device_start(e.handle)!=ESP_OK){hid_host_device_close(e.handle);detachPending[slot-interfaces]=true;continue;}
    if(key)publishKeyboard(*slot);

  }
}
void clearKeys() {
  previous = {};keyboardMux.clear();
  pendingIndex = pendingCount = 0;
  repeating = 0;
  modifiers = 0;
  commandLatch.reset();
  decoder.reset();helpSeen=false;
}
}  // namespace
static bool speedLoaded=false,speedStored=false;
bool usbHighSpeed(){if(!speedLoaded){Preferences p;if(p.begin("tab5_usb",true)){speedStored=p.getBool("highspeed",false);p.end();}speedLoaded=true;}return speedStored;}
bool usbHighSpeed(bool enabled){Preferences p;if(!p.begin("tab5_usb",false))return false;bool ok=p.putBool("highspeed",enabled);p.end();if(ok){speedStored=enabled;speedLoaded=true;}return ok;}
bool usbHighSpeedActive(){return highSpeedActive;}
bool usbBegin() {
  if (attempted) return started;
  attempted = true;
  interfaces=static_cast<Interface*>(heap_caps_malloc(MaxInterfaces*sizeof(Interface),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!interfaces){status="USB : mémoire insuffisante";return false;}
  for(unsigned i=0;i<MaxInterfaces;++i)new(&interfaces[i])Interface;
  reports = xQueueCreate(24, sizeof(Report));
  devices = xQueueCreate(16, sizeof(DeviceEvent));
  mouseReports=xQueueCreate(32,sizeof(MouseReport));
  if (!reports || !devices || !mouseReports) {
    if (reports) vQueueDelete(reports);
    if (devices) vQueueDelete(devices);
    if(mouseReports)vQueueDelete(mouseReports);mouseReports=nullptr;
    reports = devices = nullptr;heap_caps_free(interfaces);interfaces=nullptr;
    status = "USB : mémoire insuffisante";
    return false;
  }
  // Tab5 USB-A supply is IO expander #1, output #3. M5Unified owns it.
  M5.Power.setExtOutput(true, m5::ext_USB);
  usb_host_config_t cfg{};
  cfg.skip_phy_setup = false;
  cfg.root_port_unpowered = true;
  cfg.intr_flags = ESP_INTR_FLAG_LEVEL1;
  auto releaseQueues=[](){
    if(reports)vQueueDelete(reports);if(devices)vQueueDelete(devices);
    if(mouseReports)vQueueDelete(mouseReports);mouseReports=nullptr;
    reports=devices=nullptr;heap_caps_free(interfaces);interfaces=nullptr;
  };
  if (usb_host_install(&cfg) != ESP_OK) {
    releaseQueues();
    status = "USB : hôte indisponible";
    return false;
  }
  // IDF 5.5 has no transaction translator for LS/FS devices behind a HS hub.
  // Configure the existing HS controller for FS/LS before allowing enumeration.
  highSpeedActive=usbHighSpeed();if(!highSpeedActive)usb_dwc_ll_hcfg_set_fsls_supp_only(USB_DWC_LL_GET_HW(0));
  if(usb_host_lib_set_root_port_power(true)!=ESP_OK){usb_host_uninstall();releaseQueues();status="USB : hôte indisponible";return false;}
  if (xTaskCreate(libraryLoop, "writer_usb", 4096, nullptr, 3, &libraryTask) != pdTRUE ||
      xTaskCreate(deviceLoop, "writer_hid", 6144, nullptr, 3, &deviceTask) != pdTRUE) {
    if (libraryTask) vTaskDelete(libraryTask);
    if (deviceTask) vTaskDelete(deviceTask);
    libraryTask = deviceTask = nullptr;
    usb_host_device_free_all();
    usb_host_uninstall();
    releaseQueues();
    status = "USB : tâche indisponible";
    return false;
  }
  hid_host_driver_config_t hid{};
  hid.create_background_task = true;
  hid.task_priority = 4;
  hid.stack_size = 4096;
  hid.core_id = 0;
  hid.callback = driverEvent;
  if (hid_host_install(&hid) != ESP_OK) {
    if(libraryTask)vTaskDelete(libraryTask);if(deviceTask)vTaskDelete(deviceTask);
    libraryTask=deviceTask=nullptr;
    usb_host_device_free_all();usb_host_uninstall();releaseQueues();
    status = "USB : pilote indisponible";
    return false;
  }
  Tab5UsbStorage::begin();
  Tab5UsbPrinter::begin();
  started = true;
  status = "USB : branchez un périphérique";
  return true;
}
bool usbConnected() { return connected.load(); }
uint8_t usbModifiers(){uint8_t held=0;for(auto& value:keyboardModifiers)held|=value.load();return held;}
uint32_t usbGamepadButtons(){uint32_t held=0;for(auto& p:pads)if(p.identity.load()){held|=p.directions.load();uint32_t raw=p.buttons.load();for(unsigned i=0;i<8;++i){unsigned button=((i<4?p.mapping.load():p.extended.load())>>((i%4)*8))&255;if(button&&button<=32&&(raw&(1u<<(button-1))))held|=1u<<(i<4?i+4:i+7);}}return held;}
uint32_t usbGameButtons(){return gameButtons.load()|usbGamepadButtons();}
bool usbGamepadActivity(){return padActivity.exchange(false);}
bool usbGamepadInfo(GamepadInfo& out){for(auto& p:pads){uint32_t id=p.identity.load();if(!id)continue;out.identity=id;out.buttons=p.buttons.load();out.directions=p.directions.load();out.reports=p.reports.load();out.rejected=p.rejected.load();out.errors=p.errors.load();out.restarts=p.restarts.load();out.rawSize=uint8_t(std::min(p.rawSize.load(),uint32_t(255)));out.interfaceNumber=uint8_t(p.interfaceNumber.load());for(unsigned i=0;i<16;++i)out.raw[i]=p.raw[i].load();out.axisMask=uint8_t(p.axisMask.load());for(unsigned i=0;i<4;++i)out.axes[i]=int16_t(p.axes[i].load());uint32_t deviceId=p.deviceId.load();out.vendor=deviceId&0xffff;out.product=deviceId>>16;out.xinput=p.xinput.load();for(unsigned i=0;i<8;++i)out.mapping[i]=((i<4?p.mapping.load():p.extended.load())>>((i%4)*8))&255;return true;}out={};return false;}
bool usbGamepadMap(uint32_t identity,const uint8_t mapping[8]){if(!identity||!mapping)return false;uint32_t packed[2]={};for(unsigned i=0;i<8;++i){if(!mapping[i]||mapping[i]>32)return false;packed[i/4]|=uint32_t(mapping[i])<<((i%4)*8);}bool found=false;for(auto& p:pads)found|=p.identity.load()==identity;if(!found)return false;
 char key[12];Preferences preferences;if(!preferences.begin("gamepads",false))return false;bool ok=true;for(unsigned i=0;i<2;++i){snprintf(key,sizeof(key),i?"e%08lx":"%08lx",(unsigned long)identity);if(preferences.putUInt(key,packed[i])!=sizeof(uint32_t))ok=false;}preferences.end();if(!ok)return false;for(auto& p:pads)if(p.identity.load()==identity){p.mapping=packed[0];p.extended=packed[1];}return true;
}
bool usbMouseConnected(){return mouseCount.load()>0;}
bool usbMousePoll(MouseReport& report){return mouseReports&&xQueueReceive(mouseReports,&report,0)==pdTRUE;}
const char* usbStatus() { if(connected.load()||mouseCount.load())return "USB connecté";for(auto& p:pads)if(p.identity.load())return "Manette connectée";return status.load(); }
void usbMapping(const KeyMapping* data,size_t count){decoder.mapping(data,count);clearKeys();}
void usbFrench(bool enabled) {
  decoder.french(enabled);
  clearKeys();
}
bool usbPoll(KeyEvent& event, uint32_t now) {
  if (!started) return false;
  if(helpPressed.exchange(false)&&acceptHelp(now)){event={};event.key=Key::F1;return true;}
  if (overflow.exchange(false)) {
    clearKeys();
    xQueueReset(reports);
    status = "USB : entrée interrompue, retaper";
    return false;
  }
  for (;;) {
    while (pendingIndex < pendingCount) {
      uint8_t code = pending[pendingIndex++];
      if (code < 4) continue;
      event = decoder.press(code, modifiers);
      if(event.key==Key::F1&&!acceptHelp(now))continue;
      repeating = repeatable(event) ? code : 0;
      repeatAt = now + 450;
      if (event.key != Key::None) return true;
    }
    Report report;
    if (xQueueReceive(reports, &report, 0) != pdTRUE) break;
    report.keys=keyboardMux.update(report.slot,report.keys,report.disconnected);
    if(report.disconnected&&!connected.load())status="USB débranché";
    uint8_t anyKeys[6]{};
    for (uint8_t byte : report.keys.bits) anyKeys[0] |= byte;
    bool commandReleased = commandLatch.report(report.keys.modifiers, anyKeys);
    modifiers = report.keys.modifiers;
    pendingCount = 0;
    for (unsigned code = 4; code < 224; ++code)
      if (report.keys.down(code) && !previous.down(code)) pending[pendingCount++] = code;
    previous = report.keys;
    if (repeating && !previous.down(repeating)) repeating = 0;
    pendingIndex = 0;
    if (commandReleased) {
      event = {};
      event.key = Key::Command;
      return true;
    }
  }
  if (repeating && static_cast<int32_t>(now - repeatAt) >= 0) {
    repeatAt = now + 40;
    event = decoder.press(repeating, modifiers);
    // Repeat editing/navigation only, never Ctrl/Alt commands or dead keys.
    return repeatable(event);
  }
  return false;
}
}  // namespace writing
