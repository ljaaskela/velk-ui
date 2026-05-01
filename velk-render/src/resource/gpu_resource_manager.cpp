#include "gpu_resource_manager.h"

#include <velk/api/velk.h>
#include <velk-render/plugin.h>

namespace velk {

GpuResourceManager::~GpuResourceManager()
{
    GpuResourceManager::shutdown();
}

void GpuResourceManager::init(IRenderBackend* backend)
{
    backend_ = backend;
}

void GpuResourceManager::on_gpu_resource_destroyed(IGpuResource* resource)
{
    on_resource_destroyed(
        resource, backend_ ? backend_->pending_frame_completion_marker() : 0);
}

IRenderTarget::Ptr GpuResourceManager::create_render_texture(const TextureDesc& desc)
{
    if (!backend_) return {};
    TextureId tid = backend_->create_texture(desc);
    if (tid == 0) return {};

    auto rt = instance().create<IRenderTarget>(ClassId::RenderTexture);
    if (!rt) {
        backend_->destroy_texture(tid);
        return {};
    }
    rt->set_size(desc.width, desc.height);
    rt->set_format(desc.format);

    // Registers the texture in texture_map_ keyed by ISurface* (the rt
    // itself) AND stamps `gpu_handle(Default) = tid` on the resource.
    register_texture(rt.get(), tid);

    // Subscribe the renderer's observer so the rt's dtor triggers
    // on_resource_destroyed, which auto-defers `tid` for destroy with
    // the current pending_frame_completion_marker().
    rt->add_gpu_resource_observer(this);
    return rt;
}

IRenderTextureGroup::Ptr GpuResourceManager::create_render_texture_group(
    const TextureGroupDesc& desc)
{
    if (!backend_ || desc.formats.empty() || desc.width <= 0 || desc.height <= 0) return {};
    auto group = backend_->create_render_target_group(desc);
    if (group == 0) return {};

    auto rtg = instance().create<IRenderTextureGroup>(ClassId::RenderTextureGroup);
    if (!rtg) {
        backend_->destroy_render_target_group(group);
        return {};
    }
    rtg->set_size(static_cast<uint32_t>(desc.width), static_cast<uint32_t>(desc.height));
    rtg->set_format(desc.formats[0]);
    rtg->set_depth_format(desc.depth);
    rtg->set_gpu_handle(GpuResourceKey::Default, group);
    for (uint32_t i = 0; i < desc.formats.size(); ++i) {
        rtg->set_attachment(
            i, static_cast<TextureId>(backend_->get_render_target_group_attachment(group, i)));
    }

    // Track for lifecycle: on_resource_destroyed looks up the group
    // handle by ISurface* and enqueues for deferred destroy.
    auto* surf = interface_cast<ISurface>(rtg.get());
    if (surf) {
        std::lock_guard<std::mutex> lock(deferred_mutex_);
        group_map_[surf] = group;
    }
    rtg->add_gpu_resource_observer(this);
    return rtg;
}

TextureId GpuResourceManager::find_texture(ISurface* surf) const
{
    auto it = texture_map_.find(surf);
    return it != texture_map_.end() ? it->second : 0;
}

void GpuResourceManager::register_texture(ISurface* surf, TextureId tid)
{
    if (!surf) return;
    texture_map_[surf] = tid;
    surf->set_gpu_handle(GpuResourceKey::Default, static_cast<uint64_t>(tid));
    // Subscribe so dropping the wrapper auto-defers `tid` for destroy.
    // Idempotent: re-registration on resize doesn't double-subscribe.
    surf->add_gpu_resource_observer(this);
}

void GpuResourceManager::unregister_texture(ISurface* surf)
{
    texture_map_.erase(surf);
}

TextureId GpuResourceManager::ensure_texture_storage(ISurface* surf, const TextureDesc& desc)
{
    if (!surf || !backend_) return 0;
    TextureId tid = find_texture(surf);
    if (tid == 0) {
        tid = backend_->create_texture(desc);
        if (tid == 0) return 0;
        register_texture(surf, tid);
    }
    return tid;
}

IGpuResourceManager::BufferEntry* GpuResourceManager::find_buffer(IBuffer* buf)
{
    auto it = buffer_map_.find(buf);
    return it != buffer_map_.end() ? &it->second : nullptr;
}

void GpuResourceManager::register_buffer(IBuffer* buf, const BufferEntry& entry)
{
    if (!buf) return;
    // Note: doesn't populate `buf->set_gpu_handle(Default, ...)`. For
    // BDA-style buffers the renderer follows up with
    // `set_gpu_handle(Default, backend->gpu_address(entry.handle))`
    // — the GPU virtual address, not the backend handle.
    buffer_map_[buf] = entry;
    // Subscribe so dropping the wrapper auto-defers the handle.
    buf->add_gpu_resource_observer(this);
}

void GpuResourceManager::unregister_buffer(IBuffer* buf)
{
    buffer_map_.erase(buf);
}

IGpuResourceManager::BufferEntry*
GpuResourceManager::ensure_buffer_storage(IBuffer* buf, const GpuBufferDesc& desc)
{
    if (!buf || !backend_) return nullptr;
    auto* be = find_buffer(buf);
    if (be && be->size != desc.size) {
        defer_buffer_destroy(be->handle, backend_->pending_frame_completion_marker());
        unregister_buffer(buf);
        be = nullptr;
    }
    if (!be) {
        BufferEntry entry{};
        entry.handle = backend_->create_buffer(desc);
        if (!entry.handle) return nullptr;
        entry.size = desc.size;
        register_buffer(buf, entry);
        be = find_buffer(buf);
        if (be) {
            buf->set_gpu_handle(GpuResourceKey::Default,
                                backend_->gpu_address(entry.handle));
        }
    }
    return be;
}

bool GpuResourceManager::register_pipeline(IProgram* prog, PipelineId pid)
{
    if (!prog || !pid) {
        return false;
    }
    auto [it, inserted] = pipeline_map_.emplace(prog, pid);
    if (inserted) {
        // First registration: subscribe so dropping the program
        // auto-defers the pipeline for destroy.
        prog->add_gpu_resource_observer(this);
    }
    return inserted;
}

void GpuResourceManager::add_env_observer(const IBuffer::WeakPtr& res)
{
    observed_env_resources_.push_back(res);
}


void GpuResourceManager::defer_texture_destroy(TextureId tid, uint64_t completion_marker)
{
    std::lock_guard<std::mutex> lock(deferred_mutex_);
    deferred_textures_.push_back({tid, completion_marker});
}

void GpuResourceManager::defer_buffer_destroy(GpuBuffer handle, uint64_t completion_marker)
{
    std::lock_guard<std::mutex> lock(deferred_mutex_);
    deferred_buffers_.push_back({handle, completion_marker});
}

void GpuResourceManager::defer_pipeline_destroy(PipelineId pid, uint64_t completion_marker)
{
    std::lock_guard<std::mutex> lock(deferred_mutex_);
    deferred_pipelines_.push_back({pid, completion_marker});
}

void GpuResourceManager::drain_deferred(IRenderBackend& backend)
{
    std::lock_guard<std::mutex> lock(deferred_mutex_);

    for (auto it = deferred_textures_.begin(); it != deferred_textures_.end();) {
        if (backend.is_frame_complete(it->completion_marker)) {
            backend.destroy_texture(it->tid);
            it = deferred_textures_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = deferred_groups_.begin(); it != deferred_groups_.end();) {
        if (backend.is_frame_complete(it->completion_marker)) {
            backend.destroy_render_target_group(it->handle);
            it = deferred_groups_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = deferred_buffers_.begin(); it != deferred_buffers_.end();) {
        if (backend.is_frame_complete(it->completion_marker)) {
            backend.destroy_buffer(it->handle);
            it = deferred_buffers_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = deferred_pipelines_.begin(); it != deferred_pipelines_.end();) {
        if (backend.is_frame_complete(it->completion_marker)) {
            backend.destroy_pipeline(it->pid);
            it = deferred_pipelines_.erase(it);
        } else {
            ++it;
        }
    }
}

void GpuResourceManager::on_resource_destroyed(IGpuResource* resource,
                                               uint64_t completion_marker)
{
    std::lock_guard<std::mutex> lock(deferred_mutex_);

    if (auto* surf = interface_cast<ISurface>(resource)) {
        // Render-target group: cascading destroy frees its attachments.
        auto git = group_map_.find(surf);
        if (git != group_map_.end()) {
            deferred_groups_.push_back({git->second, completion_marker});
            group_map_.erase(git);
            return;
        }
        auto it = texture_map_.find(surf);
        if (it != texture_map_.end()) {
            deferred_textures_.push_back({it->second, completion_marker});
            texture_map_.erase(it);
            return;
        }
    }

    if (auto* buf = interface_cast<IBuffer>(resource)) {
        auto it = buffer_map_.find(buf);
        if (it != buffer_map_.end()) {
            deferred_buffers_.push_back({it->second.handle, completion_marker});
            buffer_map_.erase(it);
        }
    }

    if (auto* prog = interface_cast<IProgram>(resource)) {
        auto it = pipeline_map_.find(prog);
        if (it != pipeline_map_.end()) {
            deferred_pipelines_.push_back({it->second, completion_marker});
            pipeline_map_.erase(it);
        }
    }
}

void GpuResourceManager::shutdown()
{
    if (!backend_) return;

    auto unregister = [](IGpuResource* res, IGpuResourceObserver* obs) {
        if (res) {
            res->remove_gpu_resource_observer(obs);
        }
    };

    // Detach from any env resources we still observe so their CPU
    // dtors (which may run later) don't reach a dead manager.
    for (auto& weak : observed_env_resources_) {
        if (auto key = weak.lock()) {
            unregister(key.get(), this);
        }
    }
    observed_env_resources_.clear();

    {
        std::lock_guard<std::mutex> lock(deferred_mutex_);
        for (auto& d : deferred_textures_) {
            backend_->destroy_texture(d.tid);            
        }
        deferred_textures_.clear();
        for (auto& d : deferred_groups_) {
            backend_->destroy_render_target_group(d.handle);
        }
        deferred_groups_.clear();
        for (auto& d : deferred_buffers_) {
            backend_->destroy_buffer(d.handle);
        }
        deferred_buffers_.clear();
        for (auto& d : deferred_pipelines_) {
            backend_->destroy_pipeline(d.pid);
        }
        deferred_pipelines_.clear();
    }

    for (auto& [key, tid] : texture_map_) {
        backend_->destroy_texture(tid);
        unregister(key, this);
    }
    texture_map_.clear();

    for (auto& [key, group] : group_map_) {
        backend_->destroy_render_target_group(group);
        unregister(key, this);
    }
    group_map_.clear();

    for (auto& [key, entry] : buffer_map_) {
        backend_->destroy_buffer(entry.handle);
        unregister(key, this);
    }
    buffer_map_.clear();

    for (auto& [key, pid] : pipeline_map_) {
        backend_->destroy_pipeline(pid);
        unregister(key, this);
    }
    pipeline_map_.clear();
}

} // namespace velk
