#pragma once

#include "time.hpp"
#include <unordered_map>

namespace llarp
{
  namespace util
  {
    template <typename Val_t, typename Hash_t = typename Val_t::Hash>
    struct DecayingHashSet
    {
      using Time_t = std::chrono::milliseconds;

      DecayingHashSet(Time_t cacheInterval = 5s) : m_CacheInterval(cacheInterval)
      {}
      /// determine if we have v contained in our decaying hashset
      bool
      Contains(const Val_t& v) const
      {
        return m_Values.count(v) != 0;
      }

      /// return true if inserted
      /// return false if not inserted
      bool
      Insert(const Val_t& v, Time_t now = 0s)
      {
        if (now == 0s)
          now = llarp::time_now_ms();
        return m_Values.try_emplace(v, now).second;
      }

      /// decay hashset entries
      void
      Decay(Time_t now = 0s)
      {
        if (now == 0s)
          now = llarp::time_now_ms();
        EraseIf([&](const auto& item) { return (m_CacheInterval + item.second) <= now; });
      }

      Time_t
      DecayInterval() const
      {
        return m_CacheInterval;
      }

      bool
      Empty() const
      {
        return m_Values.empty();
      }

      void
      DecayInterval(Time_t interval)
      {
        m_CacheInterval = interval;
      }

     private:
      template <typename Predicate_t>
      void
      EraseIf(Predicate_t pred)
      {
        for (auto i = m_Values.begin(), last = m_Values.end(); i != last;)
        {
          if (pred(*i))
          {
            i = m_Values.erase(i);
          }
          else
          {
            ++i;
          }
        }
      }

      Time_t m_CacheInterval;
      std::unordered_map<Val_t, Time_t, Hash_t> m_Values;
    };
  }  // namespace util
}  // namespace llarp
