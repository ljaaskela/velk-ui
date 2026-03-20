#ifndef VELK_UI_STACK_H
#define VELK_UI_STACK_H

#include <velk-ui/interface/intf_constraint.h>
#include <velk-ui/interface/constraint/intf_stack.h>
#include <velk/ext/object.h>

namespace velk_ui {

class Stack : public velk::ext::Object<Stack, IConstraint, IStack>
{
public:
    VELK_CLASS_UID("b8e4d2f3-9a5c-4e7b-8d6f-2c3e4a5b6d7e", "Stack");

    ConstraintPhase get_phase() const override;
    Constraint measure(const Constraint& c, IElement& element, velk::IHierarchy* hierarchy) override;
    void apply(const Constraint& c, IElement& element, velk::IHierarchy* hierarchy) override;
};

} // namespace velk_ui

#endif // VELK_UI_STACK_H
