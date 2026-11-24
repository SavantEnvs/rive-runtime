// Audio decode (rive's miniaudio fork: WAV / MP3 / FLAC) as used for .riv audio assets.
#include "rive/audio/audio_source.hpp"
#include "rive/audio/audio_reader.hpp"
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  using namespace rive;
  std::vector<uint8_t> bytes(data, data + size);
  auto src = AudioSource::MakeAudioSource(Span<uint8_t>(bytes.data(), bytes.size()));
  if (!src)
    return 0;
  auto rd = src->makeReader(2, 44100);
  if (!rd)
    return 0;
  std::vector<float> out;
  rd->readAll(1 << 15, out);
  rd->seek(1000);
  out.clear();
  rd->readAll(1 << 12, out);
  return 0;
}
