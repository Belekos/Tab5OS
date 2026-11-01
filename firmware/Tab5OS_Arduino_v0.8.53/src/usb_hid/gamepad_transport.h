#ifndef TAB5_USB_GAMEPAD_TRANSPORT_H
#define TAB5_USB_GAMEPAD_TRANSPORT_H
#include <stdbool.h>
#include <stdint.h>

/* Wired Xbox 360 / XInput interfaces. Do not claim arbitrary vendor devices,
 * Xbox One (FF/47/D0), wireless receivers (FF/5D/81), or secondary interfaces.
 * SN30 Pro XInput is documented as 2dc8:6001 by the Linux xpad device table.
 * Some 8BitDo firmware identifies as the Microsoft-compatible controller. */
static inline bool tab5_usb_xinput_interface(uint16_t vendor, uint8_t klass,
                                            uint8_t subclass, uint8_t protocol,
                                            uint8_t alternate)
{
    return (vendor == 0x2dc8 || vendor == 0x045e) && klass == 0xff &&
           subclass == 0x5d && protocol == 0x01 && alternate == 0;
}

static inline bool tab5_usb_interrupt_input(uint8_t address, uint8_t attributes,
                                           uint16_t packet_size, bool xinput)
{
    return (address & 0x80) && (address & 0x0f) && (attributes & 3) == 3 &&
           packet_size >= (xinput ? 20 : 1) && packet_size <= 512;
}
#endif
