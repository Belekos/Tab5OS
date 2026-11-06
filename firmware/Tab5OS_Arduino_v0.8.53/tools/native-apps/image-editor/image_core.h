#ifndef TAB5_IMAGE_CORE_H
#define TAB5_IMAGE_CORE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define IE_MAX_LAYERS 8
#define IE_MAX_DIMENSION 8192
#define IE_MAX_HISTORY 16
#define IE_DOCUMENT_MAX_BYTES (8u * 1024u * 1024u - 64u * 1024u)
#define IE_HISTORY_MAX_BYTES (8u * 1024u * 1024u)
/* Straight alpha RGBA: 0xAABBGGRR, independent of host byte order. */
#define IE_RGBA(r,g,b,a) ((uint32_t)(r)|((uint32_t)(g)<<8)|((uint32_t)(b)<<16)|((uint32_t)(a)<<24))
typedef void *(*IEAlloc)(void *, size_t);
typedef void (*IEFree)(void *, void *);
typedef size_t (*IERead)(void *, void *, size_t);
typedef size_t (*IEWrite)(void *, const void *, size_t);
typedef struct { int x, y, w, h; int enabled; } IERect;
typedef struct IESelectionPixels IESelectionPixels;
typedef struct IEFloatingSelection IEFloatingSelection;
typedef struct { uint32_t *pixels; uint8_t opacity, visible; char name[24]; } IELayer;
typedef struct {
    int width, height, count, active;
    IELayer layers[IE_MAX_LAYERS];
    IERect selection;
    /* Optional 8-bit selection coverage, owned by this state and its history. */
    uint8_t *selection_mask;
    /* Immutable content and covered background, shared with undo states. */
    IEFloatingSelection *floating;
    /* History only: bitmap stored after the reserved active-layer pixels.
       Current always owns complete layers and has no sparse bitmap. */
    uint8_t *gesture_blocks;
    int gesture_layer;
} IEState;
typedef struct {
    /* current owns every layer; internal gesture history may omit unchanged
       layer buffers. History pointers must only be read for the edited layer. */
    IEState current;
    /* Editor-local clipboard survives New/Open, independently of undo. */
    IESelectionPixels *clipboard;
    IEState *undo[IE_MAX_HISTORY], *redo[IE_MAX_HISTORY];
    unsigned undo_count, redo_count;
    IEState *saved_redo[IE_MAX_HISTORY]; unsigned saved_redo_count;
    int gesture;
    uint8_t *brush_coverage;
    uint32_t *warp_scratch;
    int scratch_side, scratch_layer;
    uint32_t brush_seed;
    size_t history_bytes;
    IEAlloc alloc; IEFree release; void *memory_context;
    uint32_t random;
    void (*yield)(void *); void *yield_context;
    const char *error;
    int dirty;
} IEDoc;
typedef struct {
    uint32_t color;
    int size;       /* diameter, 1..256 */
    int opacity;   /* 0..100 */
    int hardness;  /* 0 soft .. 100 hard; brush contrast */
    int diffusion; /* 0 solid .. 100 sparse spray */
    int square;    /* IE_BRUSH_* shape; retained field name for source compatibility */
    int erase;
} IEBrush;
enum { IE_SHAPE_LINE, IE_SHAPE_RECT, IE_SHAPE_ELLIPSE };
enum { IE_BRUSH_ROUND, IE_BRUSH_SQUARE, IE_BRUSH_HORIZONTAL, IE_BRUSH_DIAGONAL_UP, IE_BRUSH_DIAGONAL_DOWN, IE_BRUSH_VERTICAL };
enum { IE_WARP_PUSH, IE_WARP_INFLATE, IE_WARP_PINCH };

int ie_init(IEDoc *, IEAlloc, IEFree, void *, int width, int height);
/* Scheduler callback only: must not mutate this document or perform UI work. */
void ie_set_yield(IEDoc *, void (*callback)(void *), void *context);
void ie_destroy(IEDoc *);
int ie_new(IEDoc *, int width, int height, uint32_t background);
const char *ie_error(const IEDoc *);
/* Call once before an in-place gesture. A failed begin must not be followed by drawing. */
int ie_begin(IEDoc *);
/* Cancellable pointer gestures retain prior redo until commit when memory permits. */
int ie_gesture_begin(IEDoc *);
void ie_commit(IEDoc *);
int ie_cancel(IEDoc *); /* revert the latest gesture without adding redo */
int ie_undo(IEDoc *);
int ie_redo(IEDoc *);
void ie_select(IEDoc *, int x0, int y0, int x1, int y1);
void ie_deselect(IEDoc *);
/* Selection and fill jobs are stepped by the UI task. No document mutation is
   allowed until completion/cancellation. Each step processes bounded rows or
   flood nodes; all workspace is admitted before pixel changes. */
typedef struct { int x,y; } IEPoint;
enum { IE_SELECT_RECT, IE_SELECT_ELLIPSE, IE_SELECT_LASSO, IE_SELECT_CONTIGUOUS,
       IE_SELECT_COLOR, IE_SELECT_EXISTING, IE_SELECT_INVERT };
enum { IE_SELECT_REPLACE, IE_SELECT_ADD, IE_SELECT_SUBTRACT, IE_SELECT_INTERSECT };
typedef struct {
 /* feather -32..32 px: negative stays inside; positive retains the centered blur.
    Combine modes apply only when a selection is already enabled. */
 int method, combine, x0,y0,x1,y1, tolerance, feather, sample_merged, edge;
 const IEPoint *points; unsigned point_count;
} IESelectionOptions;
enum { IE_FILL_SOLID, IE_FILL_LINEAR, IE_FILL_RADIAL, IE_FILL_CHECKER, IE_FILL_HATCH, IE_FILL_DOTS, IE_FILL_RELIEF };
enum { IE_FILL_CONTIGUOUS, IE_FILL_MATCHING, IE_FILL_SELECTION };
enum { IE_RELIEF_LINEAR, IE_RELIEF_CIRCULAR, IE_RELIEF_EXTRUDE };
enum { IE_RELIEF_PIXELS, IE_RELIEF_PERCENT };
typedef struct {
 /* Signed feather uses the same convention as selection; fills always stay
    within the active selection and preserve its existing alpha once. */
 int kind, region, x,y, tolerance, feather, sample_merged, edge;
 int opacity, density, scale, direction;
 uint32_t foreground,background;
 /* Relief shades the selected foreground and composites it like other fills.
    Percent extent
    refers to the largest boundary distance across this region, not a local
    medial axis. Extrusion has constant height; its extent is unused.
    Azimuth is clockwise in image coordinates: 0 right, 90 down. */
 int relief_profile, relief_unit, relief_extent;
 int relief_height;    /* signed pixels, -256..256; zero gives a flat fill */
 int relief_azimuth;   /* degrees, wrapped to 0..359 */
 int relief_intensity; /* 0..200 percent; zero gives a flat fill */
} IEFillOptions;
typedef struct IEJob IEJob;
IEJob *ie_selection_start(IEDoc *, const IESelectionOptions *);
IEJob *ie_fill_start(IEDoc *, const IEFillOptions *);
int ie_job_step(IEJob *, size_t budget); /* 0 running, 1 finished, -1 failed */
unsigned ie_job_progress(const IEJob *);
void ie_job_cancel(IEJob *); /* restores a partially applied fill */
void ie_job_destroy(IEJob *); /* cancels if still running */
unsigned ie_selection_alpha(const IEState *,int x,int y);
int ie_selection_bounds(const IEState *, IERect *);
void ie_render_selection_outline(const IEDoc *,uint16_t *,size_t,int,int,int32_t,int32_t,int,int,int,int,int);
uint32_t ie_composite(const IEDoc *, int x, int y);
void ie_composite_row(const IEDoc *, int y, uint32_t *out);
/* Update only the clipped rectangle of a document-sized RGB565 display buffer.
   stride_pixels is the full destination row width (>= current.width); out must
   have stride_pixels * current.height elements. Colors are 0xRRGGBB. A fixed
   8-pixel document-space checker makes partial and full redraws identical.
   No allocation or document mutation. Returns the number of pixels written. */
size_t ie_render_rgb565(const IEDoc *, uint16_t *out, size_t stride_pixels,
                        int x, int y, int width, int height,
                        uint32_t checker_light, uint32_t checker_dark);
/* The same update in a viewport-sized buffer; nearest-neighbour source is
   (display_x * document_width / out_width, display_y * document_height /
   out_height). The dirty rectangle remains in document coordinates. Exactly
   the display samples inside that rectangle are written. Both output
   dimensions must be 1..IE_MAX_DIMENSION, stride >= out_width; no allocation. */
size_t ie_render_scaled_rgb565(const IEDoc *, uint16_t *out, size_t stride_pixels,
                               int out_width, int out_height,
                               int x, int y, int width, int height,
                               uint32_t checker_light, uint32_t checker_dark);
/* origin16 is the document coordinate at the viewport origin, in signed 16.16.
   Dirty rectangles are in viewport coordinates; output dimensions <= 2048.
   Rendering performs no allocation and never changes the document. */
int ie_view_sample(int display_coordinate, int32_t origin16, int zoom_percent);
size_t ie_render_view_rgb565(const IEDoc *, uint16_t *, size_t stride_pixels,
                            int out_width, int out_height,
                            int32_t origin_x16, int32_t origin_y16, int zoom_percent,
                            int x, int y, int width, int height,
                            uint32_t checker_light, uint32_t checker_dark, uint32_t outside);
/* Reserve operation scratch BEFORE adding the undo snapshot. Failure preserves
   document pixels; an initial scratch refusal preserves history too. A gesture
   releases scratch on commit, cancellation, undo, replacement or destruction. */
int ie_brush_begin(IEDoc *);
int ie_warp_begin(IEDoc *, int maximum_size);
int ie_warp_brush(IEDoc *, int x0, int y0, int x1, int y1,
                  int size, int strength, int mode);
/* Legacy callers may use ie_begin first; brush then allocates its coverage mask
   before the first stamp. UI callers should prefer ie_brush_begin. */
int ie_brush(IEDoc *, int x0, int y0, int x1, int y1, const IEBrush *);
void ie_shape(IEDoc *, int shape, int x0, int y0, int x1, int y1,
              uint32_t color, int opacity, int thickness, int fill, int outline);
void ie_text(IEDoc *, int x, int y, const char *utf8, int scale, uint32_t color, int opacity);
/* Stamp a clipped A8 text/image mask into an already begun transaction. */
void ie_mask_stamp(IEDoc *, int x, int y, const uint8_t *mask, int width, int height,
                   int stride, uint32_t color, int opacity);
uint8_t ie_glyph_row(unsigned codepoint, int row); /* 5 bits, high bit at left */
/* These operations manage their own undo transaction. */
int ie_layer_add(IEDoc *, const char *name);
int ie_layer_duplicate(IEDoc *);
int ie_layer_delete(IEDoc *);
int ie_layer_move(IEDoc *, int direction);
int ie_layer_style(IEDoc *, int opacity, int visible);
int ie_layer_merge_down(IEDoc *);
int ie_clear(IEDoc *);
/* Adjust all visible layers when all_layers!=0, else active layer.
   Selection limits the adjustment. RGB are percentage gains, 0..200. */
int ie_adjust(IEDoc *, int brightness, int contrast, int saturation,
              int red, int green, int blue, int all_layers);
int ie_resize(IEDoc *, int width, int height);
int ie_crop(IEDoc *, int x, int y, int width, int height);
/* Affine deformation of active layer selection (whole image if none), nearest neighbour. */
int ie_transform(IEDoc *, int scale_x_percent, int scale_y_percent,
                 int shear_x_percent, int quarter_turns);
int ie_translate_selection(IEDoc *, int delta_x, int delta_y);
/* Each mutating operation owns its undo transaction. Copy preserves pixels.
   Paste coordinates are in the destination document. Clipboard ownership is
   independent of history and is released only on clear/destruction. */
int ie_selection_copy(IEDoc *);
int ie_selection_cut(IEDoc *);
int ie_selection_paste(IEDoc *, int x, int y);
int ie_selection_delete(IEDoc *);
int ie_clipboard_available(const IEDoc *);
int ie_clipboard_bounds(const IEDoc *, IERect *);
void ie_clipboard_clear(IEDoc *);
/* Used by transactional codecs replacing an entire IEDoc. Both documents must
   use the same allocator/context. Source relinquishes its clipboard. */
int ie_clipboard_transfer(IEDoc *destination, IEDoc *source);
/* Streaming codecs; readers start at byte 0. Imports replace the document only
   after full validation/read and reset history. T5I preserves layers/transparency.
   BMP import: uncompressed 24/32 bit; BMP export: flattened 24 bit on white. */
int ie_load_bmp(IEDoc *, IERead, void *);
int ie_save_bmp(const IEDoc *, IEWrite, void *);
int ie_load_project(IEDoc *, IERead, void *);
int ie_save_project(const IEDoc *, IEWrite, void *);
#ifdef __cplusplus
}
#endif
#endif
