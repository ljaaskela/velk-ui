#include "batch_builder.h"

#include "default_ui_shaders.h"

#include <velk/api/perf.h>
#include <velk/api/state.h>
#include <velk/interface/intf_object_storage.h>
#include <velk-render/gpu_data.h>
#include <velk-render/interface/intf_draw_data.h>
#include <velk-render/interface/intf_mesh.h>
#include <velk-render/interface/intf_render_target.h>
#include <velk-render/interface/intf_surface.h>
#include <velk-render/interface/material/intf_material.h>
#include <velk-render/interface/material/intf_material_options.h>
#include <velk-render/interface/intf_raster_shader.h>
#include "gpu_resource_manager.h"
#include <velk-scene/interface/intf_visual.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <unordered_set>
#include <velk/string.h>

namespace {

uint64_t make_batch_key(uint64_t pipeline, uint64_t mesh, uint64_t texture)
{
    return (pipeline * 31 + mesh) * 31 + texture;
}

// Builds PipelineOptions from an attached IMaterialOptions. Storage is
// either the material (IProgram) or, when there is no material, the
// visual itself — so visuals like CubeVisual that attach their own
// options still drive pipeline state. If no options are attached,
// the PipelineOptions defaults (2D-safe: no cull, alpha blend, no depth)
// apply; topology must still be set explicitly by the caller.
velk::PipelineOptions pipeline_options_from_storage(velk::IObjectStorage* storage)
{
    velk::PipelineOptions o{};
    if (!storage) return o;
    auto opts = storage->template find_attachment<velk::IMaterialOptions>();
    if (!opts) return o;
    auto r = velk::read_state<velk::IMaterialOptions>(opts.get());
    if (!r) return o;
    o.cull_mode = r->cull_mode;
    o.front_face = r->front_face;
    // Mask and Opaque both write opaquely; Blend alpha-blends.
    o.blend_mode = (r->alpha_mode == velk::AlphaMode::Blend)
                       ? velk::BlendMode::Alpha
                       : velk::BlendMode::Opaque;
    o.depth_test = r->depth_test;
    o.depth_write = r->depth_write;
    return o;
}

velk::Topology to_backend_topology(velk::MeshTopology mt)
{
    return mt == velk::MeshTopology::TriangleStrip
             ? velk::Topology::TriangleStrip
             : velk::Topology::TriangleList;
}

} // namespace

namespace velk {

void BatchBuilder::rebuild_commands(IElement* element, IGpuResourceObserver* observer,
                                    IRenderContext* render_ctx)
{
    VELK_PERF_SCOPE("renderer.rebuild_commands");
    auto& cache = element_cache_[element];
    cache.before_visuals.clear();
    cache.after_visuals.clear();
    cache.gpu_resources.clear();

    auto* storage = interface_cast<IObjectStorage>(element);
    if (!storage) {
        return;
    }

    auto state = read_state<IElement>(element);
    if (!state) {
        return;
    }

    const ::velk::size& local_size = state->size;

    for (size_t i = 0; i < storage->attachment_count(); ++i) {
        auto att = storage->get_attachment(i);
        auto* visual = interface_cast<IVisual>(att);
        if (!visual) {
            continue;
        }

        VisualCommands vc;
        {
            VELK_PERF_SCOPE("renderer.get_draw_entries");
            if (render_ctx) {
                vc.entries = visual->get_draw_entries(*render_ctx, local_size);
            }
        }

        // Resolve the visual's primitive. 3D visuals (cube, sphere,
        // future MeshVisual) populate `entry.primitive` themselves
        // inside `get_draw_entries`; 2D visuals leave it null and fall
        // back to the unit quad's primitive here.
        //
        // The unit quad's buffers are singletons that outlive every
        // element. They get destroyed when the render context goes
        // away, which is AFTER the renderer (Application destroys
        // renderer_ before render_ctx_). So we deliberately skip
        // add_gpu_resource_observer here: the buffer's destructor would
        // otherwise notify a dead renderer at process exit.
        IMeshPrimitive::Ptr quad_primitive;
        if (render_ctx) {
            auto quad = render_ctx->get_mesh_builder().get_unit_quad();
            if (quad) {
                auto prims = quad->get_primitives();
                if (prims.size() > 0) {
                    quad_primitive = prims[0];
                }
            }
        }
        // Surface every referenced primitive's buffer for the upload pass.
        // Includes the optional UV1 stream so its GPU address is ready by
        // the time batch_builder writes it into DrawDataHeader::uv1_address.
        auto push_primitive_buffer = [&](IMeshPrimitive* p) {
            if (!p) return;
            if (auto buf = interface_pointer_cast<IBuffer>(p->get_buffer())) {
                cache.gpu_resources.push_back(buf);
            }
            if (auto uv1 = interface_pointer_cast<IBuffer>(p->get_uv1_buffer())) {
                cache.gpu_resources.push_back(uv1);
            }
        };
        push_primitive_buffer(quad_primitive.get());
        for (auto& entry : vc.entries) {
            if (!entry.primitive) {
                entry.primitive = quad_primitive;
            } else {
                push_primitive_buffer(entry.primitive.get());
            }
        }

        // Ensure the visual's built-in pipeline is compiled. Empty shader
        // sources on the visual fall back to the renderer's registered
        // defaults (see IRenderContext::compile_pipeline). Visuals that
        // don't contribute their own shader (TextureVisual, cube/sphere
        // analytic-only) simply don't implement IRasterShader.
        //
        // Pipeline topology must match the mesh bound by the draw; we
        // take it from the first entry's mesh.
        if (render_ctx) {
            if (auto* rs = interface_cast<IRasterShader>(visual)) {
                vc.raster_shader = interface_pointer_cast<IRasterShader>(att);
                uint64_t key = rs->get_raster_pipeline_key();
                if (key != 0 && render_ctx->pipeline_map().find(key) == render_ctx->pipeline_map().end()) {
                    auto src = rs->get_raster_source(IRasterShader::Target::Forward);
                    ::velk::Topology topo = ::velk::Topology::TriangleStrip;
                    if (!vc.entries.empty() && vc.entries.front().primitive) {
                        topo = to_backend_topology(vc.entries.front().primitive->get_topology());
                    }
                    auto po = pipeline_options_from_storage(interface_cast<IObjectStorage>(visual));
                    po.topology = topo;
                    render_ctx->compile_pipeline(src.fragment, src.vertex, key, 0, po);
                }
            }
        }

        // Capture an optional per-visual `velk_visual_discard` snippet.
        // Deferred gbuffer fragments call this at the top of main(); the
        // composer in build_gbuffer_draw_calls splices the snippet in.
        // The key perturbation is folded from the visual's class uid so
        // two visuals sharing a material still get distinct pipelines.
        if (auto* snippet = interface_cast<IShaderSnippet>(visual)) {
            if (!snippet->get_snippet_source().empty()) {
                vc.visual_discard = interface_pointer_cast<IShaderSnippet>(att);
                if (auto* obj = interface_cast<IObject>(visual)) {
                    Uid uid = obj->get_class_uid();
                    vc.discard_key_perturb = uid.lo ^ uid.hi;
                }
            }
        }

        // Per-entry material resolution. Each DrawEntry carries its own
        // material (filled by the visual's get_draw_entries); the visual
        // itself doesn't advertise one. Compile the forward pipeline
        // once per unique material, then stash the handle on each entry
        // as `pipeline_override` for rebuild_batches to pick up.
        {
            VELK_PERF_SCOPE("renderer.resolve_material");
            std::unordered_set<IProgram*> seen;
            for (auto& entry : vc.entries) {
                if (!entry.material || !render_ctx) continue;
                auto prog = entry.material;
                if (seen.insert(prog.get()).second) {
                    if (auto* mat = interface_cast<IMaterial>(prog.get());
                        mat && prog->get_pipeline_handle(*render_ctx) == 0) {
                        auto eval_src = mat->get_eval_src();
                        auto vertex_src = mat->get_vertex_src();
                        auto eval_fn = mat->get_eval_fn_name();
                        auto frag_src = mat->get_fragment_src();
                        auto po = pipeline_options_from_storage(
                            interface_cast<IObjectStorage>(prog.get()));
                        // Topology from this entry's primitive. Two entries
                        // sharing a material must share topology; mixing
                        // topologies on one material is out of scope (would
                        // need per-topology pipeline caching).
                        po.topology = Topology::TriangleList;
                        if (entry.primitive) {
                            po.topology = to_backend_topology(entry.primitive->get_topology());
                        }
                        uint64_t h = 0;
                        if (!eval_src.empty() && !vertex_src.empty() && !eval_fn.empty()) {
                            mat->register_eval_includes(*render_ctx);
                            string frag = compose_eval_fragment(
                                forward_fragment_driver_template, eval_src, eval_fn,
                                mat->get_forward_discard_threshold());
                            h = render_ctx->compile_pipeline(
                                string_view(frag), vertex_src, 0, 0, po);
                        } else if (!frag_src.empty() && !vertex_src.empty()) {
                            // Raw-shader path: the material supplies a full
                            // fragment source (e.g. ShaderMaterial).
                            h = render_ctx->compile_pipeline(
                                frag_src, vertex_src, 0, 0, po);
                        }
                        if (h) {
                            prog->set_pipeline_handle(h);
                        }
                    }
                    // Multi-texture materials (e.g. StandardMaterial)
                    // attach their textures to the material. Surface the
                    // ones that carry uploadable pixel data so renderer's
                    // upload pass registers them.
                    if (auto* mat = interface_cast<IMaterial>(prog.get())) {
                        for (auto* tex : mat->get_textures()) {
                            if (auto buf = ::velk::get_self<IBuffer>(tex)) {
                                if (observer) {
                                    buf->add_gpu_resource_observer(observer);
                                }
                                cache.gpu_resources.push_back(buf);
                            }
                        }
                    }
                }
                entry.pipeline_override = prog->get_pipeline_handle(*render_ctx);
            }
        }

        for (auto& res : (render_ctx ? visual->get_gpu_resources(*render_ctx) : vector<IBuffer::Ptr>{})) {
            if (res) {
                if (observer) {
                    res->add_gpu_resource_observer(observer);
                }
                cache.gpu_resources.push_back(res);
            }
        }

        auto v2d = read_state<IVisual2D>(visual);
        VisualPhase phase = v2d ? v2d->visual_phase : VisualPhase::BeforeChildren;
        if (phase == VisualPhase::AfterChildren) {
            cache.after_visuals.push_back(std::move(vc));
        } else {
            cache.before_visuals.push_back(std::move(vc));
        }
    }
}

void BatchBuilder::rebuild_batches(const SceneState& state, vector<Batch>& out_batches)
{
    VELK_PERF_SCOPE("renderer.rebuild_batches");
    out_batches.clear();
    // Note: render_target_passes_ is NOT cleared here — it's cleared
    // once per frame in reset_frame_state (called at Renderer::prepare
    // start). Each view's rebuild_batches appends; find_or_make_pass
    // dedups so an RTT element shared between views only emits once
    // per frame from build_shared_passes.

    auto resolve_texture = [](const IProgram* material, uint64_t fallback) -> uint64_t {
        return material ? reinterpret_cast<uintptr_t>(material) : fallback;
    };

    auto get_visuals = [](const ElementCache& cache, VisualPhase phase) -> const vector<VisualCommands>& {
        return phase == VisualPhase::AfterChildren ? cache.after_visuals : cache.before_visuals;
    };

    auto emit_visuals = [&](const vector<IElement*>& entries, VisualPhase phase,
                            vector<Batch>& target_batches,
                            float offset_x = 0.f, float offset_y = 0.f) {
        uint64_t last_bkey = 0;
        size_t max_visuals = 0;
        for (auto* elem : entries) {
            auto it = element_cache_.find(elem);
            if (it != element_cache_.end()) {
                max_visuals = std::max(max_visuals, get_visuals(it->second, phase).size());
            }
        }

        for (size_t pass = 0; pass < max_visuals; ++pass) {
            for (auto* elem : entries) {
                auto it = element_cache_.find(elem);
                if (it == element_cache_.end()) {
                    continue;
                }

                auto& visuals = get_visuals(it->second, phase);
                if (pass >= visuals.size()) {
                    continue;
                }

                auto elem_state = read_state<IElement>(elem);
                if (!elem_state) {
                    continue;
                }

                // Every instance layout begins with a mat4 world_matrix
                // (see instance_types.h). We copy the element's transform
                // here and — for RTT sub-passes — subtract the RTT root's
                // world translation so children render in target-local
                // space. Everything downstream (rotation, 3D z) comes for
                // free because the full matrix is passed through.
                mat4 world = elem_state->world_matrix;
                world(0, 3) -= offset_x;
                world(1, 3) -= offset_y;

                auto& vc = visuals[pass];
                for (auto& de : vc.entries) {
                    uint64_t pipeline = (de.pipeline_override != 0) ? de.pipeline_override : de.pipeline_key;
                    uint64_t texture = de.texture_key;
                    uint64_t prim_key = reinterpret_cast<uintptr_t>(de.primitive.get());

                    uint64_t bkey = make_batch_key(pipeline, prim_key,
                                                   resolve_texture(de.material.get(), texture));

                    if (target_batches.empty() || bkey != last_bkey) {
                        Batch batch;
                        batch.pipeline_key = pipeline;
                        batch.texture_key = texture;
                        batch.instance_stride = de.instance_size;
                        batch.material = de.material;
                        batch.visual_discard = vc.visual_discard;
                        batch.discard_key_perturb = vc.discard_key_perturb;
                        batch.primitive = de.primitive;
                        batch.raster_shader = vc.raster_shader;
                        target_batches.push_back(std::move(batch));
                        last_bkey = bkey;
                    }

                    auto& batch = target_batches.back();

                    auto data_offset = batch.instance_data.size();
                    batch.instance_data.resize(data_offset + de.instance_size);
                    std::memcpy(batch.instance_data.data() + data_offset, de.instance_data,
                                de.instance_size);

                    // Overwrite the leading mat4 world_matrix slot. The
                    // visual leaves it zero-initialised; we fill it per
                    // instance so shaders get the correct transform.
                    std::memcpy(batch.instance_data.data() + data_offset, world.m, sizeof(world.m));

                    batch.instance_count++;
                    batch.world_aabb = aabb::merge(batch.world_aabb, elem_state->world_aabb);
                }
            }
        }
    };

    // Walk the scene tree and route elements into main vs RTT subtree
    // lists. `main_*` keeps top-level elements; each RenderTarget-marked
    // subtree produces its own RenderTargetPassData with pre- and post-
    // order element lists. Sibling z-sort matches the previous Scene-
    // side visual_list build.
    vector<IElement*> main_before;
    vector<IElement*> main_after;

    auto* scene = state.scene;

    // Returns a fresh pass entry for `rt_elem`, or nullptr if some
    // earlier view's rebuild this frame already populated this element
    // (same scene walked by two views → same subtree). The nullptr
    // answer makes the walk skip re-appending into the existing entry,
    // so the RTT pass draws the subtree exactly once.
    auto find_or_make_pass = [&](IElement* rt_elem) -> RenderTargetPassData* {
        for (auto& rtp : render_target_passes_) {
            if (rtp.element == rt_elem) return nullptr;
        }
        render_target_passes_.push_back({rt_elem, {}, {}, {}});
        return &render_target_passes_.back();
    };

    // Every element is recorded in the ambient (main) lists. An
    // element inside an active RTT subtree is ALSO recorded into that
    // pass's lists so the subtree renders into the target texture.
    // An element carrying a RenderCache trait starts its own pass that
    // covers itself + all descendants, so a textured rect sampling its
    // own RTT renders correctly into both.
    std::function<void(const IObject::Ptr&,
                       vector<IElement*>&, vector<IElement*>&,
                       RenderTargetPassData*)> walk;
    walk = [&](const IObject::Ptr& obj,
               vector<IElement*>& ambient_before,
               vector<IElement*>& ambient_after,
               RenderTargetPassData* active_pass) {
        auto elem_ptr = interface_pointer_cast<IElement>(obj);
        if (!elem_ptr) return;
        auto* elem = elem_ptr.get();

        RenderTargetPassData* inner_pass = active_pass;
        if (elem->has_render_traits()) {
            inner_pass = find_or_make_pass(elem);
        }

        ambient_before.push_back(elem);
        if (inner_pass) inner_pass->before_entries.push_back(elem);

        if (scene) {
            auto kids = scene->children_of(obj);
            std::sort(kids.begin(), kids.end(),
                [](const IObject::Ptr& a, const IObject::Ptr& b) {
                    auto ra = read_state<IElement>(a);
                    auto rb = read_state<IElement>(b);
                    int32_t za = ra ? ra->z_index : 0;
                    int32_t zb = rb ? rb->z_index : 0;
                    return za < zb;
                });
            for (auto& kid : kids) {
                walk(kid, ambient_before, ambient_after, inner_pass);
            }
        }

        ambient_after.push_back(elem);
        if (inner_pass) inner_pass->after_entries.push_back(elem);
    };

    if (scene) {
        walk(scene->root(), main_before, main_after, nullptr);
    }

    // Batch render target passes (offset by the element's world position)
    for (auto& rtp : render_target_passes_) {
        auto es = read_state<IElement>(rtp.element);
        float ox = es ? es->world_matrix(0, 3) : 0.f;
        float oy = es ? es->world_matrix(1, 3) : 0.f;
        emit_visuals(rtp.before_entries, VisualPhase::BeforeChildren, rtp.batches, ox, oy);
        emit_visuals(rtp.after_entries, VisualPhase::AfterChildren, rtp.batches, ox, oy);
    }

    emit_visuals(main_before, VisualPhase::BeforeChildren, out_batches);
    emit_visuals(main_after, VisualPhase::AfterChildren, out_batches);
}

// build_draw_calls and build_gbuffer_draw_calls moved to
// IRenderContext (impl in velk-render/src/render_context.cpp).
// BatchBuilder now only handles scene-walking (rebuild_commands /
// rebuild_batches) plus the per-frame material-cache lifecycle.

} // namespace velk
