#ifndef VELK_RENDER_MATERIAL_PROPERTY_H
#define VELK_RENDER_MATERIAL_PROPERTY_H

#include <velk/ext/object.h>
#include <velk/interface/intf_metadata_observer.h>
#include <velk/vector.h>

#include <velk-render/interface/material/intf_material_options.h>
#include <velk-render/interface/material/intf_material_property.h>
#include <velk-render/plugin.h>

namespace velk::impl {

/**
 * @brief Property classes attached to a material to configure its inputs.
 *
 * Each implements two interfaces: its class-specific interface (e.g.
 * IBaseColorProperty) carrying class-specific factors, and IMaterialProperty
 * carrying the common texture + UV transform + tex_coord state. Instances
 * are pure data holders; all behavior lives on the material that owns them.
 *
 * See design-notes/material_properties.md.
 */

class BaseColorProperty
    : public ext::Object<BaseColorProperty, IBaseColorProperty, IMaterialProperty>
{
public:
    VELK_CLASS_UID(ClassId::BaseColorProperty, "BaseColorProperty");
};

class MetallicRoughnessProperty
    : public ext::Object<MetallicRoughnessProperty, IMetallicRoughnessProperty, IMaterialProperty>
{
public:
    VELK_CLASS_UID(ClassId::MetallicRoughnessProperty, "MetallicRoughnessProperty");
};

class NormalProperty
    : public ext::Object<NormalProperty, INormalProperty, IMaterialProperty>
{
public:
    VELK_CLASS_UID(ClassId::NormalProperty, "NormalProperty");
};

class OcclusionProperty
    : public ext::Object<OcclusionProperty, IOcclusionProperty, IMaterialProperty>
{
public:
    VELK_CLASS_UID(ClassId::OcclusionProperty, "OcclusionProperty");
};

class EmissiveProperty
    : public ext::Object<EmissiveProperty, IEmissiveProperty, IMaterialProperty>
{
public:
    VELK_CLASS_UID(ClassId::EmissiveProperty, "EmissiveProperty");
};

class SpecularProperty
    : public ext::Object<SpecularProperty, ISpecularProperty, IMaterialProperty>
{
public:
    VELK_CLASS_UID(ClassId::SpecularProperty, "SpecularProperty");
};

/**
 * @brief Material pipeline options carrier.
 *
 * Observes its own state changes (as IMetadataObserver on itself) and
 * forwards any IMaterialOptions property write to subscribed
 * IMaterialOptionsObservers, so dependent materials can invalidate
 * their cached pipeline handles without polling.
 */
class MaterialOptions
    : public ext::Object<MaterialOptions, IMaterialOptions, IMetadataObserver>
{
public:
    VELK_CLASS_UID(ClassId::MaterialOptions, "MaterialOptions");

    void on_state_changed(string_view /*name*/, IMetadata& /*owner*/, Uid interface_id) override
    {
        if (interface_id != IMaterialOptions::UID) {
            return;
        }
        for (auto* obs : observers_) {
            if (obs) obs->on_material_options_changed(this);
        }
    }

    void add_observer(IMaterialOptionsObserver* observer) override
    {
        if (!observer) return;
        for (auto* existing : observers_) {
            if (existing == observer) return; // idempotent
        }
        observers_.push_back(observer);
    }

    void remove_observer(IMaterialOptionsObserver* observer) override
    {
        for (auto it = observers_.begin(); it != observers_.end(); ++it) {
            if (*it == observer) {
                observers_.erase(it);
                return;
            }
        }
    }

private:
    vector<IMaterialOptionsObserver*> observers_;
};

} // namespace velk::impl

#endif // VELK_RENDER_MATERIAL_PROPERTY_H
