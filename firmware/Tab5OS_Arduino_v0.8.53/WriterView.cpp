#include "WriterView.h"
#include "WriterSyntax.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
namespace writing {
void RichView::begin(lv_obj_t* parent, bool readonly) {
  readOnly = readonly;
  object = lv_obj_create(parent);
  lv_obj_remove_style_all(object);
  lv_obj_set_width(object, LV_PCT(100));
  lv_obj_set_height(object, 40);
  lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(object,
                  static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
  lv_obj_add_flag(object, LV_OBJ_FLAG_USER_1);
  if (readonly) lv_obj_add_flag(object, LV_OBJ_FLAG_USER_2);
  lv_obj_add_event_cb(object, event, LV_EVENT_ALL, this);
  if (!readonly) blinkTimer = lv_timer_create(blink, 500, this);
}
void RichView::releaseImages() {
  for (auto& img : images)
    if (img.handle && imageFree) imageFree(img.handle);
  images.clear();
}
void RichView::discardLayout() {
  layoutSuspended = true;
  releaseImages();
  ExternalVector<Image>().swap(images);
  ExternalVector<Box>().swap(boxes);
  ExternalVector<const RichGlyph*>().swap(glyphs);
  ExternalVector<Row>().swap(rows);
  ExternalVector<Prefix>().swap(prefixes);
  ExternalVector<BlockBox>().swap(blocks);
  ExternalVector<Metric>().swap(widths);
  // The model, source mapping, caret, selection and typing attributes are
  // preserved. A hidden view can therefore coexist with autosave safely.
}
void RichView::resumeLayout() {
  if (!layoutSuspended) return;
  layoutSuspended = false;
  reflow(false);
}
void RichView::setTheme(const ViewTheme& value) {
  if (theme.foreground == value.foreground && theme.muted == value.muted &&
      theme.accent == value.accent && theme.surface == value.surface &&
      theme.selection == value.selection && theme.border == value.border)
    return;
  theme = value;
  if (sourceMode) recolorSyntax();
  if (object) lv_obj_invalidate(object);
}
void RichView::recolorSyntax() {
  if(sourceMode)colorSyntax(text,documentPath.c_str(),(theme.surface&255)+((theme.surface>>8)&255)+(theme.surface>>16)<384,syntaxReady?syntax:SyntaxLanguage::Plain,syntaxInitial);
  loadedSyntax=syntax;loadedSyntaxInitial=syntaxInitial;loadedSyntaxReady=syntaxReady;
  if(object)lv_obj_invalidate(object);
}
int RichView::glyphWidth(const lv_font_t* f, uint32_t code) {
  // Metrics are independent of the viewport and survive local edits. Repeated
  // letters no longer call TinyTTF twice each (word wrap + glyph placement).
  const size_t slot = ((reinterpret_cast<uintptr_t>(f) >> 4) ^ (code * 2654435761u)) & 255;
  if (widths.empty()) widths.resize(256);
  auto& entry = widths[slot];
  if (entry.font != f || entry.code != code) {
    entry = {f, code, std::max(1, int(lv_font_get_glyph_width(f, code, 0)))};
  }
  return entry.width;
}
RichGlyph RichView::glyphAt(size_t index) const {
  return index < glyphs.size() && glyphs[index] ? *glyphs[index] : RichGlyph{'\n'};
}
int RichView::imageFor(const std::string& relative) {
  std::string path = relative;
  for (size_t i = 0; i + 2 < path.size(); ++i)
    if (path[i] == '%') {
      auto hex = [](char c) -> int {
        return c >= '0' && c <= '9'   ? c - '0'
               : c >= 'A' && c <= 'F' ? c - 'A' + 10
               : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                      : -1;
      };
      int a = hex(path[i + 1]), b = hex(path[i + 2]);
      if (a >= 0 && b >= 0 && a * 16 + b) {
        path.replace(i, 3, 1, char(a * 16 + b));
      }
    }
  if (path.find("://") != std::string::npos) return -1;
  if (!path.empty() && path.front() != '/')
    path = documentPath.substr(0, documentPath.find_last_of('/') + 1) + path;
  for (size_t i = 0; i < images.size(); ++i)
    if (images[i].path == path) return images[i].handle ? i : -1;
  if (!imageLoad || images.size() >= 3) return -1;
  Image im;
  im.path = path;
  if (!imageLoad(path.c_str(), 600, 240, &im.dsc, &im.handle)) im.handle = nullptr;
  images.push_back(std::move(im));
  return images.back().handle ? images.size() - 1 : -1;
}
void RichView::invalidateCaret() {
  if (!object || caret >= boxes.size()) return;
  lv_area_t a;
  lv_obj_get_coords(object, &a);
  auto b = boxes[caret];
  lv_area_t r = {a.x1 + gutter + b.x, a.y1 + b.y, a.x1 + gutter + b.x + 2, a.y1 + b.y + b.h};
  lv_obj_invalidate_area(object, &r);
}
void RichView::blink(lv_timer_t* timer) {
  auto* self = static_cast<RichView*>(lv_timer_get_user_data(timer));
  if (!self->object || !self->editing || self->anchor != self->caret) return;
  for (auto* o = self->object; o; o = lv_obj_get_parent(o))
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
  self->blinkOn = !self->blinkOn;
  self->invalidateCaret();
}
void RichView::wakeCaret() {
  blinkOn = true;
  if (blinkTimer) lv_timer_reset(blinkTimer);
  invalidateCaret();
}
void RichView::load(const std::string& source, bool fenced) {
  layoutSuspended = false;
  const bool changed = !sourceCached || loadedFenced != fenced || loadedSourceMode != sourceMode || loadedSource != source ||
                       loadedPath != documentPath;
  if (changed) {
    releaseImages();
    text.parse(source, fenced, sourceMode);

  }
  if(sourceMode&&(changed||loadedSyntax!=syntax||loadedSyntaxReady!=syntaxReady||loadedSyntaxInitial!=syntaxInitial))recolorSyntax();
  caret = anchor = std::min(caret, text.size());
  typingStyle = text.styleAt(caret);
  auto color = text.glyph(caret ? caret - 1 : 0);
  typingForeground = color.foreground;
  typingBackground = color.background;
  reflow(changed);
  loadedSource = source;
  loadedPath = documentPath;
  loadedFenced = fenced;
  loadedSourceMode = sourceMode;
  sourceCached = true;
}
void RichView::reflow(bool contentsChanged) {
  if (!object || layingOut || layoutSuspended) return;
  layingOut = true;
  lv_obj_update_layout(object);
  int digits=1;for(size_t n=firstLine+text.lines.size();n>=10;n/=10)++digits;
  gutter=lineNumbers?10+digits*glyphWidth(font?font(18,0):&lv_font_montserrat_20,'0'):0;
  int width = std::max(32, static_cast<int>(lv_obj_get_width(object)) - 10-gutter);
  if (!contentsChanged && !boxes.empty() && width == layoutWidth && fontSize == layoutFontSize &&
      font == layoutFont) {
    layingOut = false;
    return;
  }
  if (contentsChanged) sourceCached = false;
  if (font != layoutFont) widths.clear();
  layoutWidth = width;
  layoutFontSize = fontSize;
  layoutFont = font;
  ++performance.layouts;
  boxes.clear();
  glyphs.clear();
  rows.clear();
  prefixes.clear();
  blocks.clear();
  const size_t count = text.size();
  boxes.reserve(count + 1);
  glyphs.reserve(count);
  performance.laidOutGlyphs = count;
  int y = 4;numbers.clear();size_t number=firstLine;
  for (auto& line : text.lines) {
    numbers.push_back({y,number});number+=std::max<size_t>(1,std::count(line.original.begin(),line.original.end(),'\n'));
    const size_t lineStart = glyphs.size();
    for (const auto& g : line.glyphs) glyphs.push_back(&g);
    if (!line.ending.empty()) glyphs.push_back(nullptr);
    uint16_t px = line.heading ? fontSize + 8 - (line.heading - 1) * 2 : fontSize;
    const lv_font_t* normal = font ? font(px, line.heading ? 1 : 0) : &lv_font_montserrat_24;
    int h = normal->line_height + 6, x = 4;
    if (line.block) {
      BlockBox block;
      block.index = boxes.size();
      block.y = y;
      block.w = width - 8;
      block.kind = line.block;
      if (line.block == 1) {
        int columns = line.cells.empty() ? 1 : line.cells[0].size();
        int cw = std::max(1, (width - 8) / columns);
        int rowY = y;
        const lv_font_t* cf = font ? font(20, 0) : normal;
        for (size_t r = 0; r < line.cells.size(); ++r) {
          int height = cf->line_height + 18;
          const size_t firstCell = block.cells.size();
          for (int c = 0; c < columns; ++c) {
            RichText content;
            content.parseCell(line.cells[r][c]);
            CellBox cell{};
            cell.header = r == 0;
            cell.align = static_cast<size_t>(c) < line.alignment.size() ? line.alignment[c] : 0;
            int cx = 0, cy = 0, ch = cf->line_height + 2;
            size_t rowStart = 0;
            auto alignRow = [&]() {
              int extra = std::max(0, cw - 16 - cx);
              int shift = cell.align == 3 ? extra / 2 : cell.align == 2 ? extra : 0;
              for (size_t i = rowStart; i < cell.glyphs.size(); ++i) cell.glyphs[i].box.x += shift;
              rowStart = cell.glyphs.size();
            };
            for (const auto& l : content.lines) {
              const uint16_t px = l.heading ? 28 - (l.heading - 1) * 2 : 20;
              for (const auto& g : l.glyphs) {
                const auto* f =
                    font ? font(px, (g.style & 3) | ((cell.header || l.heading) ? 1 : 0)) : cf;
                int w = glyphWidth(f, g.code);
                if (cx && cx + w > cw - 16) {
                  alignRow();
                  cx = 0;
                  cy += ch;
                  ch = cf->line_height + 2;
                }
                ch = std::max(ch, int(f->line_height) + 2);
                cell.glyphs.push_back({g, {cx, cy, w, ch, f}});
                cx += w;
              }
            }
            alignRow();
            height = std::max(height, cy + ch + 16);
            cell.area = {4 + c * cw, rowY, 4 + (c + 1) * cw - 1, rowY};
            block.cells.push_back(std::move(cell));
          }
          for (size_t c = firstCell; c < block.cells.size(); ++c)
            block.cells[c].area.y2 = rowY + height - 1;
          rowY += height;
        }
        block.h = rowY - y;
      } else {
        block.image = imageFor(line.imagePath);
        block.caption = line.imageAlt;
        if (block.image >= 0) {
          auto& im = images[block.image];
          block.w = std::min<int>(width - 8, im.dsc.header.w);
          block.h = im.dsc.header.h * block.w / im.dsc.header.w;
        } else {
          block.h = 64;
          block.caption = line.imageAlt + " — " +
                          (translate ? translate("Aperçu indisponible") : "Aperçu indisponible");
        }
      }
      boxes.push_back({4, y, block.w, block.h, normal});
      // End-of-block caret belongs below the object, not over the picture/table.
      boxes.push_back({4, y + block.h + 4, 2, h, normal});
      y += block.h + h + 12;
      blocks.push_back(std::move(block));
      continue;
    }
    if (!line.heading && !line.prefix.empty()) {
      x = 22;
      prefixes.push_back(
          {lineStart, line.prefix == "> " ? "│"
                      : line.prefix == "- " || line.prefix == "* " || line.prefix == "+ "
                          ? "•"
                          : line.prefix});
    }
    const lv_font_t* fonts[4]{};
    auto fontForStyle = [&](uint8_t style) {
      style &= 3;
      if (!fonts[style]) fonts[style] = font ? font(px, style | (line.heading ? 1 : 0)) : normal;
      return fonts[style];
    };
    for (size_t i = 0; i < line.glyphs.size(); ++i) {
      const auto& g = line.glyphs[i];
      const lv_font_t* f = fontForStyle(g.style);
      int w = g.code == '\t' ? fontSize * 2 : glyphWidth(f, g.code);
      if (w < 1) w = 1;
      // Wrap at word boundaries, falling back to character wrapping for a word
      // wider than the viewport. Layout and hit-testing share these positions.
      if (g.code != ' ' && g.code != '\t' &&
          (i == 0 || line.glyphs[i - 1].code == ' ' || line.glyphs[i - 1].code == '\t')) {
        int word = 0;
        for (size_t j = i; j < line.glyphs.size(); ++j) {
          auto& n = line.glyphs[j];
          if (n.code == ' ' || n.code == '\t') break;
          word += glyphWidth(fontForStyle(n.style), n.code);
          if (word > width) break;
        }
        if (x > 22 && word <= width - 8 && x + word > width) {
          x = 4;
          y += h;
        }
      }
      if (x + w > width && x > 4) {
        x = 4;
        y += h;
      }
      boxes.push_back({x, y, w, h, f});
      x += w;
    }
    if (!line.ending.empty())
      boxes.push_back({x, y, std::max(8, fontSize / 2), h, normal});
    else
      boxes.push_back({x, y, 2, h, normal});
    y += h + (line.heading ? 8 : 4);
  }
  // Only the final unterminated line owns an extra insertion position.
  if (boxes.empty()) boxes.push_back({4, 4, 2, 30, &lv_font_montserrat_24});
  for (size_t first = 0; first < glyphs.size() && first < boxes.size();) {
    size_t last = first + 1;
    int h = boxes[first].h;
    while (last < glyphs.size() && last < boxes.size() && boxes[last].y == boxes[first].y) {
      h = std::max(h, boxes[last].h);
      ++last;
    }
    rows.push_back({first, last, boxes[first].y, h});
    first = last;
  }
  if (lv_obj_get_height(object) != std::max(40, y)) lv_obj_set_height(object, std::max(40, y));
  layingOut = false;
  lv_obj_invalidate(object);
}
size_t RichView::hit(int x, int y) const {
  x-=gutter;
  if (boxes.empty()) return 0;
  size_t best = 0;
  int distance = 0x7fffffff;
  for (size_t i = 0; i < boxes.size(); ++i) {
    auto& b = boxes[i];
    int dy = y < b.y ? b.y - y : y > b.y + b.h ? y - b.y - b.h : 0;
    int dx = std::abs(x - b.x);
    int d = dy * 4096 + dx;
    if (d < distance) {
      distance = d;
      best = i;
    }
    if (y >= b.y && y < b.y + b.h && x >= b.x && x < b.x + b.w)
      return std::min(text.size(), i + (x - b.x > b.w / 2 && glyphAt(i).code != '\n' ? 1 : 0));
  }
  return std::min(best, text.size());
}
void RichView::draw(lv_layer_t* layer) {
  lv_area_t area;
  lv_obj_get_coords(object, &area);
  if(gutter){
    lv_area_t r={area.x1,area.y1,area.x1+gutter-2,area.y2};lv_draw_rect_dsc_t bg;lv_draw_rect_dsc_init(&bg);
    bg.bg_color=lv_color_mix(lv_color_hex(theme.muted),lv_color_hex(theme.surface),32);bg.bg_opa=LV_OPA_COVER;lv_draw_rect(layer,&bg,&r);
    for(const auto& n:numbers){if(area.y1+n.y+24<layer->_clip_area.y1||area.y1+n.y>layer->_clip_area.y2)continue;char value[24];snprintf(value,sizeof(value),"%u",unsigned(n.value));lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.font=font?font(18,0):&lv_font_montserrat_20;d.color=lv_color_hex(theme.muted);d.text=value;d.text_local=1;d.align=LV_TEXT_ALIGN_RIGHT;lv_area_t box={area.x1+2,area.y1+n.y,area.x1+gutter-6,area.y1+n.y+23};lv_draw_label(layer,&d,&box);}
    area.x1+=gutter;
  }
  const size_t a = std::min(anchor, caret), b = std::max(anchor, caret);
  performance.lastDrawGlyphs = performance.lastDrawRuns = 0;
  auto visible = [&](const lv_area_t& r) {
    return r.y2 >= layer->_clip_area.y1 && r.y1 <= layer->_clip_area.y2 &&
           r.x2 >= layer->_clip_area.x1 && r.x1 <= layer->_clip_area.x2;
  };
  for (const auto& prefix : prefixes) {
    if (prefix.index >= boxes.size()) continue;
    const auto& box = boxes[prefix.index];
    lv_area_t r = {area.x1, area.y1 + box.y, area.x1 + 24, area.y1 + box.y + box.h - 1};
    if (!visible(r)) continue;
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = box.font;
    d.color = lv_color_hex(theme.accent);
    d.text = prefix.text.c_str();
    d.text_local = 1;
    lv_draw_label(layer, &d, &r);
  }
  for (auto& block : blocks) {
    if (area.y1 + block.y + block.h < layer->_clip_area.y1 ||
        area.y1 + block.y > layer->_clip_area.y2)
      continue;
    for (auto& cell : block.cells) {
      lv_area_t r = {area.x1 + cell.area.x1, area.y1 + cell.area.y1, area.x1 + cell.area.x2,
                     area.y1 + cell.area.y2};
      if (!visible(r)) continue;
      lv_draw_rect_dsc_t bg;
      lv_draw_rect_dsc_init(&bg);
      bg.bg_color = cell.header
                        ? lv_color_mix(lv_color_hex(theme.accent), lv_color_hex(theme.surface), 32)
                        : lv_color_hex(theme.surface);
      bg.bg_opa = LV_OPA_COVER;
      bg.border_color = lv_color_hex(theme.border);
      bg.border_width = 1;
      lv_draw_rect(layer, &bg, &r);
      const int top = layer->_clip_area.y1 - r.y1 - 8;
      auto firstGlyph = std::lower_bound(
          cell.glyphs.begin(), cell.glyphs.end(), top,
          [](const CellBox::GlyphBox& g, int top) { return g.box.y + g.box.h <= top; });
      for (auto it = firstGlyph; it != cell.glyphs.end();) {
        const auto& g = *it;
        if (r.y1 + 8 + g.box.y > layer->_clip_area.y2) break;
        lv_area_t box = {r.x1 + 8 + g.box.x, r.y1 + 8 + g.box.y, r.x1 + 7 + g.box.x + g.box.w,
                         r.y1 + 7 + g.box.y + g.box.h};
        if (box.y2 < layer->_clip_area.y1 || box.y1 > layer->_clip_area.y2 ||
            box.x2 < layer->_clip_area.x1 || box.x1 > layer->_clip_area.x2) {
          ++it;
          continue;
        }
        auto last = it + 1;
        std::string value = RichText::utf8(g.glyph.code);
        int right = g.box.x + g.box.w;
        while (last != cell.glyphs.end() && last->box.y == g.box.y &&
               last->box.font == g.box.font && last->box.x == right &&
               last->glyph.style == g.glyph.style && last->glyph.foreground == g.glyph.foreground &&
               last->glyph.background == g.glyph.background && last->glyph.link == g.glyph.link &&
               r.x1 + 8 + last->box.x <= layer->_clip_area.x2) {
          value += RichText::utf8(last->glyph.code);
          right = last->box.x + last->box.w;
          ++last;
        }
        box.x2 = r.x1 + 7 + right;
        if (g.glyph.background != NoColor) {
          lv_draw_rect_dsc_t mark;
          lv_draw_rect_dsc_init(&mark);
          mark.bg_color = lv_color_hex(g.glyph.background);
          mark.bg_opa = LV_OPA_COVER;
          lv_draw_rect(layer, &mark, &box);
        }
        lv_draw_label_dsc_t d;
        lv_draw_label_dsc_init(&d);
        d.font = g.box.font;
        d.color =
            lv_color_hex(g.glyph.foreground == NoColor ? theme.foreground : g.glyph.foreground);
        d.decor=static_cast<lv_text_decor_t>(((g.glyph.style&8)?LV_TEXT_DECOR_UNDERLINE:0)|((g.glyph.style&16)?LV_TEXT_DECOR_STRIKETHROUGH:0));
        d.text = value.c_str();
        d.text_local = 1;
        lv_draw_label(layer, &d, &box);
        performance.lastDrawGlyphs += last - it;
        ++performance.lastDrawRuns;
        it = last;
      }
    }
    if (block.kind == 2) {
      lv_area_t r = {area.x1 + 4, area.y1 + block.y, area.x1 + 3 + block.w,
                     area.y1 + block.y + block.h - 1};
      if (block.image >= 0) {
        auto& im = images[block.image];
        lv_draw_image_dsc_t d;
        lv_draw_image_dsc_init(&d);
        d.src = &im.dsc;
        d.scale_x = d.scale_y = block.w * 256 / im.dsc.header.w;
        d.pivot = {0, 0};
        lv_draw_image(layer, &d, &r);
      } else {
        lv_draw_label_dsc_t d;
        lv_draw_label_dsc_init(&d);
        d.font = font ? font(20, 0) : &lv_font_montserrat_20;
        d.color = lv_color_hex(theme.muted);
        d.text = block.caption.c_str();
        d.text_local = 1;
        lv_draw_label(layer, &d, &r);
      }
    }
    if (block.index >= a && block.index < b) {
      lv_area_t r = {area.x1 + 4, area.y1 + block.y, area.x1 + 3 + block.w,
                     area.y1 + block.y + block.h - 1};
      lv_draw_rect_dsc_t d;
      lv_draw_rect_dsc_init(&d);
      d.bg_opa = LV_OPA_TRANSP;
      d.border_color = lv_color_hex(theme.accent);
      d.border_width = 3;
      lv_draw_rect(layer, &d, &r);
    }
  }
  // The row index is ordered by Y. A caret blink usually intersects only one
  // row and one glyph; scrolling draws only exposed rows, in either pane.
  const int clipTop = layer->_clip_area.y1 - area.y1;
  auto row = std::lower_bound(rows.begin(), rows.end(), clipTop,
                              [](const Row& row, int top) { return row.y + row.h <= top; });
  for (; row != rows.end() && area.y1 + row->y <= layer->_clip_area.y2; ++row) {
    size_t i = row->first;
    while (i < row->last && area.x1 + boxes[i].x + boxes[i].w + 4 < layer->_clip_area.x1) ++i;
    while (i < row->last && area.x1 + boxes[i].x <= layer->_clip_area.x2) {
      const auto& box = boxes[i];
      const auto g = glyphAt(i);
      if (g.code == 0xfffc) {
        ++i;
        continue;
      }
      const bool selected = i >= a && i < b;
      size_t j = i + 1;
      std::string s = g.code == '\n' ? " " : g.code == '\t' ? "    " : RichText::utf8(g.code);
      int right = box.x + box.w;
      while (j < row->last && area.x1 + boxes[j].x <= layer->_clip_area.x2) {
        const auto next = glyphAt(j);
        const auto& bx = boxes[j];
        if (bx.font != box.font || ((j >= a && j < b) != selected) ||
            next.style != g.style || next.foreground != g.foreground || next.background != g.background ||
            next.code == 0xfffc || next.link != g.link || next.code == '\n' || g.code == '\n')
          break;
        s += next.code == '\t' ? "    " : RichText::utf8(next.code);
        right = bx.x + bx.w;
        ++j;
      }
      lv_area_t r = {area.x1 + box.x, area.y1 + box.y, area.x1 + right - 1,
                     area.y1 + box.y + box.h - 1};
      if (selected || g.background != NoColor) {
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_hex(selected ? theme.selection : g.background);
        d.bg_opa = LV_OPA_COVER;
        lv_draw_rect(layer, &d, &r);
      }
      lv_draw_label_dsc_t d;
      lv_draw_label_dsc_init(&d);
      d.font = box.font;
      d.color = lv_color_hex(g.foreground != NoColor ? g.foreground
                             : g.link                ? theme.accent
                                                     : theme.foreground);
      d.text = s.c_str();
      d.text_local = 1;
      d.decor=static_cast<lv_text_decor_t>(((g.link||(g.style&8))?LV_TEXT_DECOR_UNDERLINE:0)|((g.style&16)?LV_TEXT_DECOR_STRIKETHROUGH:0));
      r.x2 += 4;
      lv_draw_label(layer, &d, &r);
      performance.lastDrawGlyphs += j - i;
      ++performance.lastDrawRuns;
      i = j;
    }
  }
  if (editing && blinkOn && anchor == caret && caret < boxes.size()) {
    auto& b = boxes[caret];
    lv_area_t r = {area.x1 + b.x, area.y1 + b.y, area.x1 + b.x + 1, area.y1 + b.y + b.h - 5};
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(theme.accent);
    d.bg_opa = LV_OPA_COVER;
    lv_draw_rect(layer, &d, &r);
  }
}
void RichView::event(lv_event_t* e) {
  auto* self = static_cast<RichView*>(lv_event_get_user_data(e));
  auto code = lv_event_get_code(e);
  if (code == LV_EVENT_DELETE) {
    if (self->blinkTimer) lv_timer_delete(self->blinkTimer);
    self->blinkTimer = nullptr;
    self->releaseImages();
    self->object = nullptr;
    return;
  }
  if (code == LV_EVENT_KEY && self->readOnly) {
    uint32_t key = lv_event_get_key(e);
    auto* p = lv_obj_get_parent(self->object);
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
      if (lv_obj_get_scroll_bottom(p) <= 1 && self->edge)
        self->edge(self->context, 1);
      else
        lv_obj_scroll_by(p, 0, key == LV_KEY_NEXT ? -300 : -60, LV_ANIM_OFF);
    }
    if (key == LV_KEY_UP || key == LV_KEY_PREV)
      lv_obj_scroll_by(p, 0, key == LV_KEY_PREV ? 300 : 60, LV_ANIM_OFF);
    return;
  }
  if (code == LV_EVENT_DRAW_MAIN) {
    if (self->layoutSuspended) return;
    self->draw(lv_event_get_layer(e));
    return;
  }
  if (code == LV_EVENT_SIZE_CHANGED) {
    int w = lv_obj_get_width(self->object) - 10;
    if (w > 0 && w != self->layoutWidth) self->reflow(false);
    return;
  }
  lv_indev_t* indev = lv_indev_active();
  if (!indev) {
    if (code == LV_EVENT_CLICKED && !self->readOnly) {
      self->editing = true;
      self->wakeCaret();
      if (self->activated) self->activated(self->context);
      lv_obj_invalidate(self->object);
    }
    return;
  }
  lv_point_t point;
  lv_indev_get_point(indev, &point);
  lv_area_t a;
  lv_obj_get_coords(self->object, &a);
  auto* parent = lv_obj_get_parent(self->object);
  if (code == LV_EVENT_PRESSED) {
    self->pressPoint = point;
    self->pressIndex = self->hit(point.x - a.x1, point.y - a.y1);
    self->selecting = false;
    self->dragged = false;
    self->bottomAtPress = lv_obj_get_scroll_bottom(parent) <= 1;
  } else if (code == LV_EVENT_LONG_PRESSED && !self->readOnly) {
    self->selecting = true;
    self->editing = true;
    self->anchor = self->caret = self->pressIndex;
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_invalidate(self->object);
  } else if (code == LV_EVENT_PRESSING) {
    if (std::abs(point.x - self->pressPoint.x) > 8 || std::abs(point.y - self->pressPoint.y) > 8)
      self->dragged = true;
    if (self->selecting) {
      self->caret = self->hit(point.x - a.x1, point.y - a.y1);
      lv_area_t p;
      lv_obj_get_coords(parent, &p);
      if (point.y < p.y1 + 24) lv_obj_scroll_by(parent, 0, 12, LV_ANIM_OFF);
      if (point.y > p.y2 - 24) lv_obj_scroll_by(parent, 0, -12, LV_ANIM_OFF);
      lv_obj_invalidate(self->object);
    }
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    if (code == LV_EVENT_RELEASED) {
      if (!self->selecting && !self->dragged) {
        size_t at = self->hit(point.x - a.x1, point.y - a.y1);
        for (auto& block : self->blocks)
          if (point.y - a.y1 >= block.y && point.y - a.y1 < block.y + block.h) {
            self->caret = block.index;
            self->anchor = block.index + 1;
            lv_obj_invalidate(self->object);
            if (!self->readOnly && block.kind == 1 && self->blockActivated)
              self->blockActivated(self->context, block.index);
            else if (!self->readOnly && self->activated)
              self->activated(self->context);
            return;
          }
        auto g = self->text.glyph(std::min(at, self->text.size() ? self->text.size() - 1 : 0));
        if (g.link && g.link <= self->text.links.size() && self->linked)
          self->linked(self->context, self->text.links[g.link - 1].target.c_str());
        else if (!self->readOnly) {
          self->move(at);
          self->editing = true;
          if (self->activated) self->activated(self->context);
        }
      } else if (self->selecting) {
        if (self->caret == self->anchor) {
          while (self->anchor && self->glyphAt(self->anchor - 1).code > 32) --self->anchor;
          while (self->caret < self->glyphs.size() && self->glyphAt(self->caret).code > 32)
            ++self->caret;
        }
        self->typingStyle = self->text.styleAt(self->caret);
        if (self->activated) self->activated(self->context);
      } else if (self->bottomAtPress && point.y < self->pressPoint.y - 36 && self->edge)
        self->edge(self->context, 1);
    }
    self->selecting = false;
    lv_obj_invalidate(self->object);
  }
}
void RichView::commit() {
  wakeCaret();
  if (changed) changed(context);
  reflow();
  reveal();
}
void RichView::reveal() {
  if (!object || caret >= boxes.size()) return;
  auto* p = lv_obj_get_parent(object);
  int y = boxes[caret].y;
  int scroll = lv_obj_get_scroll_y(p), h = lv_obj_get_height(p) - 24;
  if (y < scroll)
    lv_obj_scroll_to_y(p, y, LV_ANIM_OFF);
  else if (y + boxes[caret].h > scroll + h)
    lv_obj_scroll_to_y(p, y + boxes[caret].h - h, LV_ANIM_OFF);
}
void RichView::insert(const char* value) {
  if (readOnly) return;
  std::string input = value;
  if (singleLine)
    for (char& c : input)
      if (c == '\n' || c == '\r') c = ' ';
  size_t a = std::min(anchor, caret), b = std::max(anchor, caret);
  caret = anchor = text.replace(a, b, input, typingStyle, typingForeground, typingBackground);
  editing = true;
  commit();
}
void RichView::color(uint32_t value, bool background) {
  if (sourceMode) return;
  if (readOnly) return;
  (background ? typingBackground : typingForeground) = value;
  if (anchor != caret) {
    text.color(std::min(anchor, caret), std::max(anchor, caret), value, background);
    commit();
  } else {
    editing = true;
    if (activated) activated(context);
    wakeCaret();
  }
}
void RichView::format(uint8_t bit) {
  if (sourceMode) return;
  if (readOnly) return;
  if (anchor != caret) {
    size_t a = std::min(anchor, caret), b = std::max(anchor, caret);
    bool on = !text.allStyle(a, b, bit);
    text.toggle(a, b, bit);
    typingStyle = on ? typingStyle | bit : typingStyle & ~bit;
    commit();
  } else {
    typingStyle ^= bit;
    if (activated) activated(context);
    lv_obj_invalidate(object);
  }
}
void RichView::heading() {
  if (sourceMode) return;
  text.heading(caret);
  commit();
}
void RichView::move(size_t p, bool shift) {
  wakeCaret();
  caret = std::min(p, text.size());
  if (!shift) anchor = caret;
  typingStyle = text.styleAt(caret);
  auto color = text.glyph(caret ? caret - 1 : 0);
  typingForeground = color.foreground;
  typingBackground = color.background;
  if (object) {
    lv_obj_invalidate(object);
    reveal();
  }
}
std::string RichView::selected() const {
  return text.plain(std::min(anchor, caret), std::max(anchor, caret));
}
void RichView::blur() {
  editing = false;
  lv_obj_invalidate(object);
}
void RichView::key(const KeyEvent& e) {
  using K = Key;
  if (readOnly) return;
  if (e.key == K::Enter && text.blockAt(caret) && blockActivated) {
    blockActivated(context, caret);
    return;
  }
  wakeCaret();
  if(e.key==K::Insert){overwrite=!overwrite;return;}
  if (e.key == K::Text && !e.ctrl && !e.alt && !e.meta){
    if(overwrite&&anchor==caret&&caret<text.size()&&glyphAt(caret).code!='\n')anchor=caret+1;
    insert(e.text);
  }
  else if (e.key == K::Enter)
    insert("\n");
  else if (e.key == K::Tab)
    insert("    ");
  else if (e.key == K::Backspace || e.key == K::Delete) {
    if (anchor == caret) {
      if (e.key == K::Backspace && caret)
        anchor = caret - 1;
      else if (e.key == K::Delete && caret < text.size())
        anchor = caret + 1;
    }
    if (anchor != caret) insert("");
  } else if (e.key == K::Left || e.key == K::Right) {
    size_t dest = caret;
    if (e.key == K::Left) {
      if (dest) --dest;
      if (e.ctrl)
        while (dest && glyphAt(dest - 1).code > 32) --dest;
    } else {
      if (dest < glyphs.size()) ++dest;
      if (e.ctrl)
        while (dest < glyphs.size() && glyphAt(dest).code > 32) ++dest;
    }
    move(dest, e.shift);
  } else if (e.key == K::Home) {
    size_t p = caret;
    while (p && glyphAt(p - 1).code != '\n') --p;
    move(p, e.shift);
  } else if (e.key == K::End) {
    size_t p = caret;
    while (p < glyphs.size() && glyphAt(p).code != '\n') ++p;
    move(p, e.shift);
  } else if ((e.key == K::Up || e.key == K::Down) && caret < boxes.size()) {
    auto b = boxes[caret];
    move(hit(b.x, b.y + (e.key == K::Up ? -b.h : b.h) + b.h / 2), e.shift);
  }
}
}  // namespace writing
