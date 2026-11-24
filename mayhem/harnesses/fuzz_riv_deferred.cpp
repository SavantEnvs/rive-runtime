// .riv import with FileImportOptions::deferDecoding: bodies are skimmed at import and decoded
// lazily on first use (artboard / animation / view model instance bodies), a separate code path.
#include "riv_play.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  static rive::NoOpFactory factory;
  rive::FileImportOptions options;
  options.deferDecoding = true;
  rive::ImportResult res;
  auto file = rive::File::import(rive::Span<const uint8_t>(data, size),
                                 &factory,
                                 &res,
                                 nullptr,
                                 nullptr,
                                 false,
                                 options);
  if (!file)
    return 0;
  // Touch every view model so deferred view model instance bodies decode too.
  for (size_t v = 0; v < file->viewModelCount() && v < 8; v++)
    file->createDefaultViewModelInstance(file->viewModel(v));
  rive_fuzz_play(file.get());
  return 0;
}
