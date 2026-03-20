#ifndef VELK_UI_LAYOUT_SOLVER_H
#define VELK_UI_LAYOUT_SOLVER_H

#include <velk/api/math_types.h>
#include <velk/interface/intf_hierarchy.h>

namespace velk_ui {

class LayoutSolver
{
public:
    void solve(velk::IHierarchy& hierarchy, const velk::aabb& viewport);

private:
    void solve_element(velk::IHierarchy& hierarchy, const velk::IObject::Ptr& obj,
                       const velk::aabb& parent_bounds, const velk::mat4& parent_world);
};

} // namespace velk_ui

#endif // VELK_UI_LAYOUT_SOLVER_H
