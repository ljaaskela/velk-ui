#ifndef VELK_UI_EXT_TRAIT_H
#define VELK_UI_EXT_TRAIT_H

#include <velk/ext/object.h>
#include <velk/interface/intf_metadata_observer.h>

#include <velk-render/interface/intf_raster_shader.h>
#include <velk-render/interface/intf_render_context.h>
#include <velk-ui/interface/intf_layout_trait.h>
#include <velk-ui/interface/intf_trait.h>
#include <velk-ui/interface/intf_transform_trait.h>
#include <velk-ui/interface/intf_visual.h>

namespace velk::ui::ext {

/**
 * @brief CRTP base for ILayoutTrait implementations.
 *
 * Provides default no-op measure/apply and a compile-time phase.
 * Includes ITraitNotify and IMetadataObserver: any property change
 * fires on_trait_dirty(Layout) so the owning element triggers a re-solve.
 */
template <class T, TraitPhase Phase, class... Extra>
class Layout : public ::velk::ext::Object<T, ILayoutTrait, ITraitNotify, IMetadataObserver, Extra...>
{
public:
    TraitPhase get_phase() const override { return Phase; }
    Constraint measure(const Constraint& c, IElement&, IHierarchy&) override { return c; }
    void apply(const Constraint&, IElement&, IHierarchy&) override {}

protected:
    void invoke_trait_dirty(DirtyFlags flags = DirtyFlags::Layout)
    {
        invoke_event(this->get_interface(IInterface::UID), "on_trait_dirty", flags);
    }

    void on_state_changed(string_view, IMetadata&, Uid) override { invoke_trait_dirty(); }
};

/**
 * @brief CRTP base for ITransformTrait implementations.
 *
 * Includes ITraitNotify and IMetadataObserver: any property change
 * fires on_trait_dirty(Layout) so the owning element triggers a re-solve
 * (transforms run during layout).
 */
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

/**
 * @brief Shared CRTP base for Visual2D / Visual3D. Not intended for
 *        direct use; concrete visuals extend `Visual2D` or `Visual3D`.
 *
 * @tparam T       The concrete visual class (CRTP parameter).
 * @tparam Variant The authoring sub-interface (IVisual2D or IVisual3D).
 * @tparam Extra   Additional interfaces the visual implements.
 */
template <class T, class Variant, class... Extra>
class VisualBase : public ::velk::ext::Object<T, Variant, ::velk::IRasterShader,
                                              ITraitNotify, IMetadataObserver, Extra...>
{
public:
    TraitPhase get_phase() const override { return TraitPhase::Visual; }

    /**
     * @brief Default: empty vertex+fragment for the requested target
     *        (renderer substitutes its registered defaults). Visuals
     *        that supply their own shader override this.
     */
    ::velk::ShaderSource get_raster_source(::velk::IRasterShader::Target) const override
    {
        return {};
    }

    /** @brief Default: `PipelineKey::Default`. Override for custom shader pipelines. */
    uint64_t get_raster_pipeline_key() const override { return PipelineKey::Default; }

    /**
     * @brief Default local-space bounds = the element's own @p bounds.
     * Overridden by visuals whose render extent can exceed the layout
     * box (text overflow, shadows, outlines).
     */
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

    /** @brief Backward compat: fires on_trait_dirty(Visual). */
    void invoke_visual_changed() { invoke_trait_dirty(DirtyFlags::Visual); }

    void on_state_changed(string_view, IMetadata&, Uid) override { invoke_trait_dirty(); }
};

/**
 * @brief CRTP base for 2D visuals (rect, rounded rect, text, image, ...).
 *
 * Adds the IVisual2D authoring surface (color / paint / visual_phase).
 */
template <class T, class... Extra>
class Visual2D : public VisualBase<T, IVisual2D, Extra...> {};

/**
 * @brief CRTP base for 3D visuals (cube, sphere, future mesh / glTF).
 *
 * Adds the IVisual3D authoring surface. 3D visuals drive appearance
 * through attached materials rather than the 2D color / paint slots.
 */
template <class T, class... Extra>
class Visual3D : public VisualBase<T, IVisual3D, Extra...> {};

/**
 * @brief CRTP base for ITrait implementations running in
 *        `TraitPhase::Render` (e.g. RenderCache). Not used by
 *        IRenderTrait-based render traits (Camera, Light) — those
 *        inherit `ext::Object` directly since they're not ITraits.
 *
 * @tparam T     The concrete class (CRTP parameter).
 * @tparam Extra Additional interfaces (e.g. IRenderToTexture).
 */
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

} // namespace velk::ui::ext

#endif // VELK_UI_EXT_TRAIT_H
