#ifndef VELK_UI_GLTF_DECODER_H
#define VELK_UI_GLTF_DECODER_H

#include <velk/ext/object.h>
#include <velk/interface/resource/intf_resource_decoder.h>

#include <velk-ui/plugins/gltf/plugin.h>

namespace velk::ui::impl {

/**
 * @brief Resource decoder turning raw glTF / glb bytes into GltfAsset objects.
 *
 * Registered with the resource store under the name "gltf". Apps fetch
 * loaded assets via
 *     `instance().resource_store().get_resource<IGltfAsset>(
 *         "gltf:app://path/to/file.glb")`.
 *
 * Round 1 stub: the inner resource is accepted and a Failed GltfAsset is
 * returned. Round 2 adds cgltf parsing.
 */
class GltfDecoder final : public ::velk::ext::Object<GltfDecoder, IResourceDecoder>
{
public:
    VELK_CLASS_UID(::velk::ui::ClassId::GltfDecoder, "GltfDecoder");

    string_view name() const override { return "gltf"; }
    IResource::Ptr decode(const IResource::Ptr& inner) const override;
};

} // namespace velk::ui::impl

#endif // VELK_UI_GLTF_DECODER_H
