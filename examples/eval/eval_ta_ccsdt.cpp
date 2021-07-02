#include <SeQuant/core/op.hpp>
#include <SeQuant/core/optimize/optimize.hpp>
#include <SeQuant/core/parse_expr.hpp>
#include <SeQuant/domain/eqs/cceqs.hpp>
#include <SeQuant/domain/eval/eval.hpp>
#include <SeQuant/domain/eval/eval_ta.hpp>
#include <SeQuant/domain/eval/read_tensor_ta.hpp>
#include <SeQuant/domain/mbpt/convention.hpp>

#include <tiledarray.h>
#include <iomanip>
#include <iostream>
#include <range/v3/all.hpp>

template <typename Tensor_t>
struct yield_leaf {
  size_t const no, nv;
  Tensor_t const &G, &F, &t_vo, &t_vvoo, &t_vvvooo;
  yield_leaf(size_t nocc, size_t nvirt, Tensor_t const& fock,
             Tensor_t const& eri, Tensor_t const& ampl_vo,
             Tensor_t const& ampl_vvoo, Tensor_t const& ampl_vvvooo)
      : no{nocc},
        nv{nvirt},
        G{eri},
        F{fock},
        t_vo{ampl_vo},
        t_vvoo{ampl_vvoo},
        t_vvvooo{ampl_vvvooo}

  {}

  auto range1_limits(sequant::Tensor const& tensor) {
    return tensor.const_braket() |
           ranges::views::transform([this](auto const& idx) {
             auto ao = sequant::IndexSpace::active_occupied;
             auto au = sequant::IndexSpace::active_unoccupied;
             auto sp = idx.space();
             assert(sp == ao || sp == au);

             return sp == ao ? no : nv;
           });
  }

  Tensor_t operator()(sequant::Tensor const& tensor) {
    if (tensor.label() == L"t") {
      auto rank = tensor.rank();
      assert(rank == 1 || rank == 2 || rank == 3);
      return rank == 1 ? t_vo : rank == 2 ? t_vvoo : t_vvvooo;
    }

    auto r1_limits = range1_limits(tensor);

    auto trange_vec = r1_limits | ranges::views::transform([](auto x) {
                        return TA::TiledRange1{0, x};
                      }) |
                      ranges::to_vector;

    auto iter_limits =
        r1_limits | ranges::views::transform([this](auto x) {
          return x == no ? std::pair{size_t{0}, no} : std::pair{no, no + nv};
        });

    auto tlabel = tensor.label();
    assert(tlabel == L"g" || tlabel == L"f");

    auto const& big_tensor = tlabel == L"g" ? G : F;

    auto slice =
        TA::TArrayD{big_tensor.world(),
                    TA::TiledRange{trange_vec.begin(), trange_vec.end()}};
    slice.fill(0);
    auto tile_orig = big_tensor.find(0).get();
    auto tile_dest = slice.find(0).get();

    assert(iter_limits.size() == 2 || iter_limits.size() == 4);
    if (iter_limits.size() == 2) {
      for (auto ii = iter_limits[0].first; ii < iter_limits[0].second; ++ii)
        for (auto jj = iter_limits[1].first; jj < iter_limits[1].second; ++jj) {
          tile_dest(ii - iter_limits[0].first,  //
                    jj - iter_limits[1].first) = tile_orig(ii, jj);
        }
    } else {  // 4 iterations
      for (auto ii = iter_limits[0].first; ii < iter_limits[0].second; ++ii)
        for (auto jj = iter_limits[1].first; jj < iter_limits[1].second; ++jj)
          for (auto kk = iter_limits[2].first; kk < iter_limits[2].second; ++kk)
            for (auto ll = iter_limits[3].first; ll < iter_limits[3].second;
                 ++ll) {
              tile_dest(ii - iter_limits[0].first, jj - iter_limits[1].first,
                        kk - iter_limits[2].first, ll - iter_limits[3].first) =
                  tile_orig(ii, jj, kk, ll);
            }
    }

    return slice;
  }
};  // yield_leaf

// clang-format off
/**
 * <executable> (fock.dat eri.dat | eri.dat fock.dat)
 *
 * .dat format:
 *
 * size_t size_t size_t         # rank, nocc, nvirt
 * double                       # data ------
 * ...                          # data       |
 * ...                          # ....       |  no. of double entries = (nocc+nvirt)^rank
 * ...                          # data       |
 * double                       # data ------
 */
// clang-format on
int main(int argc, char** argv) {
  using sequant::eval::compatible_dims;
  using sequant::eval::make_trange;
  using sequant::eval::read_header;
  using sequant::eval::read_tensor_ta;

  using ranges::views::take;
  using sequant::optimize::optimize;
  using sequant::optimize::tail_factor;

  using std::cout;
  using std::endl;

  std::string_view fock_ifname = argc > 1 ? argv[1] : "fock.dat";
  std::string_view eri_ifname = argc > 2 ? argv[2] : "eri.dat";

  assert(compatible_dims(fock_ifname, eri_ifname));

  auto fock_header = read_header(fock_ifname);
  auto eri_header = read_header(eri_ifname);
  if (fock_header.rank > eri_header.rank) {
    std::swap(fock_ifname, eri_ifname);
    std::swap(fock_header, eri_header);
  }

  assert(fock_header.rank == 2 && "Fock tensor should be rank 2");
  assert(eri_header.rank == 4 && "Eri tensor should be rank 4");

  auto const nocc = fock_header.nocc;
  auto const nvirt = fock_header.nvirt;

  auto& world = TA::initialize(argc, argv);

  auto fock = TA::TArrayD{world, make_trange(fock_header.rank, nocc + nvirt)};

  auto eri = TA::TArrayD{world, make_trange(eri_header.rank, nocc + nvirt)};

  read_tensor_ta(fock_ifname, fock);
  read_tensor_ta(eri_ifname, eri);

  sequant::detail::OpIdRegistrar op_id_registrar;

  sequant::mbpt::set_default_convention();

  sequant::TensorCanonicalizer::register_instance(
      std::make_shared<sequant::DefaultTensorCanonicalizer>());

  auto D_vo = TA::TArrayD{world, TA::TiledRange{TA::TiledRange1{0, nvirt},
                                                TA::TiledRange1{0, nocc}}};
  auto D_vvoo = TA::TArrayD{
      world,
      TA::TiledRange{TA::TiledRange1{0, nvirt}, TA::TiledRange1{0, nvirt},
                     TA::TiledRange1{0, nocc}, TA::TiledRange1{0, nocc}}};

  auto D_vvvooo = TA::TArrayD{
      world,
      TA::TiledRange{TA::TiledRange1{0, nvirt}, TA::TiledRange1{0, nvirt},
                     TA::TiledRange1{0, nvirt}, TA::TiledRange1{0, nocc},
                     TA::TiledRange1{0, nocc}, TA::TiledRange1{0, nocc}}};

  D_vo.fill(0);
  D_vvoo.fill(0);
  D_vvvooo.fill(0);
  auto tile_D_vo = D_vo.find(0).get();
  auto tile_D_vvoo = D_vvoo.find(0).get();
  auto tile_D_vvvooo = D_vvvooo.find(0).get();
  [nocc, nvirt](auto const& fock, auto& dvo_tile, auto& dvvoo_tile,
                auto& dvvvooo_tile) {
    auto tile_fock = fock.find(0).get();
    for (auto a = 0; a < nvirt; ++a)
      for (auto i = 0; i < nocc; ++i) {
        dvo_tile(a, i) = tile_fock(i, i) - tile_fock(nocc + a, nocc + a);
        for (auto b = 0; b < nvirt; ++b)
          for (auto j = 0; j < nocc; ++j) {
            dvvoo_tile(a, b, i, j) = dvo_tile(a, i) + tile_fock(j, j) -
                                     tile_fock(nocc + b, nocc + b);
            for (auto c = 0; c < nvirt; ++c)
              for (auto k = 0; k < nocc; ++k)
                dvvvooo_tile(a, b, c, i, j, k) = dvvoo_tile(a, b, i, j) +
                                                 tile_fock(k, k) -
                                                 tile_fock(nocc + c, nocc + c);
          }
      }
  }(fock, tile_D_vo, tile_D_vvoo, tile_D_vvvooo);

  auto t_vo = TA::TArrayD{world, D_vo.trange()};
  auto t_vvoo = TA::TArrayD{world, D_vvoo.trange()};
  auto t_vvvooo = TA::TArrayD{world, D_vvvooo.trange()};
  t_vo.fill(0);
  t_vvoo.fill(0);
  t_vvvooo.fill(0);

  auto yielder = yield_leaf{nocc, nvirt, fock, eri, t_vo, t_vvoo, t_vvvooo};

  auto const g_vvoo = yielder(
      sequant::parse_expr_asymm(L"g_{a1,a2}^{i1,i2}")->as<sequant::Tensor>());
  auto const f_vo =
      yielder(sequant::parse_expr_asymm(L"f_{a1}^{i1}")->as<sequant::Tensor>());

  std::cout << "Generating equations.." << std::endl;
  auto cc_r = sequant::eqs::cceqvec{3, 3}(true, true, true, true, true);
  std::cout << "Generating equations.. Done." << std::endl;
  auto contains_t3 = [](const auto& summand_ptr) {
    return ranges::any_of(
        summand_ptr->template as<sequant::Product>().factors(),
        [](const auto& factor_ptr) {
          return factor_ptr->template is<sequant::Tensor>() &&
                 factor_ptr->template as<sequant::Tensor>().label() ==
                 L"t" &&
                 factor_ptr->template as<sequant::Tensor>().bra_rank() ==
                 3;
        });
  };
  std::wcout << "R1(T3) = " << to_latex(cc_r[1]->as<sequant::Sum>().filter(contains_t3)) << std::endl;
  std::wcout << "R2(T3) = " << to_latex(cc_r[2]->as<sequant::Sum>().filter(contains_t3)) << std::endl;
  std::wcout << "R3 = " << to_latex(cc_r[3]) << std::endl;

  std::cout << "Optimizing equations.." << std::endl;
  // canonicalize expressions while optimizing
  bool canon = true;
  auto nodes = ranges::views::tail(cc_r) |
               ranges::views::transform([canon](auto const& n) {
                 // return optimize(tail_factor(n), canon);
                 return sequant::to_eval_node(tail_factor(n));
               }) |
               ranges::to_vector;
  std::cout << "Optimizing equations.. Done." << std::endl;

  auto const& node_r1 = nodes[0];
  auto const& node_r2 = nodes[1];
  auto const& node_r3 = nodes[2];

  auto const leaf_hashes = [&nodes]() {
    std::set<sequant::EvalExpr::hash_t> hash_{};
    for (auto const& n : nodes)
      n.visit_leaf([&hash_](auto const& x) { hash_.emplace(x->hash()); });
    return hash_;
  }();

  // true: leaf tensors (other than 't' tensors) will be cached
  // false: only intermediates will be cached
  // auto manager = sequant::eval::make_cache_man<TA::TArrayD>(nodes, true);
  auto manager = sequant::utils::cache_manager<TA::TArrayD>{{}, leaf_hashes};

  const auto maxiter = 100;
  const auto conv = 1e-12;
  size_t iter = 0;
  double ediff = 0.0;
  auto normdiff = 0.0;
  auto ecc = 0.0;

  auto eval_inst_r1 = sequant::eval::eval_instance_ta{node_r1};
  auto eval_inst_r2 = sequant::eval::eval_instance_ta{node_r2};
  auto eval_inst_r3 = sequant::eval::eval_instance_ta{node_r3};
  auto start = std::chrono::high_resolution_clock::now();
  do {
    ++iter;
    manager.reset_decaying();

    auto r1 = eval_inst_r1.evaluate_asymm(yielder, manager);
    auto r2 = eval_inst_r2.evaluate_asymm(yielder, manager);
    auto r3 = eval_inst_r3.evaluate_asymm(yielder, manager);

    auto tile_r1 = r1.find(0).get();
    auto tile_r2 = r2.find(0).get();
    auto tile_r3 = r3.find(0).get();
    auto tile_t_vo = t_vo.find(0).get();
    auto tile_t_vvoo = t_vvoo.find(0).get();
    auto tile_t_vvvooo = t_vvvooo.find(0).get();

    auto norm_last = t_vvvooo.find(0).get().norm();

    // updating Ts'
    for (auto i = 0; i < nocc; ++i)
      for (auto a = 0; a < nvirt; ++a) {
        tile_t_vo(a, i) += tile_r1(a, i) / tile_D_vo(a, i);
        for (auto j = 0; j < nocc; ++j)
          for (auto b = 0; b < nvirt; ++b) {
            tile_t_vvoo(a, b, i, j) +=
                tile_r2(a, b, i, j) / tile_D_vvoo(a, b, i, j);
            for (auto k = 0; k < nocc; ++k)
              for (auto c = 0; c < nvirt; ++c)
                tile_t_vvvooo(a, b, c, i, j, k) +=
                    tile_r3(a, b, c, i, j, k) / tile_D_vvvooo(a, b, c, i, j, k);
          }
      }

    normdiff = norm_last - t_vvvooo.find(0).get().norm();

    // calculating ecc
    auto ecc_last = ecc;

    TA::TArrayD tau_scaled;
    tau_scaled("a,b,i,j") =
        0.25 * t_vvoo("a,b,i,j") + 0.5 * t_vo("a,i") * t_vo("b,j");

    ecc = TA::dot(f_vo("a,i"), t_vo("a,i")) +
          TA::dot(g_vvoo("a,b,i,j"), tau_scaled("a,b,i,j"));

    ediff = ecc_last - ecc;

    cout << "E(CC) = "
         << std::setprecision(std::numeric_limits<double>::max_digits10) << ecc
         << endl;

  } while (iter < maxiter &&
           (std::fabs(normdiff) > conv || std::fabs(ediff) > conv));
  manager.reset_all();

  auto stop = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  cout << "\nOut of loop after " << iter << " iterations.\n"
       << "\nTime: " << duration.count() << " microseconds." << endl;

  return 0;
}
