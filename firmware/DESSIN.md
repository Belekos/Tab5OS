# Drawing

Open **Drawing** from the home screen, or choose **Drawing** in **Open with...** for an image in Explorer. T5I projects open directly in the editor. The application uses the OS theme, font, focus and input system.

## Tools and colors

The left rail contains the tools; the selected icon is highlighted. The settings panel starts closed; opening it overlays the image without changing its zoom or framing. The top menus group file operations, history, view, image adjustments and layers. The two toolbar colors represent foreground and background: the left mouse button uses color 1 and the right mouse button uses color 2. In the color palette, left-click a swatch to set color 1 and right-click it to set color 2. Touch and keyboard selection update the color slot selected in the panel. Swap remains available in Colors and with X.

| Tool | Purpose |
| --- | --- |
| Brush / eraser | Paint or erase with size, opacity, hardness and scatter controls. Initial size: 2 px. |
| Selection | Rectangle, ellipse, freehand lasso, contiguous magic wand and color range. |
| Fill | Solid color, gradients, patterns and relief, over a contiguous area or the entire selection. |
| Move | Move the selected pixels. |
| Rectangle / ellipse / line | Drag to preview the shape, then release to apply it. Initial thickness: 2 px. |
| Text | Click in the image and type at that position. Enter applies; Escape cancels. |
| Eyedropper | Sample a color from the image. |
| Hand | Move the view without changing pixels. |
| Local deformation | Push, expand or pinch locally with a brush. |

Opacity limits each gesture's contribution: painting over the same area without releasing does not gradually make the stroke opaque. Release and start a new stroke to add another contribution. Hardness controls the edge; scatter disperses the points. Brush shapes include round, square, horizontal bar, vertical bar and two diagonals.

## Selections and fills

Choose **Selection**, open its options and select a method. The lasso follows the mouse or finger gesture; at least three points are required. The magic wand works on a contiguous area, while color range finds similar pixels. Tolerance sets the accepted color difference. Sampling can use visible layers.

Replace, add, subtract and intersect modes combine the mask. Invert and feather commands act on the current selection. **Feathering** ranges from -32 to +32 document pixels: a negative value softens only the inside, zero keeps a sharp edge, and a positive value softens both sides of the boundary. A shape narrower than the inward feathering width may become faint or disappear. Filling the entire selection also applies this setting while staying within the active mask. After deselecting, the first gesture creates the chosen shape even if Add, Subtract or Intersect is still selected; inversion remains an explicit command. The mask stores opacity levels rather than an all-or-nothing boundary; the relevant tools respect that selection.

With **Fill**, choose the area and type: solid color, linear or radial gradient, hatching, checkerboard or dots. Density, direction and pattern size adjust the result. **Stop at edges** and its threshold limit propagation across detected edges. Long calculations can be canceled; a failed allocation leaves the image intact.

## Relief

The **Relief** fill mode paints with the selected color and shades it to simulate a lit surface. Left-click uses color 1; right-click uses color 2. It also works on transparent areas of the active layer, respecting the fill opacity and selection. It produces a 2D image, not a 3D mesh.

- **Conical**: a linear profile with a constant slope toward the inside of the boundary.
- **Spherical**: a rounded circular profile whose amplitude depends on Height.
- **Extrusion**: constant height within the area, with a height change at the edge.

**Progression** sets the profile width, in document pixels or as a proportion of the maximum distance to the boundary. At 100%, the profile reaches the points furthest from the edge without imposing a plateau; a lower value intentionally leaves a flat area. Holes form part of the boundary. The proportion uses the maximum for the entire area: it does not equalize the height of all narrow branches in a shape. Extrusion does not use this setting.

A positive **Height** creates a raised surface and a negative value creates a recess. **Azimuth** controls the light direction from 0 to 359°: 0° right, 90° down, 180° left, 270° up. **Intensity** controls lighting contrast. Zero intensity or zero height produces a flat fill in the selected color. These controls remain simulation parameters for a 2D image.

The gear opens the tool options; this command is no longer duplicated in View. Command menus do not repeat their title in the list or keep a dummy row selected. The Help menu also includes **Drawing: selection and relief**.

In **Edit**, copy, cut, paste and delete act on the selection. Ctrl+C/X/V and Delete are available outside text entry. Arrow keys move the selection by one pixel; Shift+arrows move it by ten pixels. Successive moves restore the background beneath them; the clipboard remains available when using New or Open in the same Drawing session.

## Navigation and shapes

The mouse wheel zooms around the pointer. Hand or the middle mouse button moves the view without adding a history state. Fit and 100% are available in View. Right-click uses the second color for drawing tools without opening a context menu. Escape cancels the current gesture or closes the active dialog.

An ellipse is defined from two corners of its bounding box or from its center. The circle option enforces equal axes. Coordinates appear as soon as the pointer hovers over the image. During a gesture, the bottom of the screen also displays dimensions, length and angle as appropriate for the tool. The zoom percentage remains readable up to 1,600%.

Local deformation respects the active layer and selection. **Image → Transform** applies rotation, scaling or shearing to the active layer's selection, or to the entire image when nothing is selected. Undo and redo operate on document changes; moving the view does not create a history state.

## Files and limits

| Format | Opening | Saving |
| --- | --- | --- |
| T5I | Project with layers and transparency. | Working format; preserves layers and transparency. |
| BMP | Uncompressed 24/32-bit BMP. | Image flattened onto white. |
| JPG/JPEG | Standard or grayscale JPEG, original dimensions. | Composite flattened onto white, quality 92. |
| PNG | Image with transparency, original dimensions. | Save as T5I to preserve transparency. |
| WebP | Image with transparency; first frame of an animated file. | Save as T5I, BMP or JPEG. |

Imports preserve the original dimensions: **no automatic downscaling**, including JPEG and BMP. If the format, dimensions or available memory prevent opening, a message explains why and the current document remains intact. View zoom does not change document resolution. JPEG and PNG files are limited to 16 MiB, and WebP to 2 MiB. Progressive JPEG and automatic EXIF orientation are not supported.

The engine accepts up to 8,192 pixels on one side, within an 8 MiB pixel budget across all layers: the side limit therefore does not allow an 8,192-pixel square. There are at most eight layers; history is limited to sixteen states and 8 MiB, subject to available memory. The document, history, view and work areas share the native 18 MiB allowance. An import, layer or gesture may be refused if the required memory is unavailable; the image is not downscaled to work around that refusal.

Save explicitly, preferably as T5I to continue editing. Importing preserves the source file. Switching applications preserves the document and history; closing a modified image requires confirmation. Controls and text inserted into pixels use the OS font. Text size ranges from 25 to 400%; rendering is prepared before applying it, and a memory shortage preserves both the text and image.

Text is entered at the chosen image position and becomes a single history operation when applied. During text entry, letters type text instead of switching tools.

Ctrl+Z undoes, Ctrl+Y redoes, Ctrl+S saves, Ctrl+O opens and Ctrl+N creates a document. Enter and Escape work in dialogs; the keyboard can operate native controls.

Tool shortcuts: B brush, E eraser, S selection, V move pixels, T text, I eyedropper, H or Space Hand, L line, R rectangle, O ellipse, W deformation. X swaps colors. Ctrl+plus/minus zooms; Ctrl+0 fits the image and Ctrl+1 displays it at 100%.

## Default folder

New projects and exports use `/Picture`. Existing personal projects under `/Images` remain readable and can be saved in their current location; no automatic migration or renaming takes place.
