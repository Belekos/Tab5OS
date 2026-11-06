# USB drives and card readers on Tab5

Connect a USB drive or USB card reader to USB-A, directly or through a compatible hub. Automatic mounting is enabled at startup. Open **Settings > Storage**. The USB drive panel and its actions appear when a device is detected; the mount, open and eject controls follow its state. Automatic mounting can still be changed, and that choice is retained for the session. The microSD and system-resource recovery controls are on the same page.

A mounted volume appears under `/USB`. In Explorer, the storage icon and its arrow open an **SD card / USB** menu: choose a device to open its root directory. This control also returns to the SD card from USB; it disappears when only the SD card is relevant. Explorer and the editor support reading, writing and copying between this device and microSD. File pickers also provide access to it. Internal OS services and their settings remain on microSD. An existing personal folder named `/USB` on microSD prevents USB mounting; it is neither hidden nor deleted. Rename it yourself if necessary.

Choose **Eject** before removing the device. The OS closes the USB preview, search and Explorer picker, then leaves the volume. It refuses new file opens, waits for files that are actually open on USB, synchronizes the drive and reports when removal is safe. Audited tasks that use only microSD no longer block ejection. A program with untracked raw access retains a conservative lock and must release it before unmounting. Modified documents are not deleted or forcibly closed. A synchronization or volume-close failure keeps the device mounted and displays an error.

Unexpected removal invalidates Explorer/editor file handles. The volume is not reused by a new device until the old tasks and files have closed. If the device is reconnected too soon, stop the relevant task, then disconnect and reconnect the device.

Limitations of this release:

- One microSD card and **one USB storage device** at the same time, using FAT16 or FAT32. No exFAT or NTFS; the FAT32 limit is 4 GiB minus one byte per file.
- USB mass-storage BOT/SCSI readers with an ordinary FAT volume. No UASP support or selection among multiple LUNs/partitions.
- No automatic formatting, even if mounting fails.
- A shared USB host for keyboard, mouse and storage. Full-speed hub compatibility mode (12 Mbit/s) remains the default; useful throughput is lower. The global high-speed USB setting remains available when no drive is connected and takes effect after restarting, notably for RTL-SDR. See also [USB gamepads](MANETTES_USB.md). FS/LS keyboards and mice behind an HS hub are not supported by the IDF 5.5.4 stack in this mode.
- This does not turn Tab5 into a USB drive exposed to a PC: this document concerns storage connected **to Tab5**. Existing Wi-Fi file sharing remains available separately.

Driver sources: [usb_host_msc 1.1.4](https://components.espressif.com/components/espressif/usb_host_msc/versions/1.1.4/readme), [ESP-IDF 5.5.4 USB host](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/usb_host.html).

## USB-C and Ethernet

The P4 has two OTG controllers, but the ESP-IDF 5.5.4 stack used here permits only one host at a time. In the Tab5 schematic, USB-A connects to the high-speed controller with a 5 V power output. USB-C connects to the full-speed controller, with device-side CC resistors and a charging VBUS input; it does not use the same power circuit. USB-C provides charging, programming and the USB Serial/JTAG serial device described in [SERIE.md](SERIE.md). This serial service does not create a second USB host. A simple adapter does not provide safe, equivalent support for two host ports.

A USB Ethernet interface is possible with a network driver and a supported chipset. Espressif provides a USB ECM component for P4; this does not automatically cover Realtek/ASIX adapters or every Ethernet port on a hub. No Ethernet driver is included in this release. Choosing the appropriate driver requires the model or USB identifier of the hub/adapter.

References: [official Tab5 schematic](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1132/Tab5_Schematics_PDF.pdf), [USB ECM 0.4.0](https://components.espressif.com/components/espressif/iot_usbh_ecm/versions/0.4.0/readme).
