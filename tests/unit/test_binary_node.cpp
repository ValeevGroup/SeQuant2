#include "catch.hpp"

#include <SeQuant/core/binary_node.hpp>
#include <array>
#include <range/v3/view.hpp>
#include <string>

TEST_CASE("TEST BINARY_NODE", "[BinaryNode]") {
  using ranges::views::iota;
  using ranges::views::take;
  using sequant::BinaryNode;

  SECTION("construction") {
    REQUIRE_NOTHROW(BinaryNode{0});
    REQUIRE_NOTHROW(BinaryNode{'a', 'b', 'c'});
    REQUIRE_NOTHROW(BinaryNode{'a', BinaryNode{'b'}, BinaryNode{'c'}});
  }

  SECTION("derefence") {
    auto const n1 = BinaryNode{100};

    REQUIRE_NOTHROW(*n1);
    REQUIRE(*n1 == 100);

    struct dummy {
      void dummy_fun() const {}
    };

    auto const n2 = BinaryNode{dummy{}};

    REQUIRE_NOTHROW(n2->dummy_fun());
  }

  SECTION("internal node") {
    auto const n = BinaryNode{3, 2, 5};

    REQUIRE_FALSE(n.leaf());
    REQUIRE(*n == 3);
    REQUIRE(*n.left() == 2);
    REQUIRE(*n.right() == 5);
  }

  SECTION("leaf node") {
    auto const n = BinaryNode{'n'};
    REQUIRE(*n == 'n');
    REQUIRE(n.leaf());

    REQUIRE_THROWS(n.left());
    REQUIRE_THROWS(n.right());
  }

  SECTION("advanced construction") {
    auto constexpr leaves = std::array{3, 2, 4};

    struct adder {
      int operator()(int x) const { return x; }
      int operator()(int x, int y) const { return x + y; }
    };

    auto const node = BinaryNode<int>{leaves, adder{}};

    REQUIRE(*node == 9);
    REQUIRE(*node.left() == 5);
    REQUIRE(*node.right() == 4);
    REQUIRE(*node.left().left() == 3);
    REQUIRE(*node.left().right() == 2);

    auto const leaves2 = ranges::views::all(leaves);
    auto const node2 = BinaryNode<int>{leaves2, adder{}};
  }

  SECTION("evaluation") {
    // doing simple arithmetic in a complicated way
    struct arithm_val {
      enum struct arithm_type { Id, Sum };

      int val{0};

      arithm_type arithm{arithm_type::Sum};

      arithm_val() = default;

      arithm_val(int x) : val{x}, arithm{arithm_type::Id} {}
    };

    struct arithm_binarizer {
      arithm_val operator()(arithm_val const& av) {
        assert(av.arithm == arithm_val::arithm_type::Id);
        return av;
      }

      arithm_val operator()(arithm_val const& av1, arithm_val const& av2) {
        return arithm_val{};
      }
    };  // arithm_binarizer

    struct arithm_evaluator {
      int operator()(BinaryNode<arithm_val> const& av) const {
        return av->val;
      }

      int operator()(BinaryNode<arithm_val> const& av, int leval,
                     int reval) const {
        return leval + reval;
      }
    };  // arithm_evaluator

    auto constexpr summands = std::array{1, 2, 3, 4, 5};

    auto const node = BinaryNode<arithm_val>{summands, arithm_binarizer{}};

    REQUIRE(node.evaluate(arithm_evaluator{}) == 15);

    // another one
    struct string_holder {
      std::string str{};
    };

    auto constexpr words = std::array{"he", "ll", "o,", " w", "or", "ld", "!"};

    struct words_binarizer {
      string_holder operator()(std::string const& str) const {
        return string_holder{str};
      }
      string_holder operator()(string_holder const& h1,
                               string_holder const& h2) const {
        return string_holder{};
      }
    };  // words_binarizer

    auto const words_node = BinaryNode<string_holder>{words, words_binarizer{}};

    struct string_concat {
      std::string operator()(BinaryNode<string_holder> const& node) const {
        return node->str;
      }

      std::string operator()(BinaryNode<string_holder> const& node,
                             std::string const& lstr,
                             std::string const& rstr) const {
        return lstr + rstr;
      }
    };  // string_concat

    REQUIRE(words_node.evaluate(string_concat{}) ==
            std::string{"hello, world!"});
  }

  SECTION("digraph generation") {
    auto take_nums = [](size_t count, int from = 1) {
      return iota(from) | take(count);
    };

    struct make_sum {
      int operator()(int x) const { return x; }
      int operator()(int x, int y) const { return x + y; }
    };

    auto ms = make_sum{};
    auto const node1 = BinaryNode<int>{take_nums(1), ms};
    auto const node2 = BinaryNode<int>{take_nums(2), ms};
    auto const node3 = BinaryNode<int>{take_nums(3), ms};
    auto const node4 = BinaryNode<int>{6, BinaryNode<int>{1},
                                       BinaryNode<int>{take_nums(2, 2), ms}};

    auto label_gen_str = [](auto const& n) { return std::to_string(*n); };
    auto label_gen_wstr = [](auto const& n) { return std::to_wstring(*n); };

    REQUIRE(node1.digraph<std::string>(label_gen_str, "node1") ==
            std::string{"digraph node1{\n"
                        "node0[label=1];\n"
                        "}"});

    REQUIRE(node2.digraph<std::string>(label_gen_str) ==
            std::string{"digraph {\n"
                        "node0[label=3];\n"
                        "node1[label=1];\n"
                        "node2[label=2];\n"
                        "node0 -> node1;\n"
                        "node0 -> node2;\n"
                        "}"});

    REQUIRE(node3.digraph<std::wstring>(label_gen_wstr, L"node3") ==
            std::wstring{L"digraph node3{\n"
                         "node0[label=6];\n"
                         "node1[label=3];\n"
                         "node2[label=1];\n"
                         "node3[label=2];\n"
                         "node1 -> node2;\n"
                         "node1 -> node3;\n"
                         "node4[label=3];\n"
                         "node0 -> node1;\n"
                         "node0 -> node4;\n"
                         "}"});

    REQUIRE(node4.digraph<std::wstring>(label_gen_wstr, L"node4") ==
            std::wstring{L"digraph node4{\n"
                         "node0[label=6];\n"
                         "node1[label=1];\n"
                         "node2[label=5];\n"
                         "node3[label=2];\n"
                         "node4[label=3];\n"
                         "node2 -> node3;\n"
                         "node2 -> node4;\n"
                         "node0 -> node1;\n"
                         "node0 -> node2;\n"
                         "}"});

    //
    // tree from node1:
    //              1
    //
    // tree from node2:
    //              3
    //            /   \
    //          1      2
    //
    // tree from node3:
    //              6
    //            /   \
    //          3       3
    //        /   \
    //      1      2
    //
    // tree from node4:
    //              6
    //            /   \
    //          1      5
    //                /  \
    //              2     3
    //
  }
}
