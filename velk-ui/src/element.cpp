#include <velk-ui/element.h>
#include <velk-ui/interface/intf_scene.h>

#include <velk/interface/intf_metadata.h>

namespace velk_ui {

void Element::on_attached(IScene& scene)
{
    scene_ = &scene;
    pending_dirty_ = DirtyFlags::Layout | DirtyFlags::Visual;
    scene_->notify_dirty(*this, pending_dirty_);
}

void Element::on_detached(IScene&)
{
    scene_ = nullptr;
    pending_dirty_ = DirtyFlags::None;
}

void Element::on_property_changed(velk::IProperty& property)
{
    if (!scene_) return;

    auto* meta = velk::interface_cast<velk::IMetadata>(this);
    if (!meta) return;

    auto members = meta->get_static_metadata();
    auto sid = property.get_storage_id();
    if (sid >= members.size()) return;

    auto name = members[sid].name;

    DirtyFlags flag = DirtyFlags::None;
    if (name == "position" || name == "size" || name == "local_transform") {
        flag = DirtyFlags::Layout;
    } else if (name == "color") {
        flag = DirtyFlags::Visual;
    } else if (name == "z_index") {
        flag = DirtyFlags::ZOrder;
    }

    if (flag == DirtyFlags::None) return;

    bool was_clean = (pending_dirty_ == DirtyFlags::None);
    pending_dirty_ |= flag;

    if (was_clean) {
        scene_->notify_dirty(*this, flag);
    }
}

DirtyFlags Element::consume_dirty()
{
    DirtyFlags result = pending_dirty_;
    pending_dirty_ = DirtyFlags::None;
    return result;
}

} // namespace velk_ui
