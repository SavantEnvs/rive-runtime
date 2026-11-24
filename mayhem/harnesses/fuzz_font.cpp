// Font decode (HBFont::Decode, rive's harfbuzz) + shaping / line breaking / glyph outlines over a
// fixed mixed-direction string, as a .riv text run would use an embedded font.
#include "rive/text/font_hb.hpp"
#include "rive/text_engine.hpp"
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  using namespace rive;
  auto font = HBFont::Decode(Span<const uint8_t>(data, size));
  if (!font)
    return 0;
  static const Unichar kText[] = {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', ' ',
                                  0x627, 0x644, 0x639, 0x631, 0x628, 0x64A, 0x629, ' ', 'f', 'i',
                                  '\n', 0x5E9, 0x5DC, 0x5D5, 0x5DD, ' ', '1', '2', '3'};
  const uint32_t n = sizeof(kText) / sizeof(kText[0]);
  TextRun runs[2] = {{font, 24.0f, -1.0f, 0.0f, 12, 0, 0, 0},
                     {font, 16.0f, -1.0f, 1.0f, n - 12, 0, 1, 0}};
  auto paras = font->shapeText(Span<const Unichar>(kText, n), Span<const TextRun>(runs, 2));
  for (const auto& p : paras)
  {
    auto lines = GlyphLine::BreakLines(p.runs, 100.0f);
    GlyphLine::ComputeLineSpacing(true, lines, p.runs, 100.0f, TextAlign::left);
  }
  for (GlyphID g = 0; g < 64; g++)
    font->getPath(g);
  for (uint16_t a = 0; a < font->getAxisCount() && a < 8; a++)
    font->getAxis(a);
  return 0;
}
