// Attacker-controlled text through the text stack with a fixed font: UTF-32 codepoints from the
// input go through UAX#14 line breaking, SheenBidi, harfbuzz shaping and BreakLines.
#include "rive/text/font_hb.hpp"
#include "rive/text/line_break.hpp"
#include "rive/text_engine.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

// IBM Plex Sans Arabic from upstream's test assets (Latin + Arabic), embedded at build time by
// mayhem/build.sh as kEmbeddedFont[] so the harness reads no file.
#include "embedded_font.h"

static rive::rcp<rive::Font> g_font;

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv)
{
  g_font = HBFont::Decode(
      rive::Span<const uint8_t>(kEmbeddedFont, sizeof(kEmbeddedFont)));
  if (!g_font)
  {
    fprintf(stderr, "fuzz_text_shape: could not load the embedded font\n");
    abort();
  }
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  using namespace rive;
  if (size < 2)
    return -1;
  const uint8_t ctl = data[0];
  data++;
  size--;
  std::vector<Unichar> text;
  for (size_t i = 0; i + 3 < size && text.size() < 1024; i += 3)
    text.push_back(((uint32_t)data[i] << 16 | (uint32_t)data[i + 1] << 8 | data[i + 2]) % 0x110000);
  if (text.empty())
    return -1;
  std::vector<LineBreak> breaks(text.size() + 1);
  computeLineBreaks(Span<const uint32_t>(text.data(), text.size()),
                    Span<LineBreak>(breaks.data(), breaks.size()));
  uint32_t first = (uint32_t)(text.size() / 2);
  TextRun runs[2] = {{g_font, 8.0f + (ctl & 0x1f), -1.0f, 0.0f, first, 0, 0, 0},
                     {g_font, 12.0f, -1.0f, (float)(ctl >> 6), (uint32_t)text.size() - first, 0, 1, 0}};
  int dir = (ctl & 0x20) ? 1 : -1;
  auto paras = g_font->shapeText(Span<const Unichar>(text.data(), text.size()),
                                 Span<const TextRun>(first ? runs : runs + 1, first ? 2 : 1),
                                 dir);
  float width = 20.0f + (ctl & 0x7f) * 4.0f;
  for (const auto& p : paras)
  {
    auto lines = GlyphLine::BreakLines(p.runs, width);
    GlyphLine::ComputeLineSpacing(true, lines, p.runs, width, TextAlign::center);
  }
  return 0;
}
