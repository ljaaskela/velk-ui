#ifndef VELK_UI_FIXED_SIZE_H
#define VELK_UI_FIXED_SIZE_H

#include <velk-ui/interface/intf_constraint.h>
#include <velk-ui/interface/constraint/intf_fixed_size.h>
#include <velk/ext/object.h>

namespace velk_ui {

class FixedSize : public velk::ext::Object<FixedSize, IConstraint, IFixedSize>
{
public:
    VELK_CLASS_UID("a7f3c1d2-8e4b-4f6a-9c5d-1b2e3f4a5b6c", "FixedSize");

    ConstraintPhase get_phase() const override;
    Constraint measure(const Constraint& c, IElement& element, velk::IHierarchy* hierarchy) override;
    void apply(const Constraint& c, IElement& element, velk::IHierarchy* hierarchy) override;
};

} // namespace velk_ui

#endif // VELK_UI_FIXED_SIZE_H
