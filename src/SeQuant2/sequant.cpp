#include "sequant.hpp"

namespace sequant2 {

namespace detail {

SeQuant2 &default_context_instance() {
  static SeQuant2 instance_;
  return instance_;
}

}  // anonymous namespace

const SeQuant2 &get_default_context() {
  return detail::default_context_instance();
}

void set_default_context(const SeQuant2 &ctx) {
  detail::default_context_instance() = ctx;
}

void reset_default_context() {
  detail::default_context_instance() = SeQuant2{};
}

}  // namespace sequant2