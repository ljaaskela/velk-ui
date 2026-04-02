#ifndef VELK_UI_INTF_MATERIAL_H
#define VELK_UI_INTF_MATERIAL_H

#include <velk/interface/intf_metadata.h>

#include <cstdint>

namespace velk_ui {

class IRenderContext; // forward declaration

/**
 * @brief Interface for custom materials that override the default color fill.
 *
 * A material defines how a visual's geometry is shaded. When a visual's `paint`
 * property references an IMaterial, the renderer uses the material's pipeline
 * instead of the visual's `color` property.
 *
 * Every material provides a pipeline handle that the renderer uses to look up
 * the compiled shader program. Material properties are mapped to shader uniforms
 * by the renderer via metadata introspection.
 */
class IMaterial : public velk::Interface<IMaterial>
{
public:
    /**
     * @brief Returns the pipeline handle for this material's shader program.
     *
     * The render context is provided so that materials can lazily compile
     * and register their pipeline on first use.
     * Returns 0 if no pipeline is available.
     */
    virtual uint64_t get_pipeline_handle(IRenderContext& ctx) = 0;
};

} // namespace velk_ui

#endif // VELK_UI_INTF_MATERIAL_H
