#ifndef VELK_UI_GLTF_ASSET_IMPL_H
#define VELK_UI_GLTF_ASSET_IMPL_H

#include <velk/ext/object.h>
#include <velk/string.h>

#include <velk-ui/plugins/gltf/plugin.h>
#include <velk-ui/plugins/gltf/interface/intf_gltf_asset.h>

namespace velk::ui::impl {

/**
 * @brief Concrete IGltfAsset.
 *
 * Round 1 scaffold: stores only the source URI and a loaded/failed flag.
 * `instantiate()` returns null until round 2 wires the cgltf parse path.
 */
class GltfAsset final : public ::velk::ext::Object<GltfAsset, IGltfAsset>
{
public:
    VELK_CLASS_UID(::velk::ui::ClassId::GltfAsset, "GltfAsset");

    /// Sets the asset's source URI and marks it loaded. Round 2 will
    /// populate the parsed glTF data through this (or a sibling) entry point.
    void init(string_view uri);

    /// Marks the asset as failed (decode error, missing file, etc.).
    void init_failed(string_view uri);

    // IResource
    string_view uri() const override { return uri_; }
    bool exists() const override { return loaded_; }
    int64_t size() const override { return 0; }
    bool is_persistent() const override { return persistent_; }
    void set_persistent(bool value) override { persistent_ = value; }

    // IGltfAsset
    IStore::Ptr instantiate() const override;

private:
    string uri_;
    bool loaded_{false};
    bool persistent_{false};
};

} // namespace velk::ui::impl

#endif // VELK_UI_GLTF_ASSET_IMPL_H
