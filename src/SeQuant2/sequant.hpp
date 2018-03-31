#ifndef SEQUANT2_SEQUANT_H
#define SEQUANT2_SEQUANT_H

#include "vacuum.hpp"

namespace sequant2 {

/// Specifies second quantization context, such as vacuum choice.
class SeQuant2 {

 public:
  /// @param vac the vacuum type
  explicit SeQuant2(Vacuum vac = Vacuum::SingleProduct) : vacuum_(vac) {}
  ~SeQuant2() = default;

  Vacuum vacuum() const { return vacuum_; }

 private:
  Vacuum vacuum_;
};

const SeQuant2 &get_default_context();
void set_default_context(const SeQuant2 &ctx);
void reset_default_context();

}  // namespace sequant2

#endif