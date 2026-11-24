// Image decoders (PNG / JPEG / WebP, sniffed by Bitmap::decode) used for .riv image assets.
#include "rive/decoders/bitmap_decoder.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  auto bmp = Bitmap::decode(data, size);
  if (bmp)
    bmp->pixelFormat(Bitmap::PixelFormat::RGBAPremul);
  return 0;
}
