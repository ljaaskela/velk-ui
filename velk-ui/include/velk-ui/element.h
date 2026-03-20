#ifndef VELK_UI_ELEMENT_H
#define VELK_UI_ELEMENT_H

#include <velk-ui/interface/intf_element.h>
#include <velk-ui/interface/intf_scene_observer.h>
#include <velk-ui/types.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_metadata_observer.h>

namespace velk_ui {

class Element : public velk::ext::Object<Element, IElement, velk::IMetadataObserver, ISceneObserver>
{
public:
    VELK_CLASS_UID("136ea22f-189a-4750-ad12-d4d15bd6b7cf", "Element");

    void on_property_changed(velk::IProperty& property) override;

    void on_attached(IScene& scene) override;
    void on_detached(IScene& scene) override;

    DirtyFlags consume_dirty();

private:
    IScene* scene_ = nullptr;
    DirtyFlags pending_dirty_ = DirtyFlags::None;
};

} // namespace velk_ui

#endif // VELK_UI_ELEMENT_H
