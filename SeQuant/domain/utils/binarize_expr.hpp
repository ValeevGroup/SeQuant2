#ifndef SEQUANT_UTILS_BINARIZE_EXPR_HPP
#define SEQUANT_UTILS_BINARIZE_EXPR_HPP

#include "binary_expr.hpp"
#include "eval_expr.hpp"
#include "eval_sequence.hpp"

#include <iostream>

namespace sequant::utils {

const struct {
 public:
  eval_expr operator()(const eval_expr& x) const { return x; }

  eval_expr operator()(const eval_expr& x, const eval_expr& y) const {
    return eval_expr{x, y};
  }
} binarize_eval_expr;

struct binarize_flat_prod {
  binarize_flat_prod(const Product& p) : prod{p} {
    for (const auto& f : p) assert(f->is<Tensor>());
  }

  binary_expr<eval_expr>::node_ptr operator()(
      const eval_sequence<size_t>& seq) {
    auto xpr_seq = transform_eval_sequence<size_t>(seq, [this](auto x) {
      return eval_expr{prod.at(x)->template as<Tensor>()};
    });

    auto result =
        binarize_eval_sequence<eval_expr>(xpr_seq, binarize_eval_expr);

    result->data().scale(Constant{result->data().scalar().value() *
                                  result->data().phase().value() *
                                  prod.scalar()});

    return result;
  }

 private:
  const Product& prod;
};

/**
 * @tparam Cont type of a container.
 *
 * @param container Cont type container of eval_expr objects.
 */
template <typename Cont>
binary_expr<eval_expr>::node_ptr binarize_evxpr_range(Cont&& container) {
  const auto eseq = eval_sequence<eval_expr>{
      (*ranges::begin(container)),
      ranges::views::tail(container) |
          ranges::views::transform(
              [](const auto& x) { return eval_sequence<eval_expr>{x}; }) |
          ranges::to<std::vector<eval_sequence<eval_expr>>>};

  return binarize_eval_sequence<eval_expr>(eseq, binarize_eval_expr);
}

ExprPtr debinarize_eval_expr(binary_expr<eval_expr>::node_ptr const& node);

}  // namespace sequant::utils

#endif  // SEQUANT_UTILS_BINARIZE_EXPR_HPP