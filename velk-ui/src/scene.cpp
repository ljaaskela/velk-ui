#include "scene.h"

#include <velk/api/future.h>
#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_store.h>
#include <velk/interface/resource/intf_resource.h>
#include <velk/interface/resource/intf_resource_store.h>
#include <velk/plugins/importer/api/importer.h>

#include <algorithm>

namespace velk_ui {

namespace {

velk::IHierarchy* get_hierarchy(velk::Hierarchy& h)
{
    return interface_cast<velk::IHierarchy>(h.get());
}

} // namespace

velk::vector<Scene*>& Scene::live_scenes()
{
    static velk::vector<Scene*> scenes;
    return scenes;
}

Scene::Scene()
{
    live_scenes().push_back(this);
}

Scene::~Scene()
{
    auto& scenes = live_scenes();
    scenes.erase(std::remove(scenes.begin(), scenes.end(), this), scenes.end());
}

velk::IFuture::Ptr Scene::load_from(velk::string_view path)
{
    auto promise = velk::make_promise();

    auto file = velk::instance().resource_store().get_resource<velk::IFile>(path);
    if (!file) {
        VELK_LOG(E, "Scene::load_from: failed to resolve resource '%.*s'",
                 static_cast<int>(path.size()), path.data());
        promise.set_value(velk::ReturnValue::Fail);
        return promise.get_future<velk::ReturnValue>();
    }

    velk::string json;
    if (velk::failed(file->read_text(json))) {
        VELK_LOG(E, "Scene::load_from: failed to read '%.*s'",
                 static_cast<int>(path.size()), path.data());
        promise.set_value(velk::ReturnValue::Fail);
        return promise.get_future<velk::ReturnValue>();
    }

    auto importer = velk::create_json_importer();
    auto result = importer.import_from(json);

    for (auto& err : result.errors) {
        VELK_LOG(E, "Scene::load_from: import error: %s", err.c_str());
    }

    if (!result.store) {
        promise.set_value(velk::ReturnValue::Fail);
        return promise.get_future<velk::ReturnValue>();
    }

    load(*result.store);

    promise.set_value(velk::ReturnValue::Success);
    return promise.get_future<velk::ReturnValue>();
}

void Scene::load(velk::IStore& store)
{
    ensure_hierarchy();

    // Find the first hierarchy in the store (convention: "hierarchy:<name>")
    static const velk::string_view hierarchy_keys[] = {"hierarchy:scene", "hierarchy:main", "hierarchy:root"};
    velk::IObject::Ptr hierarchy_obj;
    for (auto& key : hierarchy_keys) {
        hierarchy_obj = store.find(key);
        if (hierarchy_obj) {
            break;
        }
    }

    if (!hierarchy_obj) {
        return;
    }

    auto* src = interface_cast<velk::IHierarchy>(hierarchy_obj);
    if (!src) {
        return;
    }

    auto src_root = src->root();
    if (!src_root) {
        return;
    }

    // Replicate imported hierarchy into our scene
    set_root(src_root);
    replicate_children(*src, src_root);
}

void Scene::set_geometry(velk::aabb geometry)
{
    if (geometry_ != geometry) {
        geometry_ = geometry;
        set_dirty(DirtyFlags::Layout);
    }
}

void Scene::update(const velk::UpdateInfo& info)
{
    auto* h = get_hierarchy(logical_);
    if (!h) {
        return;
    }

    // Merge per-element dirty flags into scene-level flags
    for (auto* elem : dirty_elements_) {
        auto f = elem->consume_dirty();
        dirty_ |= f;
        if (f != DirtyFlags::None) {
            redraw_list_.push_back(elem);
        }
    }
    dirty_elements_.clear();

    if ((dirty_ & DirtyFlags::DrawOrder) != DirtyFlags::None) {
        rebuild_visual_list();
    }

    if ((dirty_ & DirtyFlags::Layout) != DirtyFlags::None) {
        solver_.solve(*h, geometry_);
        // Layout changed, all elements need redraw
        redraw_list_.clear();
        for (auto* elem : visual_list_) {
            redraw_list_.push_back(elem);
        }
    }

    dirty_ = DirtyFlags::None;
}

SceneState Scene::consume_state()
{
    // Swap into local buffers so the returned array_views stay valid
    // until the next consume_state() call.
    consumed_redraw_.swap(redraw_list_);
    consumed_removed_.swap(removed_list_);
    redraw_list_.clear();
    removed_list_.clear();

    SceneState state;
    state.visual_list = {visual_list_.data(), visual_list_.size()};
    state.redraw_list = {consumed_redraw_.data(), consumed_redraw_.size()};
    state.removed_list = {consumed_removed_.data(), consumed_removed_.size()};
    return state;
}

void Scene::notify_dirty(IElement& element, DirtyFlags)
{
    dirty_elements_.push_back(&element);
}

velk::array_view<IElement*> Scene::get_visual_list()
{
    return {visual_list_.data(), visual_list_.size()};
}

void Scene::ensure_hierarchy()
{
    if (!initialized_) {
        logical_ = velk::create_hierarchy();
        initialized_ = true;
    }
}

void Scene::attach_element(const velk::IObject::Ptr& obj)
{
    auto* observer = interface_cast<ISceneObserver>(obj);
    if (observer) {
        observer->on_attached(*this);
    }

    set_dirty(DirtyFlags::DrawOrder);
}

void Scene::detach_element(const velk::IObject::Ptr& obj)
{
    auto* observer = interface_cast<ISceneObserver>(obj);
    if (observer) {
        observer->on_detached(*this);
    }

    // Keep removed elements alive until the renderer consumes them
    removed_list_.push_back(obj);

    set_dirty(DirtyFlags::DrawOrder);
}

void Scene::detach_subtree(const velk::IObject::Ptr& obj)
{
    auto* h = get_hierarchy(logical_);
    if (!h) {
        return;
    }

    velk::vector<velk::IObject::Ptr> stack;
    stack.push_back(obj);
    while (!stack.empty()) {
        auto node = stack.back();
        stack.pop_back();
        auto kids = h->children_of(node);
        for (auto& kid : kids) {
            stack.push_back(kid);
        }
        detach_element(node);
    }
}

void Scene::replicate_children(velk::IHierarchy& src, const velk::IObject::Ptr& parent)
{
    auto kids = src.children_of(parent);
    for (auto& kid : kids) {
        add(parent, kid);
        replicate_children(src, kid);
    }
}

// IHierarchy forwarding

velk::ReturnValue Scene::set_root(const velk::IObject::Ptr& root)
{
    ensure_hierarchy();

    // Detach old root and its subtree
    auto old_root = logical_.root();
    if (old_root) {
        detach_subtree(old_root.object());
    }

    auto rv = logical_.set_root(root);
    if (rv == velk::ReturnValue::Success && root) {
        attach_element(root);
    }
    return rv;
}

velk::ReturnValue Scene::add(const velk::IObject::Ptr& parent, const velk::IObject::Ptr& child)
{
    ensure_hierarchy();

    auto rv = logical_.add(parent, child);
    if (rv == velk::ReturnValue::Success) {
        attach_element(child);
    }
    return rv;
}

velk::ReturnValue Scene::insert(const velk::IObject::Ptr& parent, size_t index,
                                const velk::IObject::Ptr& child)
{
    ensure_hierarchy();

    auto rv = logical_.insert(parent, index, child);
    if (rv == velk::ReturnValue::Success) {
        attach_element(child);
    }
    return rv;
}

velk::ReturnValue Scene::remove(const velk::IObject::Ptr& object)
{
    auto rv = logical_.remove(object);
    if (rv == velk::ReturnValue::Success) {
        detach_subtree(object);
    }
    return rv;
}

velk::ReturnValue Scene::replace(const velk::IObject::Ptr& old_child, const velk::IObject::Ptr& new_child)
{
    auto rv = logical_.replace(old_child, new_child);
    if (rv == velk::ReturnValue::Success) {
        detach_element(old_child);
        attach_element(new_child);
    }
    return rv;
}

void Scene::clear()
{
    auto r = root();
    if (r) {
        detach_subtree(r);
    }
    logical_.clear();
}

velk::IObject::Ptr Scene::root() const
{
    return logical_.root();
}

velk::IObject::Ptr Scene::parent_of(const velk::IObject::Ptr& object) const
{
    return logical_.parent_of(object);
}

velk::vector<velk::IObject::Ptr> Scene::children_of(const velk::IObject::Ptr& object) const
{
    auto h = logical_.operator velk::IHierarchy::Ptr();
    return h ? h->children_of(object) : velk::vector<velk::IObject::Ptr>{};
}

velk::IObject::Ptr Scene::child_at(const velk::IObject::Ptr& object, size_t index) const
{
    return logical_.child_at(object, index);
}

size_t Scene::child_count(const velk::IObject::Ptr& object) const
{
    return logical_.child_count(object);
}

void Scene::for_each_child(const velk::IObject::Ptr& object, void* context, ChildVisitorFn visitor) const
{
    auto h = logical_.operator velk::IHierarchy::Ptr();
    if (h) {
        h->for_each_child(object, context, visitor);
    }
}

bool Scene::contains(const velk::IObject::Ptr& object) const
{
    return logical_.contains(object);
}

size_t Scene::size() const
{
    return logical_.size();
}

velk::IHierarchy::Node Scene::node_of(const velk::IObject::Ptr& object) const
{
    return logical_.node_of(object).hierarchy_node();
}

void Scene::rebuild_visual_list()
{
    visual_list_.clear();
    if (auto r = root()) {
        collect_visual_list(r);
    }
}

void Scene::collect_visual_list(const velk::IObject::Ptr& obj)
{
    auto* element = interface_cast<IElement>(obj);
    if (element) {
        visual_list_.push_back(element);
    }

    auto* h = get_hierarchy(logical_);
    if (!h) {
        return;
    }

    auto kids = h->children_of(obj);
    std::sort(kids.begin(), kids.end(), [](const velk::IObject::Ptr& a, const velk::IObject::Ptr& b) {
        auto ra = velk::read_state<IElement>(a);
        auto rb = velk::read_state<IElement>(b);
        int32_t za = ra ? ra->z_index : 0;
        int32_t zb = rb ? rb->z_index : 0;
        return za < zb;
    });

    for (auto& kid : kids) {
        collect_visual_list(kid);
    }
}

} // namespace velk_ui
