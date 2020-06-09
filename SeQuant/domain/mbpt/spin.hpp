//
// Created by Eduard Valeyev on 2019-02-27.
//

#ifndef SEQUANT_SPIN_HPP
#define SEQUANT_SPIN_HPP

#include <SeQuant/core/tensor_network.hpp>
#include "SeQuant/core/tensor.hpp"

namespace sequant {

/// @brief Applies index replacement rules to an ExprPtr
/// @param expr ExprPtr to transform
/// @param index_replacements index replacement map
/// @param scaling_factor to scale the result
/// @return a substituted and scaled expression pointer
ExprPtr transform_expression(const ExprPtr& expr,
                             std::map<Index, Index>& index_replacements,
                             double scaling_factor = 1.0) {
  if (expr->is<Constant>()) return ex<Constant>(scaling_factor) * expr;

  auto transform_tensor = [&](const Tensor& tensor) {
    auto result = std::make_shared<Tensor>(tensor);
    result->transform_indices(index_replacements);
    ranges::for_each(result->const_braket(),
                     [&](const Index& idx) { idx.reset_tag(); });
    return result;
  };

  auto transform_product = [&](const Product& product) {
    auto result = std::make_shared<Product>();
    result->scale(product.scalar());
    for (auto&& term : product) {
      if (term->is<Tensor>()) {
        auto tensor = term->as<Tensor>();
        result->append(1, transform_tensor(tensor));
      }
    }
    result->scale(scaling_factor);
    return result;
  };

  if (expr->is<Tensor>()) {
    auto result =
        ex<Constant>(scaling_factor) * transform_tensor(expr->as<Tensor>());
    return result;
  } else if (expr->is<Product>()) {
    auto result = transform_product(expr->as<Product>());
    return result;
  } else if (expr->is<Sum>()) {
    auto result = std::make_shared<Sum>();
    for (auto&& term : *expr) {
      if (term->is<Constant>())
        result->append(ex<Constant>(scaling_factor) * term);
      else if (term->is<Tensor>()) {
        auto transformed_tensor =
            ex<Constant>(scaling_factor) * transform_tensor(term->as<Tensor>());
        result->append(transformed_tensor);
      } else if (term->is<Product>()) {
        auto transformed_product = transform_product(term->as<Product>());
        result->append(transformed_product);
      }
    }
    return result;
  } else
    return nullptr;
}

/// @brief Adds spins to indices in an expression with a replacement map
/// @param expr an expression pointer
/// @param index_replacements a map of pairs containing the index and the
/// corresponding replacement
/// @return expr the ExprPtr with substituted indices
ExprPtr append_spin(ExprPtr& expr, std::map<Index, Index>& index_replacements) {
  auto add_spin_to_tensor = [&](const Tensor& tensor) {
    auto spin_tensor = std::make_shared<Tensor>(tensor);
    spin_tensor->transform_indices(index_replacements);
    return spin_tensor;
  };

  auto add_spin_to_product = [&](const Product& product) {
    auto spin_product = std::make_shared<Product>();
    spin_product->scale(product.scalar());
    for (auto&& term : product) {
      if (term->is<Tensor>())
        spin_product->append(1, add_spin_to_tensor(term->as<Tensor>()));
    }
    return spin_product;
  };

  if (expr->is<Tensor>()) {
    return add_spin_to_tensor(expr->as<Tensor>());
  } else if (expr->is<Product>()) {
    return add_spin_to_product(expr->as<Product>());
  } else if (expr->is<Sum>()) {
    auto spin_expr = std::make_shared<Sum>();
    for (auto&& summand : *expr) {
      if (summand->is<Tensor>()) {
        spin_expr->append(add_spin_to_tensor(summand->as<Tensor>()));
      } else if (summand->is<Product>()) {
        spin_expr->append(add_spin_to_product(summand->as<Product>()));
      } else {
        spin_expr->append(summand);
      }
    }
    return spin_expr;
  } else
    return nullptr;
}

/// @brief Removes spin label from all indices in an expression
/// @param expr an ExprPtr with spin indices
/// @return expr an ExprPtr with spin labels removed
ExprPtr remove_spin(ExprPtr& expr) {
  auto remove_spin_from_tensor = [&](const Tensor& tensor) {
    container::svector<Index> bra;
    container::svector<Index> ket;
    {
      for (auto&& idx : tensor.bra()) bra.emplace_back(idx);
      for (auto&& idx : tensor.ket()) ket.emplace_back(idx);
      auto braket_list = ranges::views::concat(bra, ket);

      for (auto&& idx : braket_list) {
        auto space = IndexSpace::instance(
            IndexSpace::instance(idx.label()).type(), IndexSpace::nullqns);
        auto subscript_label = idx.label().substr(idx.label().find(L'_') + 1);
        std::wstring subscript_label_ws(subscript_label.begin(),
                                        subscript_label.end());
        idx = Index::make_label_index(space, subscript_label_ws);
      }
    }
    auto sft = Tensor(tensor.label(), bra, ket, tensor.symmetry(),
                      tensor.braket_symmetry());
    auto sf_tensor = std::make_shared<Tensor>(sft);
    return sf_tensor;
  };

  auto remove_spin_from_product = [&](const Product& product) {
    auto result = std::make_shared<Product>();
    result->scale(product.scalar());
    for (auto&& term : product) {
      if (term->is<Tensor>()) {
        result->append(1, remove_spin_from_tensor(term->as<Tensor>()));
      } else
        abort();
    }
    return result;
  };

  if (expr->is<Tensor>()) {
    return remove_spin_from_tensor(expr->as<Tensor>());
  } else if (expr->is<Product>()) {
    return remove_spin_from_product(expr->as<Product>());
  } else if (expr->is<Sum>()) {
    auto result = std::make_shared<Sum>();
    for (auto&& summand : *expr) {
      if (summand->is<Product>()) {
        result->append(remove_spin_from_product(summand->as<Product>()));
      } else if (summand->is<Tensor>()) {
        result->append(remove_spin_from_tensor(summand->as<Tensor>()));
      } else {
        result->append(summand);
      }
    }
    return result;
  } else
    return expr;
}

/// @brief Checks the spin symmetry of a pair of indices corresponding to a
/// particle in tensor notation
/// @param tensor a tensor with indices containing spin labels
/// @return true if spin symmetry matches for all pairs of indices
inline bool is_tensor_spin_symm(const Tensor& tensor) {
  bool result = false;
  assert(tensor.bra_rank() == tensor.ket_rank());
  auto iter_ket = tensor.ket().begin();
  for (auto&& index : tensor.bra()) {
    if (IndexSpace::instance(index.label()).qns() ==
        IndexSpace::instance(iter_ket->label()).qns()) {
      result = true;
    } else {
      return false;
    }
    ++iter_ket;
  }
  return result;
}

/// @brief Check if the number of alpha spins in bra and ket are equal;
/// beta spins will match if total number of indices is the same
/// @param tensor with spin indices
/// @return true if number of alpha spins match in bra and ket
inline bool can_expand(const Tensor& tensor) {
  assert(tensor.bra_rank() == tensor.ket_rank() && "can_expand failed.");
  if (tensor.bra_rank() != tensor.ket_rank()) return false;
  auto result = false;
  // TODO: Throw error if called on non-qns idx
  auto alpha_in_bra = 0;
  auto alpha_in_ket = 0;
  ranges::for_each(tensor.bra(), [&](const Index& idx) {
    if (IndexSpace::instance(idx.label()).qns() == IndexSpace::alpha)
      ++alpha_in_bra;
  });
  ranges::for_each(tensor.ket(), [&](const Index& idx) {
    if (IndexSpace::instance(idx.label()).qns() == IndexSpace::alpha)
      ++alpha_in_ket;
  });
  if (alpha_in_bra == alpha_in_ket) result = true;
  return result;
}

/// @brief expand an antisymmetric tensor
/// @param tensor a tensor from a product
/// @return an ExprPtr containing the sum of expanded terms if antisymmetric OR
/// @return an ExprPtr containing the tensor otherwise
ExprPtr expand_antisymm(const Tensor& tensor) {
  assert(tensor.bra().size() == tensor.ket().size());
  if(tensor.bra().size() == 1)
    return std::make_shared<Tensor>(tensor);

  auto get_phase = [&](const Tensor& t) {
    assert(t.bra_rank() > 1);
    container::svector<Index> bra;
    for (auto&& bra_idx : t.bra()) bra.push_back(bra_idx);
    container::svector<Index> ket;
    for (auto&& ket_idx : t.ket()) ket.push_back(ket_idx);
    IndexSwapper::thread_instance().reset();
    bubble_sort(std::begin(bra), std::end(bra), std::less<Index>{});
    bubble_sort(std::begin(ket), std::end(ket), std::less<Index>{});
    bool even = IndexSwapper::thread_instance().even_num_of_swaps();
    return (even ? 1 : -1);
  };

  // Generate a sum of asymmetric tensors if the input tensor is antisymmetric
  // and greater than one body otherwise, return the tensor
  if ((tensor.symmetry() == Symmetry::antisymm) && (tensor.bra().size() > 1)) {
    const auto prefactor = get_phase(tensor);
    container::set<Index> bra_list;
    for (auto&& bra_idx : tensor.bra()) bra_list.insert(bra_idx);
    const auto const_bra_list = bra_list;

    container::set<Index> ket_list;
    for (auto&& ket_idx : tensor.ket()) ket_list.insert(ket_idx);

    Sum expr_sum{};
    auto p_count = 0;
    do {
      auto bra_list2 = bra_list;
      auto new_tensor =
          Tensor(tensor.label(), bra_list, ket_list, Symmetry::nonsymm);

      if (is_tensor_spin_symm(new_tensor)) {
        const auto phase = get_phase(new_tensor);
        auto new_tensor_ptr = ex<Tensor>(new_tensor);
        Product new_tensor_product{};
        new_tensor_product.append(phase, new_tensor_ptr);
        new_tensor_product.scale(prefactor);
        auto new_tensor_product_ptr = ex<Product>(new_tensor_product);
        expr_sum.append(new_tensor_product_ptr);
      }
      p_count++;
    } while (std::next_permutation(bra_list.begin(), bra_list.end()));

    auto result = std::make_shared<Sum>(expr_sum);
    return result;
  } else {
    auto result = std::make_shared<Tensor>(tensor);
    return result;
  }
}

// TODO: Correct this function
/// @brief expands all antisymmetric tensors in a product
/// @param expr an expression pointer to expand
/// @return an expression pointer with expanded tensors as a sum
inline ExprPtr expand_antisymm(const ExprPtr& expr) {

  if(expr->is<Constant>())
    return expr;
  else if (expr->is<Tensor>())
    return expand_antisymm(expr->as<Tensor>());

  // Product lambda
  auto expand_product = [&] (const Product& expr){
    Product temp{};
    temp.scale(expr.scalar());
    for(auto&& term: expr){
      if(term->is<Tensor>())
        temp.append(expand_antisymm(term->as<Tensor>()));
    }
    ExprPtr result = std::make_shared<Product>(temp);
    rapid_simplify(result);
    return result;
  };

  if (expr->is<Product>())
    return expand_product(expr->as<Product>());
  else if (expr->is<Sum>()){
    Sum temp{};
    for(auto&& term: *expr){
      if(term->is<Product>())
        temp.append(expand_product(term->as<Product>()));
      else if (term->is<Tensor>())
        temp.append(expand_antisymm(term->as<Tensor>()));
      else if (term->is<Constant>())
        temp.append(term);
      else
        temp.append(nullptr);
    }
    ExprPtr result = std::make_shared<Sum>(temp);
    return result;
  } else
    return nullptr;
}

/// @brief Check if label A is present in an expression pointer
/// @detail This function assumes canonical ordering of tensors
/// in product for performance
/// @input expr an expression pointer
/// @return true if A is present in input
bool has_A_label(const ExprPtr& expr){
  if (expr->is<Constant>()) return false;

  if (expr->is<Tensor>())
    return (expr->as<Tensor>().label() == L"A") ? true : false;
  else if(expr->is<Product>())
    return ((expr->as<Product>().factor(0))->as<Tensor>().label() == L"A") ? true : false;
  else if(expr->is<Sum>()) {
    bool result = false;
    for (auto&& term : *expr) {
      if (term->is<Product>())
        result = ((term->as<Product>().factor(0))->as<Tensor>().label() == L"A") ? true : false;
      if (term->is<Tensor>())
        result = (expr->as<Tensor>().label() == L"A") ? true : false;
      if (result) return result;
    }
    return result;
  } else
    throw("control reached end of check_A_label function."); // TODO: Catch exception
}

/// @brief Check if a tensor with a certain label is present in an expression
/// @param expr input expression
/// @param label tensor label to find in the expression
/// @return true if tensor with given label is found
bool has_tensor_label(const ExprPtr& expr, std::wstring label) {
  if (expr->is<Constant>()) return false;

  auto check_tensor = [&](const Tensor& tensor) {
    if (tensor.label() == label)
      return true;
    else
      return false;
  };

  auto check_product = [&](const Product& product) {
    bool result = false;
    for (auto&& term : product) {
      if (term->is<Tensor>()) {
        auto tensor = term->as<Tensor>();
        if (check_tensor(tensor)) return true;
      }
    }
    return result;
  };

  if (expr->is<Tensor>()) {
    return check_tensor(expr->as<Tensor>());
  } else if (expr->is<Product>()) {
    return check_product(expr->as<Product>());
  } else if (expr->is<Sum>()) {
    bool result = false;
    for (auto&& term : *expr) {
      if (term->is<Product>()) result = check_product(term->as<Product>());
      if (result) return true;
    }
    return result;
  } else
    return false;
}

/// @brief Generates a vector of replacement maps for Antisymmetrizer operator
/// @param A An antisymmetrizer tensor (A) (with > 2 particle indices)
/// @return Vector of replacement maps
std::vector<std::map<Index, Index>> A_replacement_map(const Tensor& A) {
  assert(A.label() == L"A");
  assert(A.bra_rank() > 1);
  assert(A.bra().size() == A.ket().size());
  container::svector<int> bra_int_list(A.bra().size());
  std::iota(std::begin(bra_int_list), std::end(bra_int_list), 0);
  auto ket_int_list = bra_int_list;
  std::vector<std::map<Index, Index>> result;
  container::svector<Index> A_braket;
  for (auto& idx : A.const_braket()) A_braket.push_back(idx);

  do {
    do {
      std::map<Index, Index> replacement_map;
      auto A_braket_ptr = A_braket.begin();
      for (auto&& idx : bra_int_list) {
        replacement_map.emplace(std::make_pair(*A_braket_ptr, A.bra()[idx]));
        A_braket_ptr++;
      }
      for (auto&& idx : ket_int_list) {
        replacement_map.emplace(std::make_pair(*A_braket_ptr, A.ket()[idx]));
        A_braket_ptr++;
      }
      result.push_back(replacement_map);
    } while (std::next_permutation(bra_int_list.begin(), bra_int_list.end()));
  } while (std::next_permutation(ket_int_list.begin(), ket_int_list.end()));
  return result;
}

/// @brief Removes tensor with a certain label from product
/// @param product A product expression
/// @param label Label of the tensor to remove
/// @return ExprPtr with the tensor removed
ExprPtr remove_tensor_from_product(const Product& product, std::wstring label) {
  auto new_product = std::make_shared<Product>();
  new_product->scale(product.scalar());
  for (auto&& term : product) {
    if (term->is<Tensor>()) {
      auto tensor = term->as<Tensor>();
      if (tensor.label() != label) new_product->append(1, ex<Tensor>(tensor));
    }
  }
  return new_product;
}

/// @brief Expand a product containing the Antisymmetrization (A) operator
/// @param A product term with/without A operator
/// @return an ExprPtr containing sum of expanded terms if A is present
ExprPtr expand_A_operator(const Product& product) {
  bool has_A_operator = false;

  // Check A and build replacement map
  std::vector<std::map<Index, Index>> map_list;
  for (auto& term : product) {
    if (term->is<Tensor>()) {
      auto A = term->as<Tensor>();
      if ((A.label() == L"A") && (A.bra().size() > 1)) {
        has_A_operator = true;
        map_list = A_replacement_map(A);
        break;
      } else if ((A.label() == L"A") && (A.bra().size() == 1)) {
        return remove_tensor_from_product(product, L"A");
      }
    }
  }

  if (!has_A_operator) return std::make_shared<Product>(product);

  auto new_result = std::make_shared<Sum>();
  for (auto&& map : map_list) {
    // Get phase of the transformation
    bool even;
    {
      container::svector<Index> transformed_list;
      for (auto&& element : map) transformed_list.push_back(element.second);
      IndexSwapper::thread_instance().reset();
      bubble_sort(std::begin(transformed_list), std::end(transformed_list),
                  std::less<Index>{});
      even = IndexSwapper::thread_instance().even_num_of_swaps();
    }

    Product new_product{};
    new_product.scale(product.scalar());
    auto temp_product = remove_tensor_from_product(product, L"A");
    for (auto&& term : *temp_product) {
      if (term->is<Tensor>()) {
        auto new_tensor = term->as<Tensor>();
        new_tensor.transform_indices(map);
        new_product.append(1, ex<Tensor>(new_tensor));
      }
    }
    new_product.scale((even ? 1 : -1));
    new_result->append(ex<Product>(new_product));
  }  // map_list
  return new_result;
}

/// @brief Write expression in terms of Symmetrizer
/// @param product
/// @return expression pointer with Symmstrizer operator
ExprPtr expr_symmetrize(const Product& product) {
  auto result = std::make_shared<Sum>();

  // Assumes canonical sequence of tensors in the product
  if(product.factor(0)->as<Tensor>().label() != L"A")
    return std::make_shared<Product>(product);

  // CHECK: A is present and >1 particle
  // GENERATE S tensor
  auto A_tensor = product.factor(0)->as<Tensor>();
  assert(A_tensor.label() == L"A");

  auto A_is_nconserving = A_tensor.bra_rank() == A_tensor.ket_rank();
  if(A_is_nconserving)
    if(A_tensor.bra_rank() == 1) return remove_tensor_from_product(product, L"A");
  assert(A_tensor.rank() > 1);

  auto S = Tensor{};
  if(A_is_nconserving){
    S = Tensor(L"S", A_tensor.bra(), A_tensor.ket(), Symmetry::nonsymm);
  } else { // A is not nconserving
    size_t n = (A_tensor.bra_rank() < A_tensor.ket_rank()) ? A_tensor.bra_rank() : A_tensor.ket_rank();
      container::svector<Index> bra_list, ket_list;
      for(size_t idx = 0; idx != n; ++idx){
        bra_list.push_back(A_tensor.bra()[idx]);
        ket_list.push_back(A_tensor.ket()[idx]);
      }
      S = Tensor(L"S", bra_list, ket_list, Symmetry::nonsymm);
  }

  // Generate replacement maps from a list(could be a bra or a ket)
  // Uses a permuted list of int to generate permutations
  // TODO factor out for reuse
  auto maps_from_list = [&] (const container::svector<Index, 4>& list){
    container::svector<int> int_list(list.size());
    std::iota(int_list.begin(), int_list.end(), 0);
    std::vector<std::map<Index, Index>> result;
    do {
      std::map<Index, Index> map;
      auto list_ptr = list.begin();
      for(auto&& i : int_list){
        map.emplace(std::make_pair(*list_ptr, list[i]));
        list_ptr++;
      }
      result.push_back(map);
    } while(std::next_permutation(int_list.begin(), int_list.end()));
    assert(result.size() == std::tgamma(list.size() + 1));
    return result;
  };

  // Get phase relative to the canonical order
  // TODO factor out for reuse
  auto get_phase = [&] (const std::map<Index, Index>& map){
    bool even;
    container::svector<Index> idx_list;
    for (auto&& pair : map) idx_list.push_back(pair.second);
    IndexSwapper::thread_instance().reset();
    bubble_sort(std::begin(idx_list), std::end(idx_list),
                std::less<Index>{});
    even = IndexSwapper::thread_instance().even_num_of_swaps();
    return even;
  };

  std::vector<std::map<Index, Index>> maps;
  // CASE 1: n_bra = n_ket on all tensors
  if(A_is_nconserving) {
    maps = maps_from_list(A_tensor.bra());
  } else {
    assert(A_tensor.bra_rank() != A_tensor.ket_rank());
    if(A_tensor.bra_rank() > A_tensor.ket_rank())
      maps = maps_from_list(A_tensor.bra());
    else
      maps = maps_from_list(A_tensor.ket());
  }
  assert(!maps.empty());
  for(auto&& map : maps){
    auto even = get_phase(map);
    Product new_product{};
    new_product.scale(product.scalar());
    even ? new_product.append(1, ex<Tensor>(S)) : new_product.append(-1, ex<Tensor>(S));
    auto temp_product = remove_tensor_from_product(product, L"A");
    for (auto&& term : *temp_product) {
      if (term->is<Tensor>()) {
        auto new_tensor = term->as<Tensor>();
        new_tensor.transform_indices(map);
        new_product.append(1, ex<Tensor>(new_tensor));
      }
    }
    result->append(ex<Product>(new_product));
  } // map
  // std::wcout << __LINE__ << to_latex(result) << "\n";
  return result;
}

/// @brief Expand an expression containing the Antisymmetrization (A) operator
/// @param expr any ExprPtr
/// @return an ExprPtr containing sum of expanded terms if A is present
ExprPtr expr_symmetrize(const ExprPtr& expr) {
  if (expr->is<Constant>() || expr->is<Tensor>()) return expr;

  if (expr->is<Product>())
    return expr_symmetrize(expr->as<Product>());
  else if (expr->is<Sum>()) {
    auto result = std::make_shared<Sum>();
    for (auto&& summand : *expr) {
      if (summand->is<Product>())
        result->append(expr_symmetrize(summand->as<Product>()));
      else
        result->append(summand);
    }
    return result;
  } else
    throw("Unknown arg Type for expr_symmetrize."); // TODO: Catch exception
}

/// @brief Expand an expression containing the Antisymmetrization (A) operator
/// @param expr any ExprPtr
/// @return an ExprPtr containing sum of expanded terms if A is present
ExprPtr expand_A_operator(const ExprPtr& expr) {
  if (expr->is<Constant>() || expr->is<Tensor>()) return expr;

  if (expr->is<Product>())
    return expand_A_operator(expr->as<Product>());
  else if (expr->is<Sum>()) {
    auto result = std::make_shared<Sum>();
    for (auto&& summand : *expr) {
      if (summand->is<Product>())
        result->append(expand_A_operator(summand->as<Product>()));
      else
        result->append(summand);
    }
    return result;
  } else
    throw("Unknown arg Type for expand_A_operator.");
}

/// @brief Generates a vector of replacement maps for particle permutation
/// operator
/// @param P A particle permutation operator (with > 2 particle indices)
/// @return Vector of replacement maps
std::vector<std::map<Index, Index>> P_replacement_map(const Tensor& P) {
  assert(P.label() == L"P");
  assert(P.bra_rank() > 1);
  assert(P.bra().size() == P.ket().size());
  container::svector<int> int_list(P.bra().size());
  std::iota(std::begin(int_list), std::end(int_list), 0);
  std::vector<std::map<Index, Index>> result;
  container::svector<Index> P_braket;
  for (auto& idx : P.const_braket()) P_braket.push_back(idx);

  do {
    auto P_braket_ptr = P_braket.begin();
    std::map<Index, Index> replacement_map;
    for (auto&& i : int_list) {
      replacement_map.emplace(std::make_pair(*P_braket_ptr, P.bra()[i]));
      P_braket_ptr++;
    }
    for (auto&& i : int_list) {
      replacement_map.emplace(std::make_pair(*P_braket_ptr, P.ket()[i]));
      P_braket_ptr++;
    }
    result.push_back(replacement_map);
  } while (std::next_permutation(int_list.begin(), int_list.end()));

  return result;
}

/// @brief Expand a product containing the particle permutation (P) operator
/// @param A product term with/without P operator
/// @return an ExprPtr containing sum of expanded terms if P is present
ExprPtr expand_P_operator(const Product& product) {
  bool has_P_operator = false;

  // Check P and build a replacement map
  std::vector<std::map<Index, Index>> map_list;
  for (auto& term : product) {
    if (term->is<Tensor>()) {
      auto P = term->as<Tensor>();
      if ((P.label() == L"P") && (P.bra().size() > 1)) {
        has_P_operator = true;
        map_list = P_replacement_map(P);
        break;
      } else if ((P.label() == L"P") && (P.bra().size() == 1)) {
        return remove_tensor_from_product(product, L"P");
      }
    }
  }

  if (!has_P_operator) return std::make_shared<Product>(product);

  auto result = std::make_shared<Sum>();
  for (auto&& map : map_list) {
    Product new_product{};
    new_product.scale(product.scalar());
    auto temp_product = remove_tensor_from_product(product, L"P");
    for (auto&& term : *temp_product) {
      if (term->is<Tensor>()) {
        auto new_tensor = term->as<Tensor>();
        new_tensor.transform_indices(map);
        new_product.append(1, ex<Tensor>(new_tensor));
      }
    }
    result->append(ex<Product>(new_product));
  }  // map_list
  return result;
}

/// @brief Expand an expression containing the particle permutation (P) operator
/// @param expr any ExprPtr
/// @return an ExprPtr containing sum of expanded terms if P is present
ExprPtr expand_P_operator(const ExprPtr& expr) {
  if (expr->is<Constant>() || expr->is<Tensor>()) return expr;

  if (expr->is<Product>())
    return expand_P_operator(expr->as<Product>());
  else if (expr->is<Sum>()) {
    auto result = std::make_shared<Sum>();
    for (auto&& summand : *expr) {
      if (summand->is<Product>())
        result->append(expand_P_operator(summand->as<Product>()));
      else
        result->append(summand);
    }
    return result;
  } else
    throw("Unknown arg Type for expand_P_operator.");
}

/// @brief Expand S operator
ExprPtr expand_S_operator(const ExprPtr& expr){

  if (expr->is<Constant>() || expr->is<Tensor>()) return expr;

  auto result = std::make_shared<Sum>();

  // Check if S operator is present
  if(!has_tensor_label(expr, L"S"))
    return expr;

  // Lambda for making replacement maps
  auto replacement_maps = [&] (const Tensor& S){
    assert(S.label() == L"S");
    assert(S.bra_rank() > 1);
    assert(S.bra().size() == S.ket().size());
    container::svector<int> int_list(S.bra().size());
    std::iota(std::begin(int_list), std::end(int_list), 0);

    std::vector<std::map<Index, Index>> maps;
    do{
      std::map<Index, Index> map;
      auto S_bra_ptr = S.bra().begin();
      auto S_ket_ptr = S.ket().begin();
      for(auto&& i : int_list){
        map.emplace(std::make_pair(*S_bra_ptr, S.bra()[i]));
        ++S_bra_ptr;
        map.emplace(std::make_pair(*S_ket_ptr, S.ket()[i]));
        ++S_ket_ptr;
      }
      maps.push_back(map);
    }while(std::next_permutation(int_list.begin(), int_list.end()));

    return maps;
  };

  // Lambda for applying S on products
  auto expand_S_product = [&] (const Product& product){
    // check if S is present
    if(!has_tensor_label(ex<Product>(product), L"S"))
      return ex<Product>(product);

    std::vector<std::map<Index, Index>> maps;
    if(product.factor(0)->as<Tensor>().label() == L"S")
      maps = replacement_maps(product.factor(0)->as<Tensor>());
    assert(!maps.empty());
    Sum sum{};
    for(auto&&map : maps){
      Product new_product{};
      new_product.scale(product.scalar());
      auto temp_product = remove_tensor_from_product(product, L"S");
      for (auto&& term : *temp_product) {
        if (term->is<Tensor>()) {
          auto new_tensor = term->as<Tensor>();
          new_tensor.transform_indices(map);
          new_product.append(1, ex<Tensor>(new_tensor));
        }
      }
      sum.append(ex<Product>(new_product));
    }
    return ex<Sum>(sum);;
  };

  if(expr->is<Product>()){
    return expand_S_product(expr->as<Product>());
  } else if (expr->is<Sum>()){
    for(auto&& term : *expr){
      if(term->is<Product>()){
        result->append(expand_S_product(term->as<Product>()));
      } else if (term->is<Tensor>()){
        result->append(term);
      } else if (term->is<Constant>()){
        result->append(term);
      }
    }
  }

  auto reset_idx_tags = [&](ExprPtr& expr) {
    if (expr->is<Tensor>())
      ranges::for_each(expr->as<Tensor>().const_braket(),
                       [&](const Index& idx) { idx.reset_tag(); });
  };

  result->visit(reset_idx_tags);
  return result;
}

/// @brief Returns the number of cycles
/// @detailed
/// @param
/// @return
inline int count_cycles(const container::svector<int, 6>& vec1, const container::svector<int, 6>& vec2) {
  assert(vec1.size() == vec2.size());
  int n_cycles = 0;
  auto dummy_idx = 99;
  container::svector<int, 6> v = vec1;
  container::svector<int, 6> v1 = vec2;
  for (auto it = v.begin(); it != v.end(); ++it) {
    if (*it != dummy_idx) {
      n_cycles++;
      auto idx = std::distance(v.begin(), it);
      auto it0 = it;
      auto it1 = std::find(v1.begin(), v1.end(), *it0);
      auto idx1 = std::distance(v1.begin(), it1);
      do {
        it0 = std::find(v.begin(), v.end(), v[idx1]);
        it1 = std::find(v1.begin(), v1.end(), *it0);
        idx1 = std::distance(v1.begin(), it1);
        *it0 = dummy_idx;
      } while (idx1 != idx);
    }
  }
  return n_cycles;
}

/// @brief Calculates permutation matrix for Biorthogonal transformation
/// @detailed
/// @param
/// @return
Eigen::MatrixXd permutation_matrix(const int n_particles) {
  int n = std::tgamma(n_particles + 1);
  Eigen::MatrixXd result(n, n);
  result.setZero();
  size_t n_row = 0;
  container::svector<int, 6> v(n_particles), v1(n_particles);
  std::iota(v.begin(), v.end(), 0);
  std::iota(v1.begin(), v1.end(), 0);
  do {
    container::vector<double> permutation_vector;
    do {
      auto cycles = count_cycles(v1, v);
      permutation_vector.push_back(std::pow(-2, cycles));
    } while (std::next_permutation(v.begin(), v.end()));
    Eigen::VectorXd pv_eig = Eigen::Map<Eigen::VectorXd, Eigen::Unaligned>(permutation_vector.data(),
                                               permutation_vector.size());
    result.row(n_row) = pv_eig;
    ++n_row;
  } while (std::next_permutation(v1.begin(), v1.end()));
  result *= std::pow(-1, n_particles);
  // std::cout << "permutation_matrix:\n" << result << "\n";
  return result;
}

/// @brief Returns a vector of coefficients
/// @detailed
/// @param
/// @return
container::vector<double> biorthogonal_transformation_coeff(const Eigen::MatrixXd& M, const double& threshold){
  using namespace Eigen;

  // Normalization constant
  double scalar;
  {
    // inline bool nonZero(double d) { return abs(d) > threshold ? true : false; }
    auto nonZero = [&] (const double d) { return abs(d) > threshold ? true : false; };

    // Solve system of equations
    SelfAdjointEigenSolver<MatrixXd> eig_solver(M);
    container::vector<double> eig_vals(eig_solver.eigenvalues().size());
    VectorXd::Map(&eig_vals[0], eig_solver.eigenvalues().size()) =
        eig_solver.eigenvalues();

    double non0count = std::count_if(eig_vals.begin(), eig_vals.end(), nonZero);
    scalar = eig_vals.size() / non0count;
  }

  // Find Pseudo Inverse, get 1st row only
  MatrixXd pinv = M.completeOrthogonalDecomposition().pseudoInverse();
  pinv = pinv.row(0) * scalar;

  // std::cout << "Scaled oefficients: << pinv
  container::vector<double> result(pinv.size());
  VectorXd::Map(&result[0], pinv.size()) = pinv;
  return result;
}

/// @brief Transforms an expression from spin orbital to spatial orbitals
/// @detailed This functions is designed for integrating spin out of expression
/// with Coupled Cluster equations in mind.
/// @attention This function may fail on arbitrarily created expressions that lacks
/// proper index attributes.
/// @param expr ExprPtr with spin orbital indices
/// @param ext_index_groups groups of external indices
/// @return an expression with spin integrated/adapted
ExprPtr closed_shell_spintrace(const ExprPtr& expression,
    std::initializer_list<IndexList> ext_index_groups = {{}}){

  // NOT supported for Proto indices
  auto check_proto_index = [&](const ExprPtr& expr) {
    if (expr->is<Tensor>()) {
      ranges::for_each(
          expr->as<Tensor>().const_braket(), [&](const Index& idx) {
            assert(!idx.has_proto_indices() &&
                "Proto index not supported in spintrace call.");
          });
    }
  };
  expression->visit(check_proto_index);

 // Symmetrize the expr and keep S operator
  auto symm_and_expand = [&] (const ExprPtr& expr){
    auto temp = expr;
    if (has_A_label(temp))
      temp = expr_symmetrize(temp);
    temp = expand_antisymm(temp);
    rapid_simplify(temp);
    return temp;
  };

  auto expr = symm_and_expand(expression);
  // std::cout << __LINE__ << expr->size() << std::endl;

  auto reset_idx_tags = [&](ExprPtr& expr) {
    if (expr->is<Tensor>())
      ranges::for_each(expr->as<Tensor>().const_braket(),
                       [&](const Index& idx) { idx.reset_tag(); });
  };

  expr->visit(reset_idx_tags); // This call is REQUIRED
  expand(expr); // This call is REQUIRED
  rapid_simplify(expr);

  // Returns the number of cycles
  auto count_cycles = [&] (container::svector<Index>& v, container::svector<Index>& v1){
    assert(v.size() == v1.size());
    size_t n_cycles = 0;
    auto dummy_idx = Index(L"p_50");
    for(auto it = v.begin(); it != v.end(); ++it){
      if(*it != dummy_idx){
        // TODO: Throw exception if bra and ket indices don't match
        n_cycles++;
        auto idx = std::distance(v.begin(), it);
        auto it0 = it;
        auto it1 = std::find(v1.begin(), v1.end(), *it0);
        auto idx1 = std::distance(v1.begin(), it1);
        do{
          it0 = std::find(v.begin(), v.end(), v[idx1]);
          it1 = std::find(v1.begin(), v1.end(), *it0);
          idx1 = std::distance(v1.begin(), it1);
          *it0 = dummy_idx;
        }while(idx1 != idx);
      }
    }
    return n_cycles;
  };

  // Lambda for a product
  auto trace_product = [&] (const Product& product){
    // TODO: Check symmetry of tensors

    // Remove S if present in a product
    Product temp_product{};
    temp_product.scale(product.scalar());
    if(product.factor(0)->as<Tensor>().label() == L"S"){
      for(auto&& term : product.factors()){
        if(term->is<Tensor>() && term->as<Tensor>().label() != L"S")
          temp_product.append(term);
      }
    } else {
      temp_product = product;
    }

    auto get_ket_indices = [&] (const Product& prod){
      container::svector<Index> ket_idx;
      for(auto&& t: prod) {
        if(t->is<Tensor>())
        ranges::for_each(t->as<Tensor>().ket(), [&] (const Index& idx) {ket_idx.push_back(idx);});
      }
      return ket_idx;
    };

    auto get_bra_indices = [&] (const Product& prod){
      container::svector<Index> bra_idx;
      for(auto&& t: prod) {
        if(t->is<Tensor>())
          ranges::for_each(t->as<Tensor>().bra(), [&] (const Index& idx) {bra_idx.push_back(idx);});
      }
      return bra_idx;
    };

    auto product_kets = get_ket_indices(temp_product);
    auto product_bras = get_bra_indices(temp_product);

    // Substitute indices from external index list
    if((*ext_index_groups.begin()).size() == 2)
      ranges::for_each(ext_index_groups, [&](const IndexList &idx_pair) {
        assert(idx_pair.size() == 2);
        if (idx_pair.size() == 2) {
          auto it = idx_pair.begin();
          auto first = *it;
          it++;
          auto second = *it;
          std::replace(product_bras.begin(), product_bras.end(), first, second);
          std::replace(product_kets.begin(), product_kets.end(), first, second);
        }
      });

    auto n_cycles = count_cycles(product_kets, product_bras);

    auto result = std::make_shared<Product>(product);
    result->scale(std::pow(2,n_cycles));
    return result;
  };

  if(expr->is<Constant>())
    return expr;
  else if (expr->is<Tensor>())
    return trace_product((ex<Constant>(1.) * expr)->as<Product>()); // expand_all(expr);
  else if(expr->is<Product>())
    return trace_product(expr->as<Product>());
  else if (expr->is<Sum>()){
    auto result = std::make_shared<Sum>();
    for(auto&& summand : *expr){
      if(summand->is<Product>()){
        result->append(trace_product(summand->as<Product>()));
      } else if (summand->is<Tensor>()){
        result->append(trace_product((ex<Constant>(1.) * summand)->as<Product>()));
      } else // summand->is<Constant>()
        result->append(summand);
    }
    return result;
  } else
    return nullptr;
}

/// @brief Transforms an expression from spin orbital to spatial orbitals
/// @detailed Given an expression, this function extracts all indices and adds a
/// spin attribute to all the indices in the expression. A map is generated with
/// all possible spin permutations and substituted in the expression. Based on
/// spin symmetry of particle indices: the non-zero terms are expanded, the spin
/// labels removed and a sum of all non-zero expressions is returned.
/// @param expr ExprPtr with spin orbital indices
/// @param ext_index_groups groups of external indices
/// @return an expression with spin integrated/adapted
ExprPtr spintrace(ExprPtr expression,
                  std::initializer_list<IndexList> ext_index_groups = {{}}) {

  // SPIN TRACE DOES NOT SUPPORT PROTO INDICES YET.
  auto check_proto_index = [&](const ExprPtr& expr) {
    if (expr->is<Tensor>()) {
      ranges::for_each(
          expr->as<Tensor>().const_braket(), [&](const Index& idx) {
            assert(!idx.has_proto_indices() &&
                   "Proto index not supported in spintrace function.");
          });
    }
  };
  expression->visit(check_proto_index);

  if (expression->is<Constant>()) return expression;

  auto spin_trace_tensor = [&](const Tensor& tensor) {
    if (can_expand(tensor)) {
      return expand_antisymm(tensor);
    } else
      return ex<Constant>(0);
  };

  auto spin_trace_product = [&](const Product& product) {
    Product spin_product{};
    spin_product.scale(product.scalar());
    for (auto&& term : product) {
      if (term->is<Tensor>()) {
        if (can_expand(term->as<Tensor>())) {
          spin_product.append(1, spin_trace_tensor(term->as<Tensor>()));
        } else
          break;
      }
    }
    if (product.size() != spin_product.size()) spin_product.scale(0);
    ExprPtr result = std::make_shared<Product>(spin_product);
    expand(result);
    rapid_simplify(result);
    return result;
  };

  auto reset_idx_tags = [&](ExprPtr& expr) {
    if (expr->is<Tensor>())
      ranges::for_each(expr->as<Tensor>().const_braket(),
                       [&](const Index& idx) { idx.reset_tag(); });
  };

  // Most important lambda of this function
  auto trace_product = [&](const Product& expression) {
    auto result = std::make_shared<Sum>();
    ExprPtr expr = std::make_shared<Product>(expression);

    container::set<Index, Index::LabelCompare> grand_idxlist;
    auto collect_indices = [&](const ExprPtr& expr) {
      if (expr->is<Tensor>()) {
        ranges::for_each(expr->as<Tensor>().const_braket(),
                         [&](const Index& idx) {
                           idx.reset_tag();
                           grand_idxlist.insert(idx);
                         });
      }
    };
    expr->visit(collect_indices);

    container::set<Index> ext_idxlist;
    for (auto&& idxgrp : ext_index_groups) {
      for (auto&& idx : idxgrp) {
        idx.reset_tag();
        ext_idxlist.insert(idx);
      }
    }

    container::set<Index> int_idxlist;
    for (auto&& gidx : grand_idxlist) {
      if (ext_idxlist.find(gidx) == ext_idxlist.end()) {
        int_idxlist.insert(gidx);
      }
    }

    // EFV: generate the grand list of index groups by concatenating list of
    // external index EFV: groups with the groups of internal indices (each
    // internal index = 1 group)
    using IndexGroup = container::svector<Index>;
    container::svector<IndexGroup> index_groups;

    for (auto&& i : int_idxlist) {
      index_groups.emplace_back(IndexGroup(1, i));
    }

    index_groups.insert(index_groups.end(), ext_index_groups.begin(),
                        ext_index_groups.end());

    // EFV: for each spincase (loop over integer from 0 to 2^n-1, n=#of index
    // groups)

    const uint64_t nspincases = std::pow(2, index_groups.size());

    for (uint64_t spincase_bitstr = 0; spincase_bitstr != nspincases;
         ++spincase_bitstr) {
      // EFV:  assign spin to each index group => make a replacement list
      std::map<Index, Index> index_replacements;

      uint64_t index_group_count = 0;
      for (auto&& index_group : index_groups) {
        auto spin_bit = (spincase_bitstr << (64 - index_group_count - 1)) >> 63;
        assert((spin_bit == 0) || (spin_bit == 1));

        for (auto&& index : index_group) {
          IndexSpace space;
          if (spin_bit == 0) {
            space = IndexSpace::instance(
                IndexSpace::instance(index.label()).type(), IndexSpace::alpha);
          } else {
            space = IndexSpace::instance(
                IndexSpace::instance(index.label()).type(), IndexSpace::beta);
          }

          // TODO: Check if valid for index with no subscripts
          auto subscript_label =
              index.label().substr(index.label().find(L'_') + 1);
          std::wstring subscript_label_ws(subscript_label.begin(),
                                          subscript_label.end());

          Index spin_index = Index::make_label_index(space, subscript_label_ws);

          index_replacements.emplace(std::make_pair(index, spin_index));
        }
        ++index_group_count;
      }

      auto spin_expr = append_spin(expr, index_replacements);
      // This call to rapid_simplify is required for Tensor case
      rapid_simplify(spin_expr);

      if (spin_expr->is<Tensor>()) {
        auto temp = spin_trace_tensor(spin_expr->as<Tensor>());
        auto spin_removed = remove_spin(temp);
        result->append(spin_removed);
      } else if (spin_expr->is<Product>()) {
        auto temp = spin_trace_product(spin_expr->as<Product>());
        if (!(temp->size() == 0)) {
          result->append(remove_spin(temp));
        }
      } else if (spin_expr->is<Sum>()) {
        for (auto&& summand : *spin_expr) {
          Sum temp{};
          if (summand->is<Tensor>())
            temp.append(spin_trace_tensor(summand->as<Tensor>()));
          else if (summand->is<Product>())
            temp.append(spin_trace_product(summand->as<Product>()));
          else {
            temp.append(summand);
          }
          ExprPtr SumPtr = std::make_shared<Sum>(temp);
          auto spin_removed = remove_spin(SumPtr);
          result->append(spin_removed);
        }
      } else {
        result->append(expr);
      }
    }  // Permutation FOR loop
    return result;
  };

  // Expand antisymmetrizer operator (A) if present in the expression
  if (has_tensor_label(expression, L"A"))
    expression = expand_A_operator(expression);

  if (expression->is<Tensor>()) expression = ex<Constant>(1) * expression;

  if (expression->is<Product>()) {
    return trace_product(expression->as<Product>());
  } else if ((expression->is<Sum>())) {
    auto result = std::make_shared<Sum>();
    for (auto&& term : *expression) {
      if (term->is<Product>())
        result->append(trace_product(term->as<Product>()));
      else if (term->is<Tensor>()) {
        auto term_as_product = ex<Constant>(1) * term;
        result->append(trace_product(term_as_product->as<Product>()));
      } else
        result->append(term);
    }
    result->visit(reset_idx_tags);
    return result;
  } else
    return nullptr;
}  // ExprPtr spintrace

}  // namespace sequant

#endif  // SEQUANT_SPIN_HPP
