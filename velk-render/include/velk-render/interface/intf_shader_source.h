#ifndef VELK_RENDER_INTF_SHADER_SOURCE_H
#define VELK_RENDER_INTF_SHADER_SOURCE_H

#include <velk/interface/intf_metadata.h>
#include <velk/string_view.h>

#include <cstdint>

namespace velk {

/**
 * @brief Contract for objects that supply GLSL source strings for
 *        pipeline stages.
 *
 * Implemented by visuals that bring their own raster shader and/or a
 * `velk_visual_discard` snippet. Render paths (`ForwardPath`,
 * `DeferredPath`) splice the requested role's source into their
 * driver templates; non-implemented roles return an empty string and
 * the path's default stub remains in place.
 *
 * `Role::Vertex` and `Role::Fragment` cover the standard raster
 * stages. `Role::Discard` is the full `void velk_visual_discard()`
 * definition — the deferred g-buffer composer appends it verbatim
 * after the fragment driver so per-visual SDF / coverage clipping
 * lands in the deferred pass without each visual carrying a full
 * deferred fragment.
 */
class IShaderSource : public Interface<IShaderSource>
{
public:
    enum class Role : uint8_t
    {
        Vertex,
        Fragment,
        Discard,
    };

    /**
     * @brief Returns the GLSL source body for @p role.
     *
     * Empty when this source does not contribute the role; the path
     * falls through to its driver template's stub for that stage
     * (e.g. an empty `void velk_visual_discard() {}` for Discard).
     */
    virtual string_view get_source(Role role) const = 0;

    /**
     * @brief Stable per-class id for the pipeline cache.
     *
     * Visuals with their own raster shader return a `PipelineKey::*`
     * constant so two instances share a compiled pipeline. Sources
     * that are only an addendum to a material-driven pipeline (e.g.
     * a discard-only contributor) can leave the default 0; the
     * material's auto-allocated handle drives the cache key in that
     * case.
     */
    virtual uint64_t get_pipeline_key() const { return 0; }
};

} // namespace velk

#endif // VELK_RENDER_INTF_SHADER_SOURCE_H
