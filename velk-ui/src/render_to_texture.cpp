#include "render_to_texture.h"

#include <velk/api/object_ref.h>
#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_metadata_observer.h>
#include <velk-render/plugin.h>

namespace velk::ui::impl {

RenderToTexture::RenderToTexture() = default;

void RenderToTexture::on_state_changed(string_view name, IMetadata&, Uid interfaceId)
{
    if (interfaceId != IRenderToTexture::UID) {
        return;
    }
    // texture_size changes are handled by the renderer when it
    // checks the trait's size against the current GPU texture dimensions.
}

} // namespace velk::ui::impl
