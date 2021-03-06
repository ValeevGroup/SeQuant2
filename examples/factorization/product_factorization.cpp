//
// Created by Bimal Gaudel on 12/22/19.
//

#include <btas/btas.h>
#include <btas/tensorview.h>
#include <btas/tensor_func.h>

#include <SeQuant/core/container.hpp>
#include <SeQuant/core/tensor.hpp>
#include <SeQuant/domain/factorize/factorizer.hpp>
#include "../contract/interpret/contract.hpp"

#include "../sequant_setup.hpp"

#include <iostream>
#include <limits>
#include <memory>
#include <ostream>
#include <string>

#include <functional>  // for std::bind
#include <random>      // for std::mt19937
#include <chrono>      // for seeding

int main() {
  // global sequant setup...
  std::setlocale(LC_ALL, "en_US.UTF-8");
  std::wcout.precision(std::numeric_limits<double>::max_digits10);
  std::wcerr.precision(std::numeric_limits<double>::max_digits10);
  std::wostream::sync_with_stdio(true);
  std::wostream::sync_with_stdio(true);
  std::wcout.imbue(std::locale("en_US.UTF-8"));
  std::wcerr.imbue(std::locale("en_US.UTF-8"));
  std::wostream::sync_with_stdio(true);
  std::wostream::sync_with_stdio(true);
  sequant::detail::OpIdRegistrar op_id_registrar;

  sequant::mbpt::set_default_convention();

  TensorCanonicalizer::register_instance(
      std::make_shared<DefaultTensorCanonicalizer>());
  Logger::get_instance().wick_stats = false;
  // CC equations
  auto cc_r = cceqvec{3, 3}(true, true, true, true, true);

  // factorization and evaluation of the CC equations
  using sequant::factorize::factorize_product;
  using ispace_pair = std::pair<sequant::IndexSpace::Type, size_t>;
  using ispace_map = sequant::container::map<ispace_pair::first_type,
                                             ispace_pair::second_type>;
  using BTensor = btas::Tensor<double>;

  size_t nocc = 10, nvirt = 4;
  std::wcout << "\nSetting up a map with nocc = " << nocc << " and nvirt = " << nvirt << "..\n";
  auto counter_map = std::make_shared<ispace_map>(ispace_map{});
  counter_map->insert(ispace_pair{sequant::IndexSpace::active_occupied, nocc});
  counter_map->insert(
      ispace_pair{sequant::IndexSpace::active_unoccupied, nvirt});

  // initializing tensors present in the CCSD equations
  auto Fock_oo = std::make_shared<BTensor>(nocc, nocc);
  auto Fock_ov = std::make_shared<BTensor>(nocc, nvirt);
  auto Fock_vv = std::make_shared<BTensor>(nvirt, nvirt);
  auto G_oooo = std::make_shared<BTensor> (nocc, nocc, nocc, nocc);
  auto G_vvvv = std::make_shared<BTensor> (nvirt, nvirt, nvirt, nvirt);
  auto G_ovvv = std::make_shared<BTensor> (nocc, nvirt, nvirt, nvirt);
  auto G_ooov = std::make_shared<BTensor> (nocc, nocc, nocc, nvirt);
  auto G_oovv = std::make_shared<BTensor> (nocc, nocc, nvirt, nvirt);
  auto G_ovov = std::make_shared<BTensor> (nocc, nvirt, nocc, nvirt);
  auto T_ov = std::make_shared<BTensor>   (nocc, nvirt);
  auto T_oovv = std::make_shared<BTensor> (nocc, nocc, nvirt, nvirt);
  auto T_ooovvv = std::make_shared<BTensor>(nocc, nocc, nocc, nvirt, nvirt, nvirt);

  // fill random data to tensors
  auto randfill_tensor = [&](btas::Tensor<double>& tnsr){
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937 rgen(seed);
    auto dist = std::uniform_real_distribution<double>{-1.0, 1.0};
    tnsr.generate(std::bind(dist, rgen));
  };

   randfill_tensor(*Fock_oo);
   randfill_tensor(*Fock_ov);
   randfill_tensor(*Fock_vv);
   randfill_tensor(*G_oooo );
   randfill_tensor(*G_vvvv );
   randfill_tensor(*G_ovvv );
   randfill_tensor(*G_ooov );
   randfill_tensor(*G_oovv );
   randfill_tensor(*G_ovov );
   randfill_tensor(*T_ov   );
   randfill_tensor(*T_oovv );
   randfill_tensor(*T_ooovvv);

   // for the evaluation of the expressions
   // a map is needed
   using str_to_tensor_pair = std::pair<std::wstring, std::shared_ptr<BTensor>>;
   using str_to_tensor_map = std::map<str_to_tensor_pair::first_type,
                                       str_to_tensor_pair::second_type>;

   str_to_tensor_map btensor_map;
   btensor_map.insert(str_to_tensor_pair(L"f_oo",   Fock_oo));
   btensor_map.insert(str_to_tensor_pair(L"f_ov",   Fock_ov));
   btensor_map.insert(str_to_tensor_pair(L"f_vv",   Fock_vv));
   btensor_map.insert(str_to_tensor_pair(L"g_oooo", G_oooo));
   btensor_map.insert(str_to_tensor_pair(L"g_vvvv", G_vvvv));
   btensor_map.insert(str_to_tensor_pair(L"g_ovvv", G_ovvv));
   btensor_map.insert(str_to_tensor_pair(L"g_ooov", G_ooov));
   btensor_map.insert(str_to_tensor_pair(L"g_oovv", G_oovv));
   btensor_map.insert(str_to_tensor_pair(L"g_ovov", G_ovov));
   btensor_map.insert(str_to_tensor_pair(L"t_ov",   T_ov));
   btensor_map.insert(str_to_tensor_pair(L"t_oovv", T_oovv));
   btensor_map.insert(str_to_tensor_pair(L"t_ooovvv", T_ooovvv));

   // factorization and evaluation
   auto& expr_to_factorize = cc_r[3];
   auto unfactorized_expr  = sequant::factorize::factorize_expr(expr_to_factorize, counter_map, false);
   auto factorized_expr    = sequant::factorize::factorize_expr(expr_to_factorize, counter_map, true);

  // to confirm that we are not working
  // on the same Expr
  if (*unfactorized_expr != *factorized_expr)
       std::wcout << "\nunfactorized and factorized Expr are not the same.. which is good:)\n";
  else std::wcout << "\nunfactorized and factorized Expr are the same.. time to debug:(\n";

  using std::chrono::duration_cast;
  using std::chrono::microseconds;

  auto tstart            = std::chrono::high_resolution_clock::now();
  auto unfactorized_eval = sequant::interpret::eval_equation(unfactorized_expr, btensor_map);
  auto tstop             = std::chrono::high_resolution_clock::now();
  auto duration          = duration_cast<microseconds>(tstop - tstart).count();
  std::wcout << "time(unfactorized_eval) = " << duration << " microseconds.\n";

  tstart                 = std::chrono::high_resolution_clock::now();
  auto factorized_eval   = sequant::interpret::eval_equation(factorized_expr, btensor_map);
  tstop                  = std::chrono::high_resolution_clock::now();
  duration               = duration_cast<microseconds>(tstop - tstart).count();
  std::wcout << "time(factorized_eval) = " << duration << " microseconds.\n";

  std::wcout << "\nnorm(unfac) = "
    << std::sqrt(btas::dot(unfactorized_eval.tensor(), unfactorized_eval.tensor()))
    << "\n";
  std::wcout << "\nnorm(fac) = "
    << std::sqrt(btas::dot(factorized_eval.tensor(), factorized_eval.tensor()))
    << "\n";
  std::wcout << "\nUnfactorized expr (scalars dropped!) \n";
  // print_expr(unfactorized_expr);
  std::wcout << unfactorized_expr->to_latex();
  std::wcout << "\n\nFactorized expr (scalars dropped!) \n";
  // print_expr(factorized_expr);
  std::wcout << factorized_expr->to_latex();
  return 0;
}
