# Drawing 0.8.41

The firmware editor and freestanding ELF example share `image_core.c` and its
`image_selection.h`. The built-in editor uses the OS palette, current font,
translation callback, focus navigation, mouse/touch events and `/Picture` paths.
The public editor hooks did not change.

## Corrections in 0.8.41

The unexpected inversion is reproduced against the byte-identical 0.8.40
selection implementation (its SHA-256 is checked against the historical report).
After deselection, keeping **Subtract** selected treated the absent mask as a
full-image selection: the first rectangle selected its exterior. The paint API
intentionally reports alpha 255 for no selection, but a new selection must not
use that value as an existing operand. Add/subtract/intersect now create the
first traced shape when no selection is enabled. With an existing selection,
including an explicitly empty one, their previous behavior is retained.
Inversion remains an explicit command. Clockwise/counterclockwise, concave and
crossed lassos plus all rectangle/ellipse drag directions are tested; these did
not reproduce an independent winding inversion.

The feather control now accepts **−32 to +32 document pixels**:

- Zero keeps a sharp edge, with the existing ellipse antialiasing.
- Positive values retain 0.8.40's centered box blur. Coverage can extend outside
  the original shape.
- Negative values soften only inward. They blur the original nonzero support,
  map its inner half to 0–255 coverage, then multiply by the original alpha.
  Outside pixels stay exactly zero; partially selected interiors are retained.
  Concavities, holes and thin areas follow the same rule. A shape narrower than
  the chosen inner feather can become faint or disappear.

Whole-selection fills now generate their working mask from the actual selection.
Previously the feather setting blurred an all-255 image before clipping it, so
it had no effect. The resulting fill remains clipped to the active selection,
without applying its existing alpha twice. Other fill targets still combine
region coverage with the active selection. The new signed operation uses the
same existing buffers and cooperative job stages; it adds no document-sized
allocation, no periodic timer and no persistent selection file format.

## Selection and fill behavior

- Rectangle, antialiased ellipse, mouse/touch lasso, contiguous color wand and
  global color range. Replace/add/subtract/intersect, select all/invert/deselect.
- Feathering uses a separable box filter with clamped image edges and a signed
  radius from −32 to +32 document pixels, as described above. The existing
  selection can also be softened explicitly.
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
python tools/drawing-tests-0841/run_core.py --build /tmp/drawing-core-0841
python tools/drawing-tests-0841/run.py \
  --lvgl /path/to/lvgl-9.6.0 \
  --lvgl-build /path/to/asan-lvgl-build \
  --m5gfx /path/to/M5GFX \
  --build /tmp/drawing-native-0841
```

The LVGL archive must use the firmware `lv_conf.h` and ASan/UBSan flags.
`run_core.py` executes the production C core plus the existing regression suite,
independent fixed-point flood and naive feather oracles, clipping/alpha/undo,
partial-job cancellation, 72 signed-job allocation fault cases and lasso extremes.
The 0.8.40 reproducer prints inside=0/outside=255; the fixed implementation must
print inside=255/outside=0. Logs and fixture provenance are included in the new
report. The expanded suite passes 157,059 checks in addition to the existing
34,777-check regression run.
`run.py` executes the production editor/core, codecs, focus and styling against
real LVGL. Hardware, reservation admission and storage boundaries are mocked.
It tests actual pointer/keyboard events, controls, 22/24 px OS fonts, light/dark
appearance, background lifecycle, OOM and complete allocation teardown. The
signed tests exercise the real dropdown/slider callbacks, deselect with Subtract
still active, mouse and touch in both directions for rectangle/ellipse/lasso,
explicit inversion, inward whole-selection fill, Undo, allocation refusal and
30 simulated 90-second idle intervals. The host run passes 7,492 checks, creates
38 captures and ends with zero tracked PSRAM bytes. The new control layouts were
visually inspected at both font sizes and in both themes.

Current reports and PNG captures are under `tools/verification/`, suffixed
`0841`, with production and test hashes. Historical reports were not relabeled.
The host harness uses documented portable copies of two external M5GFX files:
miniz's RISC-V byte-access path, and four JPEG signed negative shifts replaced
by multiplication by two. Firmware dependency files are unchanged. No sanitizer
is disabled on production editor/core code. LeakSanitizer cannot inspect
container threads; tracked PSRAM/model allocators must return to zero.

These checks are host evidence. They do not measure physical Tab5 FPS, PSRAM
bandwidth or hardware touch/mouse behavior.
