// mayhem/oracle.cpp — functional oracle for mayhem/test.sh (built by build.sh with upstream's normal
// release flags, no sanitizers). For each input it prints one line of facts the runtime decodes from
// upstream's own test assets; test.sh diffs every line against mayhem/oracle_expected.txt.
//
//   riv  <file> -> artboard count, and per artboard: name, size, animation/state machine counts,
//                  each first animation's duration/fps, and the bounds after advancing it
//   png/jpg/webp -> decoded width x height and a checksum of the RGBA pixels
//   ttf  <file> -> glyph count and total advance of a shaped string
#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text_engine.hpp"
#include "utils/no_op_factory.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace rive;

static std::vector<uint8_t> readFile(const char* path)
{
  std::vector<uint8_t> bytes;
  FILE* f = fopen(path, "rb");
  if (!f)
    return bytes;
  uint8_t buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0)
    bytes.insert(bytes.end(), buf, buf + n);
  fclose(f);
  return bytes;
}

static const char* base(const char* p)
{
  const char* s = strrchr(p, '/');
  return s ? s + 1 : p;
}

static void riv(const char* path, const std::vector<uint8_t>& b)
{
  NoOpFactory factory;
  ImportResult res;
  auto file = File::import(Span<const uint8_t>(b.data(), b.size()), &factory, &res);
  if (!file)
  {
    printf("riv %s import-failed %d\n", base(path), (int)res);
    return;
  }
  printf("riv %s artboards=%zu", base(path), file->artboardCount());
  for (size_t i = 0; i < file->artboardCount() && i < 3; i++)
  {
    auto ab = file->artboardAt(i);
    printf(" [%s %.0fx%.0f anims=%zu sms=%zu",
           ab->name().c_str(),
           ab->width(),
           ab->height(),
           ab->animationCount(),
           ab->stateMachineCount());
    if (ab->animationCount() > 0)
    {
      auto ai = ab->animationAt(0);
      printf(" a0=%s dur=%u fps=%u",
             ai->animation()->name().c_str(),
             ai->animation()->duration(),
             ai->animation()->fps());
      ai->advanceAndApply(0.25f);
      printf(" t=%.3f", ai->time());
    }
    auto bounds = ab->bounds();
    printf(" b=%.1f,%.1f,%.1f,%.1f]", bounds.left(), bounds.top(), bounds.right(), bounds.bottom());
  }
  printf("\n");
}

static void image(const char* path, const std::vector<uint8_t>& b)
{
  auto bmp = Bitmap::decode(b.data(), b.size());
  if (!bmp)
  {
    printf("image %s decode-failed\n", base(path));
    return;
  }
  bmp->pixelFormat(Bitmap::PixelFormat::RGBA);
  uint32_t sum = 0;
  size_t n = (size_t)bmp->width() * bmp->height() * 4;
  for (size_t i = 0; i < n; i++)
    sum = sum * 31 + bmp->bytes()[i];
  printf("image %s %ux%u sum=%08x\n", base(path), bmp->width(), bmp->height(), sum);
}

static void font(const char* path, const std::vector<uint8_t>& b)
{
  auto f = HBFont::Decode(Span<const uint8_t>(b.data(), b.size()));
  if (!f)
  {
    printf("font %s decode-failed\n", base(path));
    return;
  }
  const Unichar text[] = {'R', 'i', 'v', 'e', ' ', 'f', 'u', 'z', 'z', 'i', 'n', 'g'};
  TextRun run = {f, 32.0f, -1.0f, 0.0f, 12, 0, 0, 0};
  auto paras = f->shapeText(Span<const Unichar>(text, 12), Span<const TextRun>(&run, 1));
  size_t glyphs = 0;
  float advance = 0;
  for (const auto& p : paras)
    for (const auto& r : p.runs)
    {
      glyphs += r.glyphs.size();
      for (float a : r.advances)
        advance += a;
    }
  printf("font %s paras=%zu glyphs=%zu advance=%.2f axes=%u\n",
         base(path),
         paras.size(),
         glyphs,
         advance,
         (unsigned)f->getAxisCount());
}

int main(int argc, char** argv)
{
  for (int i = 1; i < argc; i++)
  {
    auto b = readFile(argv[i]);
    if (b.empty())
    {
      printf("missing %s\n", base(argv[i]));
      continue;
    }
    std::string p = argv[i];
    std::string ext = p.substr(p.find_last_of('.') + 1);
    if (ext == "riv")
      riv(argv[i], b);
    else if (ext == "png" || ext == "jpg" || ext == "webp")
      image(argv[i], b);
    else if (ext == "ttf")
      font(argv[i], b);
  }
  return 0;
}
