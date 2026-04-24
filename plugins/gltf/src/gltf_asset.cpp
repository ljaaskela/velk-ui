#include "gltf_asset.h"

namespace velk::ui::impl {

void GltfAsset::init(string_view uri)
{
    uri_ = string(uri);
    loaded_ = true;
}

void GltfAsset::init_failed(string_view uri)
{
    uri_ = string(uri);
    loaded_ = false;
}

IStore::Ptr GltfAsset::instantiate() const
{
    // Round 1 stub: real cgltf-backed element-tree construction lands
    // in round 2.
    return nullptr;
}

} // namespace velk::ui::impl
