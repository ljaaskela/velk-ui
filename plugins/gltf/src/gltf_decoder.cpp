#include "gltf_decoder.h"

#include "gltf_asset.h"

#include <velk/ext/core_object.h>
#include <velk/interface/resource/intf_resource.h>

namespace velk::ui::impl {

IResource::Ptr GltfDecoder::decode(const IResource::Ptr& inner) const
{
    if (!inner) {
        return nullptr;
    }

    // Round 1 stub: report Failed state carrying the source URI so callers
    // see a coherent resource even before the parser is wired up. Round 2
    // calls cgltf here and populates shared meshes / materials / textures.
    auto obj = ::velk::ext::make_object<GltfAsset>();
    if (!obj) {
        return nullptr;
    }
    auto* asset = static_cast<GltfAsset*>(obj.get());
    asset->init_failed(inner->uri());
    return interface_pointer_cast<IResource>(obj);
}

} // namespace velk::ui::impl
