//
// Created by Eduard Valeyev on 3/28/18.
//

#ifndef SEQUANT2_TIMER_HPP
#define SEQUANT2_TIMER_HPP

#include <chrono>
#include <iostream>

namespace sequant2 {

/// TimerPool aggregates \c N C++11 "timers"; used to high-resolution profile
/// stages of integral computation
/// @tparam N the number of timers
/// @note member functions are not reentrant, use one Timers object per thread
template<size_t N = 1>
class TimerPool {
 public:
  typedef std::chrono::duration<double> dur_t;
  typedef std::chrono::high_resolution_clock clock_t;
  typedef std::chrono::time_point<clock_t> time_point_t;

  TimerPool() {
    clear();
    set_now_overhead(0);
  }

  /// returns the current time point
  static time_point_t now() { return clock_t::now(); }

  /// use this to report the overhead of now() call; if set, the reported
  /// timings will be adjusted for this overhead
  /// @note this is clearly compiler and system dependent, please measure
  /// carefully (turn off turboboost, etc.)
  ///       using src/bin/profile/chrono.cc
  void set_now_overhead(size_t ns) { overhead_ = std::chrono::nanoseconds(ns); }

  /// starts timer \c t
  void start(size_t t = 0) { tstart_[t] = now(); }
  /// stops timer \c t
  /// @return the duration, corrected for overhead, elapsed since the last call
  /// to \c start(t)
  dur_t stop(size_t t = 0) {
    const auto tstop = now();
    const dur_t result = (tstop - tstart_[t]) - overhead_;
    timers_[t] += result;
    return result;
  }
  /// reads value (in seconds) of timer \c t , converted to \c double
  double read(size_t t = 0) const { return timers_[t].count(); }
  /// resets timers to zero
  void clear() {
    for (auto t = 0; t != ntimers; ++t) {
      timers_[t] = dur_t::zero();
      tstart_[t] = time_point_t();
    }
  }

 private:
  constexpr static auto ntimers = N;
  dur_t timers_[ntimers];
  time_point_t tstart_[ntimers];
  dur_t overhead_;  // the duration of now() call ... use this to automatically
  // adjust reported timings is you need fine-grained timing
};

}   // namespace sequant2

#define SEQUANT2_PROFILE_SINGLE(id, call)                                    \
    {                                                                        \
    sequant2::TimerPool<> timer;                                             \
    timer.start();                                                           \
    { (call); }                                                              \
    timer.stop();                                                            \
    auto elapsed_seconds = timer.read();                                     \
    std::wcout << id ": elapsed_time = " << std::scientific                  \
               << elapsed_seconds << " seconds" << std::endl;                \
    }


#endif //SEQUANT2_TIMER_HPP