// KTX2 container parser (decoders/src/decode_ktx2.cpp), with and without GPU-format support so
// both the pass-through and the software-decode branch run.
#include "rive/decoders/decode_ktx2.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  rive::Ktx2DecodeResult out;
  rive::DecodeKtx2(data, size, out);
  rive::Ktx2DecodeResult soft;
  rive::Ktx2HwSupport none;
  none.bc = none.astc = none.etc2 = false;
  rive::DecodeKtx2(data, size, soft, none);
  return 0;
}
