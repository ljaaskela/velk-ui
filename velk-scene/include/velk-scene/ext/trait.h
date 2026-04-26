#ifndef VELK_SCENE_EXT_TRAIT_H
#define VELK_SCENE_EXT_TRAIT_H

#include <velk/ext/object.h>
#include <velk/interface/intf_metadata_observer.h>

#include <velk-render/interface/intf_raster_shader.h>
#include <velk-render/interface/intf_render_context.h>
#include <velk-scene/interface/intf_trait.h>
#include <velk-scene/interface/intf_transform_trait.h>
#include <velk-scene/interface/intf_visual.h>

namespace velk::ext {

template <class T, class... Extra>
class Transform : public ::velk::ext::Object<T, ITransformTrait, ITraitNotify, IMetadataObserver, Extra...>
{
public:
    TraitPhase get_phase() const override { return TraitPhase::Transform; }
    void transform(IElement&) override {}

protected:
    void invoke_trait_dirty(DirtyFlags flags = DirtyFlags::Layout)
    {
        invoke_event(this->get_interface(IInterface::UID), "on_trait_dirty", flags);
    }

    void on_state_changed(string_view, IMetadata&, Uid) override { invoke_trait_dirty(); }
};

template <class T, class Variant, class... Extra>
class VisualBase : public ::velk::ext::Object<T, Variant, ::velk::IRasterShader,
                                              ITraitNotify, IMetadataObserver, Extra...>
{
public:
    TraitPhase get_phase() const override { return TraitPhase::Visual; }

    ::velk::ShaderSource get_raster_source(::velk::IRasterShader::Target) const override
    {
        return {};
    }

    uint64_t get_raster_pipeline_key() const override { return PipelineKey::Default; }

    aabb get_local_bounds(const ::velk::size& bounds) const override
    {
        aabb out;
        out.position = {0.f, 0.f, 0.f};
        out.extent = {bounds.width, bounds.height, bounds.depth};
        return out;
    }

    vector<IBuffer::Ptr> get_gpu_resources(::velk::IRenderContext&) const override { return {}; }

protected:
    void invoke_trait_dirty(DirtyFlags flags = DirtyFlags::Visual)
    {
        invoke_event(this->get_interface(IInterface::UID), "on_trait_dirty", flags);
    }

    void invoke_visual_changed() { invoke_trait_dirty(DirtyFlags::Visual); }

    void on_state_changed(string_view, IMetadata&, Uid) override { invoke_trait_dirty(); }
};

template <class T, class... Extra>
class Visual2D : public VisualBase<T, IVisual2D, Extra...> {};

template <class T, class... Extra>
class Visual3D : public VisualBase<T, IVisual3D, Extra...> {};

template <class T, class... Extra>
class Render : public ::velk::ext::Object<T, ITraitNotify, Extra...>
{
public:
    TraitPhase get_phase() const override { return TraitPhase::Render; }

protected:
    void invoke_trait_dirty(DirtyFlags flags = DirtyFlags::Visual)
    {
        invoke_event(this->get_interface(IInterface::UID), "on_trait_dirty", flags);
    }
};

} // namespace velk::ext

#endif // VELK_SCENE_EXT_TRAIT_H
