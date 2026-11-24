// .riv import (eager decoding) + play: rive::File::import is the runtime's entry point for
// untrusted .riv bytes (binary reader, importers, in-band fonts/audio, data binding).
#include "riv_play.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  static rive::NoOpFactory factory;
  rive::ImportResult res;
  auto file = rive::File::import(rive::Span<const uint8_t>(data, size), &factory, &res);
  if (!file)
    return 0;
  rive_fuzz_play(file.get());
  return 0;
}
