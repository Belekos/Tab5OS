#ifndef TAB5_USB_INPUT_INTERFACE_H
#define TAB5_USB_INPUT_INTERFACE_H
#include <stddef.h>
#include <string.h>
#include "gamepad_transport.h"

/* Parse within one interface only. A missing HID descriptor must never borrow
 * the next interface's descriptor/endpoint (composite receivers, storage).
 * No allocation, no VID-based override of a genuine HID interface. */
typedef struct {
    const uint8_t *interface_desc, *hid_desc, *endpoint_desc;
    bool xinput;
} tab5_usb_input_interface_t;

static inline bool tab5_usb_next_input_interface(const uint8_t *config, size_t length,
                                                 uint16_t vendor, size_t *cursor,
                                                 tab5_usb_input_interface_t *out)
{
    if (!config || !cursor || !out || length < 9 || config[0] < 9 || config[1] != 2) return false;
    const size_t total = (size_t)config[2] | ((size_t)config[3] << 8);
    if (total > length || total < config[0]) return false;
    size_t at = *cursor < config[0] ? config[0] : *cursor;
    memset(out, 0, sizeof(*out));
    while (at + 2 <= total) {
        const size_t n = config[at];
        if (n < 2 || n > total - at) break;
        if (config[at + 1] != 4) { at += n; continue; }
        if (n < 9) break;
        const uint8_t *iface = config + at;
        const bool xinput = tab5_usb_xinput_interface(vendor, iface[5], iface[6], iface[7], iface[3]);
        const bool candidate = iface[3] == 0 && (iface[5] == 3 || xinput);
        const uint8_t *hid = NULL, *ep = NULL;
        size_t end = at + n;
        bool valid = true;
        unsigned endpoint_count = 0;
        while (end + 2 <= total) {
            const size_t sublength = config[end];
            if (sublength < 2 || sublength > total - end) { valid = false; break; }
            const uint8_t *desc = config + end;
            if (desc[1] == 4) break;
            if (candidate && desc[1] == 0x21 && !xinput && sublength >= 9 &&
                desc[5] >= 1 && desc[6] == 0x22 && (desc[7] || desc[8])) hid = desc;
            if (desc[1] == 5) {
                ++endpoint_count;
                if (candidate && !ep && sublength >= 7 &&
                    tab5_usb_interrupt_input(desc[2], desc[3],
                        ((uint16_t)desc[4] | ((uint16_t)desc[5] << 8)) & 0x7ff, xinput)) ep = desc;
            }
            end += sublength;
        }
        if (!valid || end == total - 1) break;
        at = end;
        *cursor = at;
        if (candidate && ep && (xinput || hid) && endpoint_count == iface[4]) {
            out->interface_desc = iface; out->hid_desc = hid; out->endpoint_desc = ep;
            out->xinput = xinput;
            return true;
        }
    }
    *cursor = total;
    return false;
}
#endif
