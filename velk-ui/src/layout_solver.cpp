#include "layout_solver.h"

#include <velk/api/state.h>
#include <velk/interface/intf_object_storage.h>

#include <algorithm>
#include <vector>
#include <velk-ui/interface/intf_layout_trait.h>
#include <velk-ui/interface/intf_transform_trait.h>
#include <velk-ui/interface/intf_element.h>

#ifdef VELK_LAYOUT_DEBUG
#define LAYOUT_LOG(...) VELK_LOG(I, __VA_ARGS__)
#else
#define LAYOUT_LOG(...) ((void)0)
#endif

namespace velk_ui {

void LayoutSolver::solve(velk::IHierarchy& hierarchy, const velk::aabb& viewport)
{
    auto root = hierarchy.root();
    if (!root) {
        return;
    }

    solve_element(hierarchy, root, viewport, velk::mat4::identity());
}

void LayoutSolver::solve_element(velk::IHierarchy& hierarchy, const velk::IObject::Ptr& obj,
                                 const velk::aabb& parent_bounds, const velk::mat4& parent_world)
{
    auto* element = interface_cast<IElement>(obj);
    if (!element) {
        return;
    }

    // Collect layout and transform traits
    velk::vector<ILayoutTrait*> layout_traits;
    velk::vector<ITransformTrait*> transform_traits;
    auto* storage = interface_cast<velk::IObjectStorage>(obj);
    if (storage) {
        for (size_t i = 0; i < storage->attachment_count(); ++i) {
            auto att = storage->get_attachment(i);
            auto* lt = interface_cast<ILayoutTrait>(att);
            if (lt) {
                layout_traits.push_back(lt);
            }
            auto* tt = interface_cast<ITransformTrait>(att);
            if (tt) {
                transform_traits.push_back(tt);
            }
        }
    }

    // Sort layout traits: Layout phase first, then Constraint phase
    std::sort(layout_traits.begin(), layout_traits.end(), [](ILayoutTrait* a, ILayoutTrait* b) {
        return static_cast<uint8_t>(a->get_phase()) < static_cast<uint8_t>(b->get_phase());
    });

    // Available space from parent
    Constraint c;
    c.bounds = parent_bounds;

    // Measure pass
    for (auto* lt : layout_traits) {
        c = lt->measure(c, *element, hierarchy);
    }

    // Write size from constraint bounds
    velk::write_state<IElement>(element, [&](IElement::State& s) {
        s.size.width = c.bounds.extent.width;
        s.size.height = c.bounds.extent.height;
    });

    // Apply pass
    for (auto* lt : layout_traits) {
        lt->apply(c, *element, hierarchy);
    }

    // Compute base world matrix from layout position
    auto reader = velk::read_state<IElement>(element);
    if (!reader) {
        return;
    }

    velk::mat4 world = parent_world * velk::mat4::translate(reader->position);
    velk::write_state<IElement>(element, [&](IElement::State& s) { s.world_matrix = world; });

    // Run transform traits
    for (auto* tt : transform_traits) {
        tt->transform(*element);
    }

    // Re-read world matrix after transforms
    reader = velk::read_state<IElement>(element);
    if (!reader) {
        return;
    }
    world = reader->world_matrix;

    // Recurse: each child gets its own allocated bounds (set by this element's Stack)
    auto children = hierarchy.children_of(obj);
    for (auto& child : children) {
        auto* child_elem = interface_cast<IElement>(child);
        if (!child_elem) {
            continue;
        }

        auto child_state = velk::read_state<IElement>(child_elem);
        velk::aabb child_bounds;
        if (child_state && (child_state->size.width > 0.f || child_state->size.height > 0.f)) {
            // Parent's Stack already allocated this child's bounds
            child_bounds.extent.width = child_state->size.width;
            child_bounds.extent.height = child_state->size.height;
        } else {
            // No Stack positioned this child, it fills the parent
            child_bounds.extent.width = reader->size.width;
            child_bounds.extent.height = reader->size.height;
        }

        solve_element(hierarchy, child, child_bounds, world);
    }
}

} // namespace velk_ui
