#include "rect_visual.h"

#include <velk/api/state.h>
#include <velk/hash.h>
#include <velk-ui/instance_types.h>

namespace velk::ui {

namespace {
constexpr uint64_t kPipelineKey = make_hash64("RectVisual");
}

vector<DrawEntry> RectVisual::get_draw_entries(const rect& bounds)
{
    auto state = read_state<IVisual>(this);
    if (!state) {
        return {};
    }

    DrawEntry entry{};
    entry.pipeline_key = kPipelineKey;
    entry.bounds = bounds;
    entry.set_instance(RectInstance{
        {bounds.x, bounds.y},
        {bounds.width, bounds.height},
        state->color});

    return {entry};
}

uint64_t RectVisual::get_pipeline_key() const
{
    return kPipelineKey;
}

} // namespace velk::ui
