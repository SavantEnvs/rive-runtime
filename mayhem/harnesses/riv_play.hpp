// Shared by fuzz_riv_import / fuzz_riv_deferred: instance and play what File::import produced, the
// way an embedding app does (artboard instance, default view model bound, state machines driven
// with pointer events, animations advanced, everything drawn into a no-op renderer).
#pragma once
#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"

static inline void rive_fuzz_play(rive::File* file)
{
  using namespace rive;
  NoOpRenderer renderer;
  for (size_t i = 0; i < file->artboardCount() && i < 4; i++)
  {
    auto ab = file->artboardAt(i);
    if (!ab)
      continue;
    auto vmi = file->createDefaultViewModelInstance(file->artboard(i));
    if (vmi)
      ab->bindViewModelInstance(vmi);
    ab->advance(0.0f);
    ab->draw(&renderer);
    for (size_t s = 0; s < ab->stateMachineCount() && s < 4; s++)
    {
      auto smi = ab->stateMachineAt(s);
      if (!smi)
        continue;
      if (vmi)
        smi->bindViewModelInstance(vmi);
      smi->pointerMove(Vec2D(10, 10));
      smi->pointerDown(Vec2D(10, 10));
      smi->pointerUp(Vec2D(10, 10));
      for (int k = 0; k < 4; k++)
        smi->advanceAndApply(0.1f);
      ab->draw(&renderer);
    }
    for (size_t a = 0; a < ab->animationCount() && a < 4; a++)
    {
      auto ai = ab->animationAt(a);
      if (!ai)
        continue;
      ai->advanceAndApply(0.1f);
      ai->advanceAndApply(0.5f);
      ab->draw(&renderer);
    }
  }
}
