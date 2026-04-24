#include "gltf_plugin.h"

#include "gltf_asset.h"
#include "gltf_decoder.h"

#include <velk/api/object.h>
#include <velk/api/velk.h>
#include <velk/interface/resource/intf_resource_store.h>

namespace velk::ui::impl {

ReturnValue GltfPlugin::initialize(IVelk& velk, PluginConfig&)
{
    auto rv = register_type<GltfAsset>(velk);
    rv &= register_type<GltfDecoder>(velk);

    auto& store = velk.resource_store();

    // Register decoder so URIs route through it:
    //   "gltf:<inner_uri>" -> GltfDecoder
    auto dec = velk.create<IResourceDecoder>(GltfDecoder::static_class_id());
    if (!dec) {
        return ReturnValue::Fail;
    }
    gltf_decoder_ = dec;
    store.register_decoder(gltf_decoder_);

    return rv;
}

ReturnValue GltfPlugin::shutdown(IVelk& velk)
{
    auto& store = velk.resource_store();
    if (gltf_decoder_) {
        store.unregister_decoder(gltf_decoder_);
        gltf_decoder_ = nullptr;
    }
    return ReturnValue::Success;
}

} // namespace velk::ui::impl
