#ifndef LLARP_TIME_HPP
#define LLARP_TIME_HPP

#include <util/types.hpp>

using namespace std::chrono_literals;

namespace llarp
{
  /// get time right now as milliseconds, this is monotonic
  llarp_time_t
  time_now_ms();
  /// get time right now as a Time_t, monotonic
  Time_t
  time_now();

}  // namespace llarp

#endif
