# Drawing raster engine

`image_core.c` is the C engine compiled into the firmware for Drawing and reused by the ELF example in this directory. It depends on neither LVGL, a display nor a filesystem: the host supplies allocation/deallocation, file transfers and scheduler cooperation.

## Representation and limits

Pixels use non-premultiplied RGBA (`0xAABBGGRR`). Each layer contains a pixel array, a 23-byte name, opacity from 0 to 255 and a visible/hidden state. The image supports up to 8 layers and 8192 pixels per dimension; combined layer pixel storage stays below 8 MiB, with 64 KiB reserved for metadata and the allocator. Thus 1024 × 1024 fits one layer, and 512 × 512 fits seven layers. The effective limit also depends on available memory.

History retains at most 16 states and 8 MiB. The oldest states are freed when a limit or allocation requires it. There is no guaranteed fixed number of undo steps: larger images retain fewer. Transformations reuse the most recent undo state as their source to avoid a third image copy.

Before changing pixels, the engine must be able to retain the previous state. On failure, it returns an error and preserves the document; part of the older history may have been freed to attempt the allocation. Imports build a separate state and replace the image only after validation and a complete read. The engine alone cannot guarantee atomic SD writes; the application uses the OS file transaction.

## Transactions

- `ie_begin` creates an undo step for drawing primitives executed afterwards.
- `ie_gesture_begin` opens a cancellable gesture; `ie_commit` commits it and `ie_cancel` restores its previous state. Earlier redo states are preserved until commit, unless evicted under memory pressure.
- Layer operations, adjustments, geometry and movement create their own undo steps.
- The callback set by `ie_set_yield` must only give time to the scheduler. It must not modify the document or call the interface. A started operation completes a consistent state before processing the next cancellation/close command.

## Tools and formats

The round or square brush has diameter, opacity, edge hardness/contrast and point scatter. The eraser changes alpha. The engine provides lines, filled and/or outlined rectangles/ellipses, rectangular selection, movement, independent scaling on each axis, horizontal shear and quarter-turn rotations. Resizing and distortions use nearest-neighbour sampling. Brightness, contrast, saturation and red/green/blue gains apply to the selection or image, on the active layer or all visible layers.

Relief fill paints the supplied foreground color and shades it according to the selected profile and lighting, using normal alpha compositing. It also works on a transparent layer. The built-in interface supplies color 1 for a left-button/touch fill and color 2 for a right-button fill.

Light color is independent of both paint colors. `relief_intensity` ranges from 0 to 200 percent, with 100 percent as the interface default. `relief_gloss` ranges from 0 (matte) to 100 (glossy), adding a stronger, narrower directional highlight. `relief_light` supplies the light RGB; a zero alpha value selects white for compatibility with zero-initialized options. White light and zero gloss preserve the earlier matte shading. Zero intensity or zero height produces a flat fill in the selected paint color, regardless of light color or gloss; alpha, selection coverage and fill opacity retain their normal behavior. The interface's separate light palette and RGB controls change the light without changing colors 1 and 2.

Text uses an original 5 × 7 bitmap font, enlarged by an integer factor. Latin letters, digits, punctuation, French accents and cedilla are available; unsupported characters are replaced with `?`. Layers are raster images: drawn text is not a separate typographic object.

- **BMP**: uncompressed 24-bit or 32-bit `BI_RGB` import, top-down or bottom-up orientation; the fourth `BI_RGB` byte is reserved and is not interpreted as alpha. Export is 24-bit, flattened onto white. Compressed BMPs and BMPs with color masks are rejected.
- **T5I**: project format preserving layers, their order, names, alpha, opacity, visibility and the active layer. History and selection are not saved. The 32-byte header contains magic `T5IMG001`, width, height, layer count, active layer, reserved zero flags and a 32-bit FNV-1a checksum. Integers are little-endian. Each layer has 28 bytes of metadata (opacity, visibility, two reserved bytes and a 24-byte name), followed by width × height RGBA pixels. The checksum covers the first 28 header bytes, metadata and pixels; it detects ordinary corruption and does not provide authentication.

Read/write callbacks accept partial transfers, and each request is limited to 4096 bytes.

## Verification

Run `python3 run_tests.py`. The program compiles the production engine with AddressSanitizer and UndefinedBehaviorSanitizer, then checks tools, layers, undo/redo, gesture cancellation after undo, codecs and corrupted inputs, 336 allocation-failure scenarios and 400 editing cycles. The separate relief suite checks selected colors, transparency, light color, gloss, flat fills and 48 additional allocation-failure scenarios. Every scenario must release all of its allocations. The report links results to source SHA-256 hashes.

These computer tests do not replace checking the display, touch input, SD and ELF loader on the ESP32-P4 tablet.
