#ifndef VELK_UI_EXT_TRAIT_H
#define VELK_UI_EXT_TRAIT_H

#include <velk/ext/object.h>
#include <velk/interface/intf_metadata_observer.h>

#include <velk-ui/interface/intf_layout_trait.h>
#include <velk-scene/ext/trait.h>

namespace velk::ui::ext {

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

template <class T, class... Extra> using Visual2D = ::velk::ext::Visual2D<T, Extra...>;
template <class T, class... Extra> using Visual3D = ::velk::ext::Visual3D<T, Extra...>;
template <class T, class... Extra> using Render = ::velk::ext::Render<T, Extra...>;

} // namespace velk::ui::ext

#endif // VELK_UI_EXT_TRAIT_H
