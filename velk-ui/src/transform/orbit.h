#ifndef VELK_UI_ORBIT_TRANSFORM_H
#define VELK_UI_ORBIT_TRANSFORM_H

#include <velk/api/change.h>
#include <velk-ui/ext/trait.h>
#include <velk-ui/interface/trait/intf_orbit.h>
#include <velk-ui/plugin.h>

namespace velk::ui {

class Orbit : public ext::Transform<Orbit, IOrbit>
{
public:
    VELK_CLASS_UID(ClassId::Transform::Orbit, "Orbit");

    void transform(IElement& element) override;

private:
    struct CacheKey
    {
        IElement* target{};
        vec3 target_pos{};
        size target_size{};
        float yaw{}, pitch{}, distance{};

        bool operator==(const CacheKey& o) const
        {
            return target == o.target
                && target_pos == o.target_pos
                && target_size == o.target_size
                && yaw == o.yaw && pitch == o.pitch && distance == o.distance;
        }
        bool operator!=(const CacheKey& o) const { return !(*this == o); }
    };
    ChangeCache<CacheKey> cache_;
};

} // namespace velk::ui

#endif // VELK_UI_ORBIT_TRANSFORM_H
