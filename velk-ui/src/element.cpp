#include "element.h"

#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object_storage.h>

#include <velk-ui/interface/intf_layout_notify.h>
#include <velk-ui/interface/intf_render_to_texture.h>
#include <velk-ui/interface/intf_scene.h>
#include <velk-ui/interface/intf_visual.h>

namespace velk::ui {

void Element::on_attached(IScene& scene)
{
    auto s = interface_cast<IObject>(&scene)->get_self<IScene>();
    scene_ = s;
    pending_dirty_ = DirtyFlags::All;
    s->notify_dirty(*this, pending_dirty_);
    subscribe_traits();
}

void Element::on_detached(IScene&)
{
    trait_subs_.clear();
    scene_ = nullptr;
    pending_dirty_ = DirtyFlags::None;
}

void Element::on_state_changed(string_view name, IMetadata& owner, Uid interfaceId)
{
    auto scene = get_scene();
    if (!scene) {
        return;
    }

    auto* meta = interface_cast<IMetadata>(this);
    if (!meta) {
        return;
    }

    DirtyFlags flag = DirtyFlags::None;
    if (name == "position" || name == "size") {
        flag = DirtyFlags::Layout;
    } else if (name == "z_index") {
        flag = DirtyFlags::DrawOrder;
    }

    if (flag == DirtyFlags::None) {
        return;
    }

    bool was_clean = (pending_dirty_ == DirtyFlags::None);
    pending_dirty_ |= flag;

    if (was_clean) {
        scene->notify_dirty(*this, flag);
    }
}

DirtyFlags Element::consume_dirty()
{
    DirtyFlags result = pending_dirty_;
    pending_dirty_ = DirtyFlags::None;
    return result;
}

ReturnValue Element::add_attachment(const IInterface::Ptr& attachment)
{
    auto rv = Object::add_attachment(attachment);
    if (succeeded(rv) && scene_.lock()) {
        subscribe_trait(attachment);
    }
    return rv;
}

ReturnValue Element::remove_attachment(const IInterface::Ptr& attachment)
{
    auto rv = Object::remove_attachment(attachment);
    if (succeeded(rv) && scene_.lock()) {
        // Re-subscribe from scratch: the removed trait's event handler is
        // now stale. This is simpler than tracking individual subscriptions
        // and only runs on structural changes (rare).
        if (interface_cast<IRenderToTexture>(attachment)) {
            render_trait_count--;
        }
        subscribe_traits();
    }
    return rv;
}

void Element::subscribe_trait(const IInterface::Ptr& attachment)
{
    auto notify = [this](DirtyFlags flag) {
        return [this, flag](FnArgs) -> ReturnValue {
            auto scene = get_scene();
            if (!scene) {
                return ReturnValue::Fail;
            }
            bool was_clean = (pending_dirty_ == DirtyFlags::None);
            pending_dirty_ |= flag;
            if (was_clean) {
                scene->notify_dirty(*this, flag);
            }
            return ReturnValue::Success;
        };
    };

    if (interface_cast<IRenderToTexture>(attachment)) {
        render_trait_count++;
    }

    // Visual traits: on_visual_changed -> DirtyFlags::Visual
    if (auto* visual = interface_cast<IVisual>(attachment)) {
        Event evt = visual->on_visual_changed();
        if (evt) {
            trait_subs_.emplace_back(evt, notify(DirtyFlags::Visual));
        }
    }

    // Layout/transform traits: on_layout_changed -> DirtyFlags::Layout
    if (auto* layout_notify = interface_cast<ILayoutNotify>(attachment)) {
        Event evt = layout_notify->on_layout_changed();
        if (evt) {
            trait_subs_.emplace_back(evt, notify(DirtyFlags::Layout));
        }
    }
}

void Element::subscribe_traits()
{
    trait_subs_.clear();

    auto* storage = interface_cast<IObjectStorage>(this);
    if (storage) {
        for (size_t i = 0; i < storage->attachment_count(); ++i) {
            subscribe_trait(storage->get_attachment(i));
        }
    }
}

} // namespace velk::ui
