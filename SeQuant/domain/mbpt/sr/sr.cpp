//
// Created by Eduard Valeyev on 2019-02-19.
//

#include "sr.hpp"

#include "SeQuant/core/expr.hpp"
#include "SeQuant/core/op.hpp"
#include "SeQuant/core/tensor.hpp"
#include "SeQuant/core/wick.hpp"

namespace sequant {
namespace mbpt {
namespace sr {
namespace so {

inline constexpr size_t fac(std::size_t n) {
  if (n == 1 || n == 0)
    return 1;
  else
    return n * fac(n - 1);
}

make_op::make_op(std::size_t nbra, std::size_t nket, OpType op, bool csv) :
      nbra_(nbra), nket_(nket), op_(op), csv_(csv) {}

ExprPtr make_op::operator()(bool complete_unoccupieds, bool antisymm) const {
  const auto unocc = complete_unoccupieds ? IndexSpace::complete_unoccupied
                                          : IndexSpace::active_unoccupied;
  const auto occ = IndexSpace::active_occupied;
  return (*this)(unocc, occ, antisymm);
}

ExprPtr make_op::operator()(IndexSpace::Type unocc, IndexSpace::Type occ, bool antisymm) const {
  // not sure what it means to use nonsymmetric operator if nbra != nket
  if (!antisymm)
    assert(nbra_ == nket_);

  const auto nbra = nbra_;
  const auto nket = nket_;
  const auto csv = csv_;
  OpType op = op_;
  auto make_idx_vector = [](size_t n, IndexSpace::Type spacetype) {
    auto space = IndexSpace::instance(spacetype);
    std::vector<Index> result;
    result.reserve(n);
    for (size_t i = 0; i != n; ++i) {
      result.push_back(Index::make_tmp_index(space));
    }
    return result;
  };
  auto make_depidx_vector = [](size_t n, IndexSpace::Type spacetype,
                                 auto&& protoidxs) {
    auto space = IndexSpace::instance(spacetype);
    std::vector<Index> result;
    result.reserve(n);
    for (size_t i = 0; i != n; ++i) {
      result.push_back(Index::make_tmp_index(space, protoidxs, true));
    }
    return result;
  };
  std::vector<Index> braidxs;
  std::vector<Index> ketidxs;
  if (to_class(op) == OpClass::gen) {
    braidxs = make_idx_vector(nbra, IndexSpace::complete);
    ketidxs = make_idx_vector(nket, IndexSpace::complete);
  } else {
    auto make_occidxs = [&make_idx_vector, &occ](size_t n) {
      return make_idx_vector(n, occ);
    };
    auto make_uoccidxs = [csv, &unocc, &make_idx_vector,
                          &make_depidx_vector](size_t n, auto&& occidxs) {
      return csv ? make_depidx_vector(n, unocc, occidxs)
                 : make_idx_vector(n, unocc);
    };
    if (to_class(op) == OpClass::ex) {
      ketidxs = make_occidxs(nket);
      braidxs = make_uoccidxs(nbra, ketidxs);
    } else {
      braidxs = make_occidxs(nbra);
      ketidxs = make_uoccidxs(nket, braidxs);
    }
  }
  const auto mult = antisymm ? fac(nbra) * fac(nket) : fac(nbra);
  const auto opsymm = antisymm ? Symmetry::antisymm : Symmetry::nonsymm;
  return ex<Constant>(1. / mult) *
         ex<Tensor>(to_wstring(op), braidxs, ketidxs, opsymm) *
         ex<FNOperator>(braidxs, ketidxs, Vacuum::SingleProduct);
}

make_op Op(OpType _Op, std::size_t Nbra, std::size_t Nket) {
  assert(Nbra < std::numeric_limits<std::size_t>::max());
  const auto Nket_ = Nket == std::numeric_limits<std::size_t>::max() ? Nbra : Nket;
  assert(Nbra > 0 || Nket_ > 0);
  return make_op{Nbra, Nket_, _Op, false};
}

#include "sr_op.impl.cpp"

ExprPtr H1() {
  return get_default_context().vacuum() == Vacuum::Physical ? Op(OpType::h, 1)(false,false) : Op(OpType::f, 1)(false,false);
}

ExprPtr H2(bool antisymm) {
  return Op(OpType::g, 2)(false, antisymm);
}

ExprPtr H0mp() {
  assert(get_default_context().vacuum() == Vacuum::SingleProduct);
  return H1();
}

ExprPtr H1mp(bool antisymm) {
  assert(get_default_context().vacuum() == Vacuum::SingleProduct);
  return H2(antisymm);
}

ExprPtr F() {
  return Op(OpType::f, 1)(false,false);
}

ExprPtr W(bool antisymm) {
  assert(get_default_context().vacuum() == Vacuum::SingleProduct);
  return H1mp(antisymm);
}

ExprPtr H(bool antisymm) {
  return H1() + H2(antisymm);
}

ExprPtr vac_av(ExprPtr expr, std::initializer_list<std::pair<int,int>> op_connections, bool use_top) {
  FWickTheorem wick{expr};
  wick.spinfree(false)
      .use_topology(use_top)
      .set_op_connections(op_connections);
  auto result = wick.compute();
  simplify(result);
  if (Logger::get_instance().wick_stats) {
    std::wcout << "WickTheorem stats: # of contractions attempted = "
               << wick.stats().num_attempted_contractions
               << " # of useful contractions = "
               << wick.stats().num_useful_contractions << std::endl;
  }
  return result;
}

namespace csv {

make_op Op(OpType _Op, std::size_t Nbra, std::size_t Nket) {
  assert(Nbra > 0 && Nbra < std::numeric_limits<std::size_t>::max());
  const auto Nket_ = Nket == std::numeric_limits<std::size_t>::max() ? Nbra : Nket;
  assert(Nket_ > 0);
  return make_op{Nbra, Nket_, _Op, true};
}

#include "sr_op.impl.cpp"

using sequant::mbpt::sr::so::H;
using sequant::mbpt::sr::so::H0mp;
using sequant::mbpt::sr::so::H1;
using sequant::mbpt::sr::so::H1mp;
using sequant::mbpt::sr::so::H2;
using sequant::mbpt::sr::so::vac_av;

}

}  // namespace so
}  // namespace sr
}  // namespace mbpt
}  // namespace sequant