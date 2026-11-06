# RTL-SDR

## Interface and reception

The top bar contains the frequency, unit, step, demodulation mode and reception controls. **Receive** becomes **Apply** during reception; this button is enabled only when a reception setting has changed. **Stop** is disabled when reception is stopped and while the USB session is closing. Status remains visible below the tabs. **Scan**, **Pause**/**Resume** and **Hold** also reflect the current scanning state.

The tabs are **Spectrum**, **Scan**, **Detections** and **Settings**. Lengthy explanations have moved out of the working pages and remain in Help and this documentation. PPM correction, de-emphasis, input and axis settings are grouped in **Settings**. Fields use widths, lengths and allowed characters appropriate to their values: decimals for frequencies, integers for PPM, levels, offset and duration. Selecting another field with the keyboard replaces the previous touch input target.

The spectrum keeps square corners regardless of the theme. Its Y-axis marks are distributed over the actual graph height. The application adjusts its layout when available space shrinks, particularly with the keyboard open; pages and the list scroll within their own areas. Selection buttons, action buttons and disabled states use theme colors.

**Hold** keeps the current frequency and stops scanning so you can listen. It is not automatic drift correction. Y-axis minimum and maximum values remain in dBFS and do not claim to measure calibrated RF power. The suggested ranges remain editable; the tuner determines which frequencies are actually accepted.

## Detections: 2,048 entries

The list holds **2,048 detections**. It displays **24 rows per page** and creates widgets only for that page. The counter, pagination and actions remain outside the scrolling list. Changing pages selects its first row, so **Listen** and **Delete** cannot act on a selection hidden on another page. Selecting a row alone does not change reception.

- **Listen** restores the selected entry's frequency, mode and input.
- **Store** adds the current frequency; duplicates are merged.
- **Delete** removes the selected row; **Clear** empties the working list.
- **Save** persists the list and its deletions in `/System/SDR/detections.csv`.

The asterisk indicates unsaved changes. There is no automatic write for every detection. CSV format version 1 remains compatible with earlier saves. Saving uses the existing atomic replacement mechanism; a truncated or invalid file is rejected in full. The file reader's capacity was increased together with the list capacity so a large saved list can be loaded again.

The data list uses about 16 KiB of PSRAM, belongs to the interface and is released when it closes. The temporary CSV read buffer is limited to 98,320 bytes; the write buffer is sized for the actual list (16 to 49,168 bytes). These buffers exist only during reading or writing; no collection of 2,048 buttons is created. The detection queue between the receiver and interface remains bounded and pauses scanning when full.

## USB and FM quality

FM processing includes DC removal after demodulation, a mono filter before conversion to 48 kHz, selectable 50/75 microsecond de-emphasis and disabled RTL digital gain. PPM correction remains adjustable from -200 to +200. Changing the frequency or settings reuses the USB session instead of forcing the receiver to close and reopen.
