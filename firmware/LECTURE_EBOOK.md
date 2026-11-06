# EPUB reading

The bottom slider lets you choose a page anywhere in the book. While dragging it, the target page number and total page count temporarily replace the status. The displayed page changes only after release. The reader then prepares the destination progressively, keeping the current page visible during preparation.

With a keyboard, use Tab to focus the slider, Enter to adjust it, the arrow keys to choose a page and Enter to open it. Escape cancels the adjustment. Previous/next page controls continue to turn pages.

The page count depends on the text size and layout. On first opening a book, or after changing these settings, the reader calculates the book index in the background. The slider becomes available once the exact total is known; the “Laying out…” status and approximate progress remain visible meanwhile. A percentage prefixed with `~` is an estimate, never a page count presented as exact.

The index is saved to storage when complete and reused on later openings. Version 0.8.22 recalculates older indexes once, because the reading area now reserves space for the slider. Bookmarks based on section and text position remain compatible.

To reach a chapter that is not loaded, the reader reuses the indexer's working document; it does not load several background chapters at once. It lays out at most one page per service pass, then transfers the prepared document to the reader without parsing it a second time. A destination already paginated in the current chapter opens immediately. The destination replaces the bookmark only after rendering succeeds. If allocation, measurement or rendering fails, the previous document and bookmark are retained. If memory is also insufficient to redraw the previous page, its display may remain incomplete; its saved position remains intact.
