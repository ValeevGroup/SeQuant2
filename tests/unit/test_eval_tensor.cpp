//
// created by Bimal Gaudel on Mar 11, 2020
//

#include <clocale>
#include <iostream>
#include <memory>
#include <random>

#include <tiledarray.h>

#include "catch.hpp"

#include <SeQuant/core/tensor_network.hpp>
#include <SeQuant/domain/evaluate/eval_context.hpp>
#include <SeQuant/domain/evaluate/eval_tensor.hpp>
#include <SeQuant/domain/evaluate/eval_tensor_builder.hpp>
#include <SeQuant/domain/evaluate/factorizer.hpp>

using namespace sequant;
using namespace sequant::evaluate;

using DTensorType = TA::TArrayD;
using ContextMapType =
    sequant::container::map<sequant::ExprPtr, std::shared_ptr<DTensorType>>;

// initialize MADWorld
int argc = 1;
char* args[]{};
char** argv = {args};
auto& world = TA::initialize(argc, argv);

// get a sequant Tensor made out of specs
// specs -> {label, b1, ..., b(n/2), k1, ..., k(n/2)}
// eg. {"g", "i_1", "i_2", "a_1", "a_2"}
auto make_tensor_expr =
    [](const sequant::container::svector<std::string>& specs) {
      // only equal bra-ket ranks are expected
      assert((specs.size() > 2) && (specs.size() % 2 != 0));
      std::wstring label = std::wstring(specs[0].begin(), specs[0].end());
      sequant::container::svector<sequant::Index, 4> bra_indices, ket_indices;
      for (auto i = 1; i < specs.size(); ++i) {
        if (i <= specs.size() / 2)
          bra_indices.push_back(
              sequant::Index(std::wstring(specs[i].begin(), specs[i].end())));
        else
          ket_indices.push_back(
              sequant::Index(std::wstring(specs[i].begin(), specs[i].end())));
      }
      return std::make_shared<sequant::Tensor>(label, bra_indices, ket_indices);
    };

// context map builder for evaluating the eval_tensor
auto builder = EvalTensorBuilder<DTensorType>{};

const size_t nocc = 10;
const size_t nvirt = 20;

TA::TiledRange tr_oo{{0, nocc}, {0, nocc}};
TA::TiledRange tr_ov{{0, nocc}, {0, nvirt}};
TA::TiledRange tr_vv{{0, nvirt}, {0, nvirt}};
TA::TiledRange tr_oooo{{0, nocc}, {0, nocc}, {0, nocc}, {0, nocc}};
TA::TiledRange tr_ooov{{0, nocc}, {0, nocc}, {0, nocc}, {0, nvirt}};
TA::TiledRange tr_oovv{{0, nocc}, {0, nocc}, {0, nvirt}, {0, nvirt}};
TA::TiledRange tr_ovov{{0, nocc}, {0, nvirt}, {0, nocc}, {0, nvirt}};
TA::TiledRange tr_ovvv{{0, nocc}, {0, nvirt}, {0, nvirt}, {0, nvirt}};
TA::TiledRange tr_vvvv{{0, nvirt}, {0, nvirt}, {0, nvirt}, {0, nvirt}};
//
TA::TiledRange tr_ovvo{{0, nocc}, {0, nvirt}, {0, nvirt}, {0, nocc}};
TA::TiledRange tr_voov{{0, nvirt}, {0, nocc}, {0, nocc}, {0, nvirt}};

TEST_CASE("Eval_Tensor_CONSTRUCTOR_TESTS", "[eval_tensor]") {
  SECTION("Intermediate construction") {
    EvalTensorIntermediate<DTensorType> evt_imed;
    REQUIRE(!evt_imed.is_leaf());
    REQUIRE(evt_imed.get_operation() == Operation::INVALID);
    REQUIRE(evt_imed.get_left_tensor() == nullptr);
    REQUIRE(evt_imed.get_right_tensor() == nullptr);
  }

  SECTION("Leaf construction") {
    EvalTensorLeaf<DTensorType> evt_leaf(nullptr);
    REQUIRE(evt_leaf.is_leaf());
  }
}

TEST_CASE("EVAL_TENSOR_EVALUATE_TESTS", "[eval_tensor_builder]") {
  // creating some random tensors
  auto T_ov = std::make_shared<DTensorType>(world, tr_ov);
  auto T_oovv = std::make_shared<DTensorType>(world, tr_oovv);
  auto G_oovv = std::make_shared<DTensorType>(world, tr_oovv);
  //
  auto G_ovvo = std::make_shared<DTensorType>(world, tr_ovvo);
  auto G_voov = std::make_shared<DTensorType>(world, tr_voov);
  //

  T_ov->fill_random();
  T_oovv->fill_random();
  G_oovv->fill_random();
  G_ovvo->fill_random();

  SECTION("Testing Sum type evaluation") {
    auto t = make_tensor_expr({"t", "i_1", "i_2", "a_1", "a_2"});
    auto g = make_tensor_expr({"g", "i_1", "i_2", "a_1", "a_2"});

    DTensorType manual_sum;
    manual_sum("i, j, a, b") =
        (*G_oovv)("i, j, a, b") + (*T_oovv)("i, j, a, b");

    ContextMapType context;
    context.insert(ContextMapType::value_type(t, T_oovv));
    context.insert(ContextMapType::value_type(g, G_oovv));
    auto ev_context = EvalContext(context, builder);

    auto expr = std::make_shared<Sum>(Sum({g, t}));
    auto tree = builder.build_tree(expr);
    auto eval_sum = tree->evaluate(ev_context.get_map());

    auto manual_norm =
        std::sqrt(manual_sum("i,j,a,b").dot(manual_sum("i,j,a,b")));

    auto eval_norm = std::sqrt(eval_sum("i,j,a,b").dot(eval_sum("i,j,a,b")));

    REQUIRE(manual_norm == Approx(eval_norm));

    // sum by permutation test
    t = make_tensor_expr({"t", "i_1", "i_2", "a_1", "a_2"});
    g = make_tensor_expr({"g", "i_1", "i_2", "a_2", "a_1"});
    context.clear();
    context.insert(ContextMapType::value_type(t, T_oovv));
    context.insert(ContextMapType::value_type(g, G_oovv));
    ev_context = EvalContext(context, builder);
    expr = std::make_shared<Sum>(Sum({g, t}));
    tree = builder.build_tree(expr);

    eval_sum = tree->evaluate(ev_context.get_map());

    manual_sum("i, j, a, b") =
        (*G_oovv)("i, j, a, b") + (*T_oovv)("i, j, b, a");
    manual_norm = std::sqrt(manual_sum("i,j,a,b").dot(manual_sum("i,j,a,b")));

    eval_norm = std::sqrt(eval_sum("i,j,a,b").dot(eval_sum("i,j,a,b")));

    REQUIRE(manual_norm == Approx(eval_norm));
  }

  SECTION("Testing Product type evaluation") {
    auto t = make_tensor_expr({"t", "i_1", "a_1"});
    auto g1 = make_tensor_expr({"g", "i_1", "i_2", "a_1", "a_2"});

    DTensorType manual_prod;
    manual_prod("j,b") = (*T_ov)("i,a") * (*G_oovv)("i,j,a,b");

    ContextMapType context;
    context.insert(ContextMapType::value_type(t, T_ov));
    context.insert(ContextMapType::value_type(g1, G_oovv));
    auto ev_context = EvalContext(context, builder);

    auto expr = std::make_shared<Product>(Product({t, g1}));
    auto tree = builder.build_tree(expr);
    auto eval_prod = tree->evaluate(ev_context.get_map());

    auto manual_norm = std::sqrt(manual_prod("j,b").dot(manual_prod("j,b")));

    auto eval_norm = std::sqrt(eval_prod("j,b").dot(eval_prod("j,b")));

    REQUIRE(manual_norm == Approx(eval_norm));
  }

  SECTION("Testing antisymmetrization evaluation") {
    auto t = make_tensor_expr({"t", "i_1", "i_2", "a_1", "a_2"});
    auto A = make_tensor_expr({"A", "i_1", "i_2", "a_1", "a_2"});

    ContextMapType context;
    context.insert(ContextMapType::value_type(t, T_oovv));
    auto ev_context = EvalContext(context, builder);

    DTensorType manual_result;
    manual_result("i,j,a,b") = (*T_oovv)("i,j,a,b") - (*T_oovv)("i,j,b,a") +
                               (*T_oovv)("j,i,b,a") - (*T_oovv)("j,i,a,b");

    auto manual_norm =
        std::sqrt(manual_result("i,j,a,b").dot(manual_result("i,j,a,b")));

    auto expr = std::make_shared<Product>(Product({A, t}));
    auto tree = builder.build_tree(expr);
    auto eval_result = tree->evaluate(ev_context.get_map());

    auto eval_norm =
        std::sqrt(eval_result("i,j,a,b").dot(eval_result("i,j,a,b")));

    REQUIRE(manual_norm == Approx(eval_norm));
  }

  SECTION("Testing missing data tensor") {
    auto seq_tensor_bad = make_tensor_expr({"t", "a_1", "i_1", "a_2", "a_3"});
    auto seq_tensor_good = make_tensor_expr({"t", "i_1", "a_1", "a_2", "a_3"});
    ContextMapType context;
    context.insert(ContextMapType::value_type(seq_tensor_good, T_ov));
    auto ev_context = EvalContext(context, builder);

    auto expr = seq_tensor_bad;
    auto tree = builder.build_tree(expr);

    REQUIRE_THROWS_AS(tree->evaluate(ev_context.get_map()), std::logic_error);
  }

  // SECTION("Bra and ket indices") {
  //  auto visitor = [](const EvalTensor<DTensorType>& evtensor) {
  //    std::wcout << "Indices: ";
  //    for (const auto& idx : evtensor.get_indices())
  //      std::wcout << idx.to_latex();
  //    std::wcout << std::endl;
  //  };
  //  auto t = make_tensor_expr({"t", "i_1", "i_2", "a_1", "a_2"});
  //  auto g = make_tensor_expr({"g", "i_1", "i_2", "a_1", "a_2"});
  //  auto expr = std::make_shared<Sum>(Sum({g, t}));
  //  auto tree = builder.build_tree(expr);
  //  tree->visit(visitor);
  //  std::wcout << "digraph G {\n";
  //  std::wcout << tree->to_digraph();
  //  std::wcout << "}\n";
  //}
}

TEST_CASE("FACTORIZER_TESTS", "[factorizer]") {
  container::map<IndexSpace::TypeAttr, size_t> space_size;
  space_size.insert(
      decltype(space_size)::value_type(IndexSpace::active_occupied, nocc));
  space_size.insert(
      decltype(space_size)::value_type(IndexSpace::active_unoccupied, nvirt));

  SECTION("Testing operation counts for product") {
    auto t = make_tensor_expr({"t", "i_1", "a_1"});
    auto g = make_tensor_expr({"g", "i_1", "i_2", "a_1", "a_2"});
    auto expr = std::make_shared<Product>(Product({t, g}));
    auto tree = builder.build_tree(expr);

    tree->set_ops_count(space_size);

    // by looking at 't' and 'g' tensors
    // we see the ops count should be
    // (num of active_occupied squared) * (num of active_unoccupied squared)

    REQUIRE(tree->get_ops_count() == nocc * nocc * nvirt * nvirt);
  }

  SECTION("Testing operation counts for sum") {
    auto t = make_tensor_expr({"t", "i_1", "a_1"});
    auto f = make_tensor_expr({"f", "i_2", "a_2"});
    auto g = make_tensor_expr({"g", "i_1", "i_2", "a_1", "a_2"});

    auto left_sumand = std::make_shared<Product>(Product{t, f});
    auto& right_sumand = g;

    auto expr = std::make_shared<Sum>(Sum{left_sumand, right_sumand});

    auto tree = builder.build_tree(expr);

    tree->set_ops_count(space_size);

    REQUIRE(tree->get_ops_count() == nocc * nocc * nvirt * nvirt);

    // auto ops_printer = [](const EvalTensor<DTensorType>& node) {
    //   std::wcout << "ops count = " << node.get_ops_count() << "\n";
    // };
    // std::wcout << "Digraph G{\n" << tree->to_digraph() << "}\n";
    // tree->visit(ops_printer);
  }

  SECTION("Testing largest common subfactor: Product") {
    // forming two tensor products
    auto prodA = std::make_shared<Product>(
        Product({make_tensor_expr({"t", "i_1", "i_2", "a_1", "a_2"}),
                 make_tensor_expr({"g", "i_1", "i_3", "a_1", "a_3"}),
                 make_tensor_expr({"f", "i_2", "a_2"})}));

    auto prodB = std::make_shared<Product>(
        Product({make_tensor_expr({"t", "i_4", "i_6", "a_4", "a_6"}),
                 make_tensor_expr({"g", "i_4", "i_8", "a_4", "a_8"}),
                 make_tensor_expr({"f", "i_8", "a_8"})}));
    // Note how each tensor in prodA is equivalent to a tensor in prodB
    // (equivalent in the sense that both represent the same data tensor)
    // however, prodA and prodB are not equivalent products
    // that is beacuse even though, the nodes of the two tensor networks (prodA
    // and prodB) are the same, their edges differ.
    //
    // The 't' and 'g' tensors are labelled in such a way that the network of
    // t<-->g in both products have the same edges, and thus t<-->g is the
    // subtensor network common to prodA and prodB.

    auto [subfactorA, subfactorB] =
        largest_common_subnet(prodA, prodB, builder);
    REQUIRE(subfactorA == container::svector<size_t>{0, 1});
    REQUIRE(subfactorB == container::svector<size_t>{0, 1});

    // prodC is the same as prodB except the position of 'f' and 'g' tensors are
    // swapped this shows that absolute order of the constiuents that make up
    // the common subnet in the pair of tensor networks
    // doesn't need to be the same
    auto prodC = std::make_shared<Product>(
        Product({make_tensor_expr({"t", "i_4", "i_6", "a_4", "a_6"}),
                 make_tensor_expr({"f", "i_8", "a_8"}),
                 make_tensor_expr({"g", "i_4", "i_8", "a_4", "a_8"})}));
    auto [subfactorX, subfactorY] =
        largest_common_subnet(prodA, prodC, builder);
    REQUIRE(subfactorX == container::svector<size_t>{0, 1});
    REQUIRE(subfactorY == container::svector<size_t>{0, 2});

    // lets completely reverse the order of the tensors in prodB
    // shows that the relative order of the constiuents of the common subnet
    // doesn't matter
    prodC = std::make_shared<Product>(
        Product({make_tensor_expr({"f", "i_8", "a_8"}),
                 make_tensor_expr({"g", "i_4", "i_8", "a_4", "a_8"}),
                 make_tensor_expr({"t", "i_4", "i_6", "a_4", "a_6"})}));

    auto [subfactorXX, subfactorYY] =
        largest_common_subnet(prodA, prodC, builder);
    REQUIRE(subfactorXX == container::svector<size_t>{0, 1});
    REQUIRE(subfactorYY == container::svector<size_t>{2, 1});

    // no common subnet
    prodA = std::make_shared<Product>(
        Product({make_tensor_expr({"f", "i_8", "a_8"})}));
    prodB = std::make_shared<Product>(
        Product({make_tensor_expr({"g", "i_4", "i_8", "a_4", "a_8"})}));

    auto [subfactorA_null, subfactorB_null] =
        largest_common_subnet(prodA, prodB, builder);

    REQUIRE(subfactorA_null.empty());
    REQUIRE(subfactorB_null.empty());
  }

  SECTION("Testing largest common subfactor: Product (v2)") {
    // forming two tensor products
    auto prodA = std::make_shared<Product>(
        Product({make_tensor_expr({"t", "i_1", "i_2", "a_1", "a_2"}),
                 make_tensor_expr({"g", "i_3", "i_4", "a_2", "a_4"}),
                 make_tensor_expr({"t", "i_3", "i_4", "a_3", "a_4"})}));

    auto prodB = std::make_shared<Product>(
        Product({make_tensor_expr({"t", "i_3", "i_4", "a_3", "a_4"}),
                 make_tensor_expr({"g", "i_3", "i_4", "a_2", "a_4"}),
                 make_tensor_expr({"t", "i_1", "i_2", "a_1", "a_2"})}));
    // Note how each tensor in prodA is equivalent to a tensor in prodB
    // (equivalent in the sense that both represent the same data tensor)
    // however, prodA and prodB are not equivalent products
    // that is beacuse even though, the nodes of the two tensor networks (prodA
    // and prodB) are the same, their edges differ.
    //
    // The 't' and 'g' tensors are labelled in such a way that the network of
    // t<-->g in both products have the same edges, and thus t<-->g is the
    // subtensor network common to prodA and prodB.

    // auto [subfactorA, subfactorB] = largest_common_subnet(prodA, prodB,
    // builder); REQUIRE(subfactorA == container::svector<size_t>{0, 1, 2});
    // REQUIRE(subfactorB == container::svector<size_t>{2, 1, 0});

    auto tnA = TensorNetwork(*prodA);
    auto tnB = TensorNetwork(*prodB);

    tnA.canonicalize(TensorCanonicalizer::cardinal_tensor_labels(), true);
    // tnB.canonicalize(TensorCanonicalizer::cardinal_tensor_labels(), false);

    /* std::wcout << "canonicalized prodA..\n"; */
    /* for (const auto& tnsr: tnA.tensors()) */
    /*     std::wcout << std::dynamic_pointer_cast<Expr>(tnsr)->to_latex() << std::endl; */

    /* std::wcout << "canonicalized prodB..\n"; */
    /* for (const auto& tnsr: tnB.tensors()) */
    /*     std::wcout << std::dynamic_pointer_cast<Expr>(tnsr)->to_latex() << std::endl; */
  }

  SECTION("Testing largest common subfactor: Sum") {
    // forming two sums
    auto sumA = std::make_shared<Sum>(
        Sum({make_tensor_expr({"t", "i_1", "i_2", "a_1", "a_2"}),
             make_tensor_expr({"g", "i_1", "i_2", "a_1", "a_4"})}));

    auto sumB = std::make_shared<Sum>(
        Sum({make_tensor_expr({"t", "i_4", "i_6", "a_4", "a_6"}),
             make_tensor_expr({"g", "i_4", "i_6", "a_4", "a_6"})}));

    auto [summandA, summandB] = largest_common_subnet(sumA, sumB, builder);

    REQUIRE(summandA == container::svector<size_t>{0, 1});
    REQUIRE(summandB == container::svector<size_t>{0, 1});

    // forming sums whose summands are products
    auto prod1 = std::make_shared<Product>(
        Product({make_tensor_expr({"t", "i_1", "i_2", "a_1", "a_2"}),
                 make_tensor_expr({"g", "i_1", "i_3", "a_1", "a_3"}),
                 make_tensor_expr({"f", "i_2", "a_2"})}));

    auto prod2 = std::make_shared<Product>(
        Product({make_tensor_expr({"f", "i_3", "a_3"})}));

    auto prod3 = std::make_shared<Product>(
        Product({make_tensor_expr({"t", "i_2", "i_3", "a_2", "a_3"}),
                 make_tensor_expr({"f", "i_2", "a_2"})}));

    auto prod4 = std::make_shared<Product>(
        Product({make_tensor_expr({"g", "i_2", "i_3", "a_2", "a_3"}),
                 make_tensor_expr({"t", "i_2", "a_2"})}));

    sumA = std::make_shared<Sum>(Sum({prod1, prod2, prod3}));
    sumB = std::make_shared<Sum>(Sum({prod1, prod2, prod4}));

    auto expectedSubNet = std::make_shared<Sum>(Sum({prod1, prod2}));

    auto [summandAA, summandBB] = largest_common_subnet(sumA, sumB, builder);

    REQUIRE(summandAA == container::svector<size_t>{0, 1});
    REQUIRE(summandBB == container::svector<size_t>{0, 1});

    // no common subnet
    sumA = std::make_shared<Sum>(Sum({prod1, prod2}));
    sumB = std::make_shared<Sum>(Sum({prod3, prod4}));

    auto [summandAnull, summandBnull] =
        largest_common_subnet(sumA, sumB, builder);

    REQUIRE(summandAnull.empty());
    REQUIRE(summandBnull.empty());
  }
}

TEST_CASE("EVAL_TENSOR_BUILDER_TESTS", "[eval_tensor_builder]") {
  using DTensorType = TA::TArrayD;

  // complex data false
  auto real_builder = EvalTensorBuilder<TA::TArrayD>(false);

  // complex data true
  auto complex_builder = EvalTensorBuilder<TA::TArrayD>(true);

  SECTION("Index label agnostic hashing") {
    auto t1 = std::make_shared<Tensor>(
        Tensor(L"t", {L"i_1", L"i_2"}, {L"a_1", L"a_2"}));

    auto t2 = std::make_shared<Tensor>(
        Tensor(L"t", {L"i_10", L"i_11"}, {L"a_11", L"a_12"}));

    REQUIRE(real_builder.build_tree(t1)->get_hash_value() ==
            real_builder.build_tree(t2)->get_hash_value());

    REQUIRE(complex_builder.build_tree(t1)->get_hash_value() ==
            complex_builder.build_tree(t2)->get_hash_value());
  }

  SECTION("Non-symmetric brakets") {
    auto t1 = std::make_shared<Tensor>(
        Tensor(L"t", {L"i_1", L"i_2"}, {L"a_1", L"a_2"}, Symmetry::nonsymm,
               BraKetSymmetry::nonsymm));

    auto t2 = std::make_shared<Tensor>(
        Tensor(L"t", {L"a_1", L"a_2"}, {L"i_1", L"i_2"}, Symmetry::nonsymm,
               BraKetSymmetry::nonsymm));

    REQUIRE(real_builder.build_tree(t1)->get_hash_value() !=
            real_builder.build_tree(t2)->get_hash_value());

    REQUIRE(complex_builder.build_tree(t1)->get_hash_value() !=
            complex_builder.build_tree(t2)->get_hash_value());
  }

  SECTION("Symmetric brakets") {
    auto t1 = std::make_shared<Tensor>(
        Tensor(L"t", {L"i_1", L"i_2"}, {L"a_1", L"a_2"}, Symmetry::nonsymm,
               BraKetSymmetry::symm));

    auto t2 = std::make_shared<Tensor>(
        Tensor(L"t", {L"a_1", L"a_2"}, {L"i_1", L"i_2"}, Symmetry::nonsymm,
               BraKetSymmetry::symm));

    REQUIRE(real_builder.build_tree(t1)->get_hash_value() ==
            real_builder.build_tree(t2)->get_hash_value());

    REQUIRE(complex_builder.build_tree(t1)->get_hash_value() ==
            complex_builder.build_tree(t2)->get_hash_value());
  }

  SECTION("Conjugate brakets") {
    auto t1 = std::make_shared<Tensor>(
        Tensor(L"t", {L"i_1", L"i_2"}, {L"a_1", L"a_2"}, Symmetry::nonsymm,
               BraKetSymmetry::conjugate));

    auto t2 = std::make_shared<Tensor>(
        Tensor(L"t", {L"a_1", L"a_2"}, {L"i_1", L"i_2"}, Symmetry::nonsymm,
               BraKetSymmetry::conjugate));

    REQUIRE(real_builder.build_tree(t1)->get_hash_value() ==
            real_builder.build_tree(t2)->get_hash_value());

    REQUIRE(complex_builder.build_tree(t1)->get_hash_value() !=
            complex_builder.build_tree(t2)->get_hash_value());
  }
}
