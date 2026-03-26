#ifndef VELK_UI_RENDERER_IMPL_H
#define VELK_UI_RENDERER_IMPL_H

#include <velk/ext/object.h>
#include <velk/vector.h>

#include <unordered_map>
#include <velk-ui/interface/intf_renderer.h>
#include <velk-ui/interface/intf_scene.h>
#include <velk-ui/interface/intf_texture_provider.h>
#include <velk-ui/plugins/render/intf_render_backend.h>
#include <velk-ui/plugins/render/plugin.h>
#include <velk-ui/types.h>

namespace velk_ui {


class Renderer : public velk::ext::Object<Renderer, IRenderer>
{
public:
    VELK_CLASS_UID(ClassId::Renderer, "Renderer");

    bool init(const RenderConfig& config) override;
    ISurface::Ptr create_surface(int width, int height) override;
    void attach(const ISurface::Ptr& surface, const velk::IInterface::Ptr& scene) override;
    void detach(const ISurface::Ptr& surface) override;
    void render() override;
    void shutdown() override;

private:
    struct VisualCommands
    {
        velk::vector<DrawCommand> commands;
        uint64_t pipeline_key = 0;
    };

    struct ElementCache
    {
        velk::vector<VisualCommands> visuals;
        ITextureProvider* texture_provider = nullptr;
    };

    struct SurfaceEntry
    {
        ISurface::Ptr surface;
        IScene::Ptr scene;
        uint64_t surface_id = 0;
        bool batches_dirty = true;
        int cached_width = 0;
        int cached_height = 0;
    };

    void rebuild_commands(IElement* element);
    void rebuild_batches(const SceneState& state, const SurfaceEntry& entry);

    IRenderBackend::Ptr backend_;
    velk::vector<SurfaceEntry> surfaces_;
    std::unordered_map<IElement*, ElementCache> element_cache_;

    // Batch building workspace (reused per frame)
    std::unordered_map<uint64_t, size_t> batch_index_; // composite key -> index in batches_
    velk::vector<RenderBatch> batches_;

    // Pipeline key generation for custom materials
    std::unordered_map<uint64_t, uint64_t> material_hash_to_pipeline_; // source hash -> pipeline key
    uint64_t next_pipeline_key_ = PipelineKey::CustomBase;

    uint64_t next_surface_id_ = 1;
    bool initialized_ = false;
};

} // namespace velk_ui

#endif // VELK_UI_RENDERER_IMPL_H
