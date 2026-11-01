static unsigned receivedReports,receivedErrors;
static void input_event(hid_host_device_handle_t h,hid_host_interface_event_t e,void *arg){(void)h;(void)arg;if(e==HID_HOST_INTERFACE_EVENT_INPUT_REPORT)++receivedReports;if(e==HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR)++receivedErrors;}
static void disconnected(hid_host_device_handle_t h,hid_host_interface_event_t e,void *arg){(void)h;(void)arg;if(e==HID_HOST_INTERFACE_EVENT_DISCONNECTED)++disconnect_events;}
static void notify(hid_host_device_handle_t h,hid_host_driver_event_t e,void *arg){(void)h;(void)e;(void)arg;++notifications;}
static void clean_device(hid_device_t *device){
 while(STAILQ_FIRST(&s_hid_driver->hid_ifaces_tailq))_hid_host_remove_interface(STAILQ_FIRST(&s_hid_driver->hid_ifaces_tailq));
 if(device)hid_host_uninstall_device(device);
}
int main(void){
 hid_driver_t driver={0};s_hid_driver=&driver;driver.client_handle=(void*)1;driver.open_close_mutex=(void*)1;driver.user_cb=notify;
 /* Each possible partial install allocation failure must leave no SRAM held. */
 for(int at=0;at<4;++at){
  alloc_attempt=0;fail_allocation=at;hid_device_t *device=NULL;
  CHECK(hid_host_install_device(1,(void*)2,&device)==ESP_ERR_NO_MEM);
  CHECK(device==NULL);CHECK(live_allocations==0);CHECK(invalid_removals==0);
 }
 fail_allocation=-1;
#ifdef CURRENT_USB_DRIVER
 /* Device-level failure handling and partial composite-list rollback execute
  * production init/list code as well, rather than merely checking for macros. */
 static const uint8_t config[]={9,2,59,0,2,1,0,0x80,50,
  9,4,0,0,1,3,0,0,0,9,0x21,0x11,1,0,1,0x22,50,0,7,5,0x81,3,64,0,4,
  9,4,1,0,1,3,0,0,0,9,0x21,0x11,1,0,1,0x22,50,0,7,5,0x82,3,64,0,4};
 test_config=(const usb_config_desc_t*)config;
 for(int at=0;at<6;++at){
  alloc_attempt=0;fail_allocation=at;CHECK(!hid_host_device_init_attempt(1));
  CHECK(live_allocations==0);CHECK(notifications==0);CHECK(fatal_calls==0);
  CHECK(STAILQ_FIRST(&driver.hid_ifaces_tailq)==NULL);CHECK(STAILQ_FIRST(&driver.hid_devices_tailq)==NULL);
 }
 fail_allocation=-1;
#endif
 hid_device_t *device=NULL;CHECK(hid_host_install_device(1,(void*)2,&device)==ESP_OK);
 device->ctrl_xfer->status=1;CHECK(hid_control_transfer(device,8,50)!=ESP_OK);
 device->ctrl_xfer->status=USB_TRANSFER_STATUS_COMPLETED;CHECK(hid_control_transfer(device,8,50)==ESP_OK);
 usb_intf_desc_t descriptor={9,4,0,0,1,3,0,0,0};
 hid_descriptor_t hid={9,0x21,0x11,1,0,1,0x22,50};
 usb_ep_desc_t endpoint={7,5,0x81,3,64,4};
 CHECK(hid_host_add_interface(device,&descriptor,&hid,&endpoint,false)==ESP_OK);
 hid_iface_t *iface=STAILQ_FIRST(&driver.hid_ifaces_tailq);
 fail_allocation=alloc_attempt;
 CHECK(hid_host_interface_claim_and_prepare_transfer(iface)==ESP_ERR_NO_MEM);
 CHECK(claims==0);CHECK(iface->state==HID_INTERFACE_STATE_IDLE);
 fail_allocation=-1;
 CHECK(hid_host_interface_claim_and_prepare_transfer(iface)==ESP_OK);
 fail_submit=1;CHECK(hid_host_device_start(iface)!=ESP_OK);
 CHECK(iface->state==HID_INTERFACE_STATE_READY);fail_submit=0;
 CHECK(hid_host_device_start(iface)==ESP_OK);
 CHECK(iface->in_xfer->timeout_ms==0);iface->user_cb=input_event;
 unsigned submits=inputSubmits;
 iface->in_xfer->status=USB_TRANSFER_STATUS_COMPLETED;in_xfer_done(iface->in_xfer);
 CHECK(receivedReports==1&&receivedErrors==0&&inputSubmits==submits+1);
 fail_submit=1;in_xfer_done(iface->in_xfer);CHECK(receivedReports==2&&receivedErrors==1);fail_submit=0;
 iface->in_xfer->status=99;in_xfer_done(iface->in_xfer);CHECK(receivedErrors==2);
 submits=inputSubmits;iface->in_xfer->status=USB_TRANSFER_STATUS_CANCELED;in_xfer_done(iface->in_xfer);CHECK(inputSubmits==submits&&receivedErrors==2);
 iface->state=HID_INTERFACE_STATE_READY;iface->in_xfer->status=USB_TRANSFER_STATUS_COMPLETED;in_xfer_done(iface->in_xfer);CHECK(inputSubmits==submits);
 iface->state=HID_INTERFACE_STATE_ACTIVE;iface->user_cb=NULL;
 CHECK(hid_host_device_close(iface)==ESP_OK);
 CHECK(claims==0);clean_device(device);CHECK(live_allocations==0);
 /* Real start path: HID never receives XInput vendor requests. Controllers
  * requiring the request see it before start returns; a STALL stays optional. */
 CHECK(xinput_init_calls==0);
 CHECK(hid_host_install_device(1,(void*)2,&device)==ESP_OK);
 CHECK(hid_host_add_interface(device,&descriptor,&hid,&endpoint,true)==ESP_OK);
 iface=STAILQ_FIRST(&driver.hid_ifaces_tailq);
 CHECK(hid_host_interface_claim_and_prepare_transfer(iface)==ESP_OK);
 CHECK(hid_host_device_start(iface)==ESP_OK);CHECK(xinput_init_calls==1);
 CHECK(iface->state==HID_INTERFACE_STATE_ACTIVE);
 CHECK(hid_host_device_close(iface)==ESP_OK);clean_device(device);CHECK(live_allocations==0);
 CHECK(hid_host_install_device(1,(void*)2,&device)==ESP_OK);
 CHECK(hid_host_add_interface(device,&descriptor,&hid,&endpoint,true)==ESP_OK);
 iface=STAILQ_FIRST(&driver.hid_ifaces_tailq);CHECK(hid_host_interface_claim_and_prepare_transfer(iface)==ESP_OK);
 xinput_init_stall=1;CHECK(hid_host_device_start(iface)==ESP_OK);CHECK(xinput_init_calls==2);
 CHECK(iface->state==HID_INTERFACE_STATE_ACTIVE);xinput_init_stall=0;
 CHECK(hid_host_device_close(iface)==ESP_OK);clean_device(device);CHECK(live_allocations==0);
 /* Repeated hot-unplug, queued app acknowledgements and mode switches.
  * Parent must remain live while either interface still refers to it. */
 for(int round=0;round<2000;++round){
  CHECK(hid_host_install_device(1,(void*)2,&device)==ESP_OK);
  for(int i=0;i<2;++i){descriptor.number=(uint8_t)i;CHECK(hid_host_add_interface(device,&descriptor,&hid,&endpoint,(round&1)!=0)==ESP_OK);}
  hid_iface_t *first=STAILQ_FIRST(&driver.hid_ifaces_tailq),*second=STAILQ_NEXT(first,tailq_entry);
  for(hid_iface_t *p=first;p;p=STAILQ_NEXT(p,tailq_entry)){CHECK(hid_host_interface_claim_and_prepare_transfer(p)==ESP_OK);p->user_cb=disconnected;CHECK(hid_host_device_start(p)==ESP_OK);}
  CHECK(hid_host_device_suspended((void*)2)==ESP_OK);
  CHECK(first->state==HID_INTERFACE_STATE_SUSPENDED&&second->state==HID_INTERFACE_STATE_SUSPENDED);
  CHECK(hid_host_device_resumed((void*)2)==ESP_OK);
  CHECK(first->state==HID_INTERFACE_STATE_ACTIVE&&second->state==HID_INTERFACE_STATE_ACTIVE);
#ifdef CURRENT_USB_DRIVER
  CHECK(!device->disconnected&&!device->disconnecting);
#endif
  CHECK(hid_host_device_disconnected((void*)2)==ESP_OK);
  CHECK(get_hid_device_by_handle((void*)2)==device);
  CHECK(first->parent==device&&second->parent==device);
  CHECK(first->state==HID_INTERFACE_STATE_WAIT_USER_DELETION);
  CHECK(hid_host_device_close(first)==ESP_OK);
  CHECK(get_hid_device_by_handle((void*)2)==device);
  CHECK(hid_host_device_close(second)==ESP_OK);
  CHECK(get_hid_device_by_handle((void*)2)==NULL);
  CHECK(live_allocations==0&&claims==0);
 }
 CHECK(fatal_calls==0);CHECK(invalid_removals==0);
 printf("%s: %d USB lifecycle assertions; %d failures; retained allocations %d; claims %d\n",failures?"FAIL":"PASS",checks,failures,live_allocations,claims);
 return failures?1:0;
}
