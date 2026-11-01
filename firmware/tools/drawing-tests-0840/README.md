# Drawing 0.8.40

The firmware editor and freestanding ELF example share `image_core.c` and its
`image_selection.h`. The built-in editor uses the OS palette, current font,
translation callback, focus navigation, mouse/touch events and `/Picture` paths.
The public editor hooks did not change.

## Selection and fill behavior

- Rectangle, antialiased ellipse, mouse/touch lasso, contiguous color wand and
  global color range. Replace/add/subtract/intersect, select all/invert/deselect.
- Feathering is a separable box filter with clamped image edges, radius 0–32
  document pixels. The existing selection can also be softened explicitly.
- Tolerance compares premultiplied RGBA channels (0–255); hidden RGB in a fully
  transparent pixel does not create a false boundary. Sampling can use the active
  layer or the composite of visible layers. The optional contour threshold
  limits the color difference across each neighboring step of a contiguous
  search. This is a local edge filter, not semantic object segmentation.
- Solid, linear (four directions), radial, checker, diagonal hatch and dot fills.
  Opacity, density and pattern scale are independent. Gradients and checker use
  foreground/background colors. A secondary mouse click reverses their roles.
  Linear gradients span the target bounds; radial gradients center on the click.
- Fill targets can be contiguous, all similar colors, or the current selection
  (the whole image when no selection is active). Existing selection alpha is
  always applied. Fill changes only the active layer, using source-over alpha.
- Brush, eraser, shapes, text, color adjustments, warp and selection movement also
  honor mask alpha. Empty selection and no selection are distinct.
- An outline is drawn into the preview only; it never touches document pixels
  and has no idle animation/timer. The outline marks nonzero mask coverage.
- Selection changes are transient; pixel-edit history captures their mask along
  with the layer state. T5I keeps its compatible layer format and does not persist
  the transient selection across closing/reopening a project.

## Memory and scheduling

All job, mask, queue and lasso buffers use the editor's admitted PSRAM allocator,
within its existing 18 MiB aggregate ceiling. `N = width × height`:

- Current selection: at most `N` bytes; plain rectangles need no mask.
- Working selection/fill mask: `N` bytes.
- Contiguous search queue: `4N` bytes, one queue entry per accepted pixel at most;
  released before painting. No recursive flood fill.
- Feathering: `N + 4 × width` temporary bytes; released before painting.
- Lasso: 2,048 points maximum; captured input is decimated when this limit is
  reached. Job copies are bounded, with in-place nonrecursive heapsort for edge
  crossings; no hidden libc allocator or new ELF import.
- History retains its explicit 8 MiB ceiling and includes selection masks in
  accounting. A mask or additional layer that would make the state too large for an undo
  snapshot is refused before installation;
  the current pixels and selection remain available. Deselecting can release
  the mask. Existing snapshot admission may evict old history to make room.

`service()` advances 4,096 work units per call. Rows are indivisible; lasso
crossings are charged to that budget so a narrow tall image cannot process
hundreds of complex polygon rows in one call. Generation and painting are
separate. Fill takes one undo snapshot only after its target mask is ready.
Escape/Cancel rolls back even a partially painted job, including prior dirty
state and retained redo entries. UI progress uses a modal cancel control; an
explicit app hide hides its modal while service can continue in background.
Closing/replacing the document while a job is active is blocked. Allocation
failures release temporary buffers and preserve current pixels/selection.

## Reproduce the host checks

```sh
python tools/drawing-tests-0840/run_core.py --build /tmp/drawing-core-0840
python tools/drawing-tests-0840/run.py \
  --lvgl /path/to/lvgl-9.6.0 \
  --lvgl-build /path/to/asan-lvgl-build \
  --m5gfx /path/to/M5GFX \
  --build /tmp/drawing-native-0840
```

The LVGL archive must use the firmware `lv_conf.h` and ASan/UBSan flags.
`run_core.py` executes the production C core plus the existing regression suite,
independent fixed-point flood and naive feather oracles, clipping/alpha/undo,
partial-job cancellation, allocation faults and lasso extremes.
`run.py` executes the production editor/core, codecs, focus and styling against
real LVGL. Hardware, reservation admission and storage boundaries are mocked.
It tests actual pointer/keyboard events, controls, 22/24 px OS fonts, light/dark
appearance, background lifecycle, OOM and complete allocation teardown.

Current reports and PNG captures are under `tools/verification/`, suffixed
`0840`, with production and test hashes. Historical reports were not relabeled.
The host harness uses documented portable copies of two external M5GFX files:
miniz's RISC-V byte-access path, and four JPEG signed negative shifts replaced
by multiplication by two. Firmware dependency files are unchanged. No sanitizer
is disabled on production editor/core code. LeakSanitizer cannot inspect
container threads; tracked PSRAM/model allocators must return to zero.

These checks are host evidence. They do not measure physical Tab5 FPS, PSRAM
bandwidth or hardware touch/mouse behavior.
