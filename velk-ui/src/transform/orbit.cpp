#include "orbit.h"

#include <velk/api/state.h>
#include <velk-ui/interface/intf_element.h>

#include <cmath>

namespace velk::ui {

void Orbit::transform(IElement& element)
{
    auto state = read_state<IOrbit>(this);
    if (!state) {
        return;
    }
    auto target_obj = state->target.get<IElement>();
    auto target_state = read_state<IElement>(target_obj);
    if (!target_state) {
        cache_.invalidate();
        return;
    }

    CacheKey key{target_obj.get(),
                 {target_state->world_matrix(0, 3),
                  target_state->world_matrix(1, 3),
                  target_state->world_matrix(2, 3)},
                 target_state->size,
                 state->yaw,
                 state->pitch,
                 state->distance};
    if (!cache_.changed(key)) {
        return;
    }

    // Target position (center of target element)
    vec3 target{
        target_state->world_matrix(0, 3) + target_state->size.width * 0.5f,
        target_state->world_matrix(1, 3) + target_state->size.height * 0.5f,
        0.f
    };

    // Spherical to cartesian offset from target
    float yaw = deg_to_rad(state->yaw);
    float pitch = deg_to_rad(state->pitch);
    float d = state->distance;
    float cp = std::cos(pitch);

    vec3 eye{
        target.x + d * std::sin(yaw) * cp,
        target.y + d * std::sin(pitch),
        target.z + d * std::cos(yaw) * cp
    };

    // Forward = normalize(target - eye)
    vec3 fwd = target - eye;
    if (vec3::is_zero(fwd)) {
        return;
    }
    fwd = vec3::normalize(fwd);

    // Right = normalize(forward x up), up = (0, 1, 0)
    vec3 right = vec3::cross(fwd, vec3::unit_y());
    if (vec3::is_zero(right)) {
        return;
    }
    right = vec3::normalize(right);

    // Recompute up = right x forward
    vec3 up = vec3::cross(right, fwd);

    // Write world matrix: columns are right, up, -forward, position
    write_state<IElement>(&element, [&](IElement::State& es) {
        es.world_matrix.set_col(0, right);
        es.world_matrix.set_col(1, up);
        es.world_matrix.set_col(2, -fwd);
        es.world_matrix.set_col(3, eye);
    });
}

} // namespace velk::ui
