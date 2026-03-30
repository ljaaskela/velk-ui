#ifndef VELK_UI_API_TRAIT_MATRIX_H
#define VELK_UI_API_TRAIT_MATRIX_H

#include <velk/api/state.h>

#include <velk-ui/api/trait.h>
#include <velk-ui/interface/trait/intf_matrix.h>
#include <velk-ui/plugin.h>

namespace velk_ui {

/**
 * @brief Convenience wrapper around IMatrix.
 *
 *   auto mtx = transform::create_matrix();
 *   mtx.set_matrix(velk::mat4::scale({2.f, 2.f, 1.f}));
 *   elem.add_trait(mtx);
 */
class Matrix : public Trait
{
public:
    Matrix() = default;
    explicit Matrix(velk::IObject::Ptr obj) : Trait(check_object<IMatrix>(obj)) {}
    explicit Matrix(IMatrix::Ptr m) : Trait(velk::as_object(m)) {}

    operator IMatrix::Ptr() const { return as_ptr<IMatrix>(); }

    auto get_matrix() const { return read_state_value<IMatrix>(&IMatrix::State::matrix); }
    void set_matrix(const velk::mat4& v) { write_state_value<IMatrix>(&IMatrix::State::matrix, v); }
};

namespace transform {

/** @brief Creates a new Matrix transform trait. */
inline Matrix create_matrix()
{
    return Matrix(velk::instance().create<IMatrix>(ClassId::Transform::Matrix));
}

} // namespace transform

} // namespace velk_ui

#endif // VELK_UI_API_TRAIT_MATRIX_H
