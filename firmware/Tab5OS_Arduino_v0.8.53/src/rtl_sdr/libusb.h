#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct libusb_context libusb_context;
typedef struct libusb_device libusb_device;
typedef struct libusb_device_handle libusb_device_handle;
struct libusb_transfer;
struct libusb_device_descriptor {uint8_t bLength,bDescriptorType;uint16_t bcdUSB;uint8_t bDeviceClass,bDeviceSubClass,bDeviceProtocol,bMaxPacketSize0;uint16_t idVendor,idProduct,bcdDevice;uint8_t iManufacturer,iProduct,iSerialNumber,bNumConfigurations;} __attribute__((packed));
#define LIBUSB_ERROR_ACCESS -3
#define LIBUSB_ERROR_NO_DEVICE -4
#define LIBUSB_ERROR_NOT_SUPPORTED -12
#define LIBUSB_ENDPOINT_IN 0x80
#define LIBUSB_ENDPOINT_OUT 0
#define LIBUSB_REQUEST_TYPE_VENDOR 0x40
#define LIBUSB_RECIPIENT_DEVICE 0
int libusb_init(libusb_context**);void libusb_exit(libusb_context*);
ssize_t libusb_get_device_list(libusb_context*,libusb_device***);void libusb_free_device_list(libusb_device**,int);
int libusb_get_device_descriptor(libusb_device*,struct libusb_device_descriptor*);
int libusb_open(libusb_device*,libusb_device_handle**);void libusb_close(libusb_device_handle*);
libusb_device* libusb_get_device(libusb_device_handle*);
int libusb_claim_interface(libusb_device_handle*,int);int libusb_release_interface(libusb_device_handle*,int);
int libusb_kernel_driver_active(libusb_device_handle*,int);int libusb_reset_device(libusb_device_handle*);
int libusb_get_string_descriptor_ascii(libusb_device_handle*,uint8_t,unsigned char*,int);
int libusb_control_transfer(libusb_device_handle*,uint8_t,uint8_t,uint16_t,uint16_t,unsigned char*,uint16_t,unsigned);
int libusb_bulk_transfer(libusb_device_handle*,unsigned char,unsigned char*,int,int*,unsigned);
void tab5_rtl_cancel_callback(int(*callback)(void));
void tab5_rtl_stall_callback(void(*callback)(void));
#ifdef __cplusplus
}
#endif
