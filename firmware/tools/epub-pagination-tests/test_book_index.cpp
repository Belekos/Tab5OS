#include "EpubPageIndex.h"
#include <cassert>
#include <cstdio>
#include <cstdint>

int main() {
  epub_pages::Index<256> book;
  const uint32_t weights[] = {100, 900, 1000};
  book.reset(3, weights);
  auto s = book.status(1, 2, 450, 900);
  assert(!s.exact && !s.pageKnown && s.percent == 27);
  assert(book.nextUnknown() == 0);
  assert(book.record(0, 2));
  s = book.status(1, 2, 450, 900);
  assert(!s.exact && s.pageKnown && s.page == 5 && s.total == 0);
  book.record(1, 10); book.record(2, 8);
  s = book.status(1, 2, 450, 900);
  assert(s.exact && s.pageKnown && s.page == 5 && s.total == 20 && s.percent == 25);
  s = book.status(2, 7, 0, 1);
  assert(s.page == 20 && s.total == 20 && s.percent == 100);
  // Resume derives the book page from semantic chapter position + local page.
  s = book.status(2, 0, 0, 1);
  assert(s.page == 13 && s.percent == 65);
  // Different font / dimensions discard every old chapter count together.
  book.reset(3, weights);
  assert(!book.complete() && book.nextUnknown() == 0);
  book.record(0, 4); book.record(1, 20); book.record(2, 16);
  s = book.status(2, 0, 0, 1);
  assert(s.page == 25 && s.total == 40);
  assert(!book.record(3, 7) && !book.record(0, 0) && !book.record(0, 1000001));
  // Blank sections occupy the same single page that the reader displays.
  book.reset(2); book.record(0, 1); book.record(1, 1);
  s = book.status(1, 0, 0, 0);
  assert(s.exact && s.page == 2 && s.total == 2 && s.percent == 100);
  book.reset(0);
  assert(!book.complete() && !book.status(0, 0, 0, 0).exact);
  book.reset(257);
  assert(book.count() == 0);
  // Maliciously large ZIP weights must not wrap the weighted percentage.
  uint32_t huge[256]; for (auto& w : huge) w = UINT32_MAX;
  book.reset(256, huge);
  s = book.status(255, 0, UINT32_MAX, UINT32_MAX);
  assert(s.percent == 99 && !s.exact);
  s = book.status(128, 0, 0, 0);
  assert(s.percent == 50);
  // Geometry shared by real page and off-screen count: fixed / reflowable.
  auto g = epub_pages::geometry(720, 1280, 2.0f / 3.0f);
  assert(g.width == 720 && g.height == 1080 && g.x == 0 && g.y == 111);
  g = epub_pages::geometry(720, 1280, 0.5f);
  assert(g.width == 589 && g.height == 1178 && g.x == 65 && g.y == 62);
  auto fallback = epub_pages::geometry(720, 1280, -1.0f);
  assert(fallback.width == 720 && fallback.height == 1080);
  std::puts("PASS: EPUB global counts, resume prefix, font invalidation, bounds, overflow and shared geometry.");
}
