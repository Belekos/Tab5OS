#ifndef TAB5_IMAGE_CORE_H
#define TAB5_IMAGE_CORE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define IE_MAX_LAYERS 8
#define IE_MAX_DIMENSION 1024
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
typedef struct { uint32_t *pixels; uint8_t opacity, visible; char name[24]; } IELayer;
typedef struct {
    int width, height, count, active;
    IELayer layers[IE_MAX_LAYERS];
    IERect selection;
} IEState;
typedef struct {
    IEState current;
    IEState *undo[IE_MAX_HISTORY], *redo[IE_MAX_HISTORY];
    unsigned undo_count, redo_count;
    IEState *saved_redo[IE_MAX_HISTORY]; unsigned saved_redo_count;
    int gesture;
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
    int square;
    int erase;
} IEBrush;
enum { IE_SHAPE_LINE, IE_SHAPE_RECT, IE_SHAPE_ELLIPSE };

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
   dimensions must be 1..1024, stride >= out_width; no allocation. */
size_t ie_render_scaled_rgb565(const IEDoc *, uint16_t *out, size_t stride_pixels,
                               int out_width, int out_height,
                               int x, int y, int width, int height,
                               uint32_t checker_light, uint32_t checker_dark);
/* These are in-place primitives: caller obtains one snapshot with ie_begin. */
void ie_brush(IEDoc *, int x0, int y0, int x1, int y1, const IEBrush *);
void ie_shape(IEDoc *, int shape, int x0, int y0, int x1, int y1,
              uint32_t color, int opacity, int thickness, int fill, int outline);
void ie_text(IEDoc *, int x, int y, const char *utf8, int scale, uint32_t color, int opacity);
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
