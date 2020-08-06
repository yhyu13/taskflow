#pragma once

#include "../core/executor.hpp"

namespace tf {

// Function: parallel_for_static
// static scheduling with even partition
template <typename I, typename C>
Task FlowBuilder::parallel_for_static(I beg, I end, C&& c){
  
  using namespace std::string_literals;

  Task task = emplace(
  [beg, end, c=std::forward<C>(c)] (Subflow& sf) mutable {

    if(beg == end) {
      return;
    }
  
    const size_t nworkers = sf._executor.num_workers();
    
    // only myself - no need to spawn another graph
    if(nworkers <= 1) {
      std::for_each(beg, end, c);
      return;
    }
  
    // zero-based start and end points
    const size_t n0 = std::distance(beg, end);
    const size_t q0 = n0 / nworkers;
    const size_t t0 = n0 % nworkers;

    for(size_t i=0; i<nworkers; ++i) {
      
      size_t q, t;

      if(i < t0) {
        t = 0;
        q = q0 + 1; 
      }
      else {
        t = t0;
        q = q0;
      }
  
      size_t s0 = q*i + t;
      size_t e0 = s0 + q;

      // no iteration allocated
      if(s0 >= e0) {
        break;
      }
  
      // transform to the actual start and end numbers
      auto s = std::next(beg, s0);
      auto e = std::next(s, q);

      sf.emplace([&c, s, e] () mutable {
        std::for_each(s, e, c);
      }).name("pfs_"s + std::to_string(i));
    }
  });  

  return task;

}

// Function: parallel_for_static
// static scheduling with chunk size
template <typename I, typename C>
Task FlowBuilder::parallel_for_static(
  I beg, I end, C&& c, size_t chunk_size
){

  using namespace std::string_literals;

  if(chunk_size == 0) {
    return parallel_for_static(beg, end, std::forward<C>(c));
  }

  Task task = emplace(
  [beg, end, chunk_size, c=std::forward<C>(c)] (Subflow& sf) mutable {
  
    if(beg == end) {
      return;
    }
    
    const size_t nworkers = sf._executor.num_workers();
    
    // only myself - no need to spawn another graph
    if(nworkers <= 1) {
      std::for_each(beg, end, c);
      return;
    }
  
    const size_t n = std::distance(beg, end);

    for(size_t i=0; i<nworkers; ++i) {
      
      // initial
      if(i*chunk_size >= n) {
        break;
      }

      sf.emplace([beg, chunk_size, n, nworkers, i, &c] () mutable {

        size_t trip = 0;
        
        while(1) {

          size_t s0 = (trip * nworkers + i) * chunk_size;
          size_t e0 = s0 + chunk_size;

          if(s0 >= n) {
            break;
          }

          if(e0 > n) {
            e0 = n;
          }

          auto s = std::next(beg, s0);
          auto e = std::next(beg, e0);

          std::for_each(s, e, c);

          if(e0 == n) {
            break;
          }
          trip++;
        }

      }).name("pfs_"s + std::to_string(i));

    }

  });  

  return task;
}

// Function: parallel_for_static
// static scheduling with even partition
template <typename I, typename C, 
  std::enable_if_t<std::is_integral<std::decay_t<I>>::value, void>*
>
Task FlowBuilder::parallel_for_static(I beg, I end, I inc, C&& c){

  using namespace std::string_literals;

  Task task = emplace(
  [beg, end, inc, c=std::forward<C>(c)] (Subflow& sf) mutable {
  
    if((inc == 0 && beg != end) || 
       (beg < end && inc <=  0) || 
       (beg > end && inc >=  0)) {
      TF_THROW("invalid range [", beg, ", ", end, ") with step size ", inc);
    }
    
    const size_t nworkers = sf._executor.num_workers();
    
    // only myself - no need to spawn another graph
    if(nworkers <= 1) {
      for(I x = beg; (inc < 0 ? x > end : x < end); x+= inc) {
        c(x);
      }
      return;
    }
  
    // zero-based start and end points
    const size_t n0 = (end - beg + inc + (inc > 0 ? -1 : 1)) / inc;
    const size_t q0 = n0 / nworkers;
    const size_t t0 = n0 % nworkers;

    for(size_t i=0; i<nworkers; ++i) {
      
      size_t q, t;

      if(i < t0) {
        t = 0;
        q = q0 + 1; 
      }
      else {
        t = t0;
        q = q0;
      }
  
      size_t s0 = q*i + t;
      size_t e0 = s0 + q;

      // no iteration allocated
      if(s0 >= e0) {
        break;
      }
  
      // transform to the actual start and end numbers
      I s = static_cast<I>(s0) * inc + beg;
      I e = static_cast<I>(e0) * inc + beg;

      sf.emplace([&c, &inc, s, e] () mutable {
        for(I x = s; (inc < 0 ? x > e : x < e); x+= inc) {
          c(x);
        }
      }).name("pfs_"s + std::to_string(i));
    }
  });  

  return task;
}

// Function: parallel_for_static
// static scheduling with chunk size
template <typename I, typename C, 
  std::enable_if_t<std::is_integral<std::decay_t<I>>::value, void>*
>
Task FlowBuilder::parallel_for_static(
  I beg, I end, I inc, C&& c, size_t chunk_size
){

  using namespace std::string_literals;

  if(chunk_size == 0) {
    return parallel_for_static(beg, end, inc, std::forward<C>(c));
  }

  Task task = emplace(
  [beg, end, inc, chunk_size, c=std::forward<C>(c)] (Subflow& sf) mutable {
  
    if((inc == 0 && beg != end) || 
       (beg < end && inc <=  0) || 
       (beg > end && inc >=  0)) {
      TF_THROW("invalid range [", beg, ", ", end, ") with step size ", inc);
    }
    
    size_t nworkers = sf._executor.num_workers();
    
    // only myself - no need to spawn another graph
    if(nworkers <= 1) {
      for(I x = beg; (inc < 0 ? x > end : x < end); x+= inc) {
        c(x);
      }
      return;
    }
  
    // zero-based start and end points
    const size_t n = (end - beg + inc + (inc > 0 ? -1 : 1)) / inc;

    for(size_t i=0; i<nworkers; ++i) {
      
      // initial
      if(i*chunk_size >= n) {
        break;
      }

      sf.emplace([beg, inc, chunk_size, n, nworkers, i, &c] () mutable {

        size_t trip = 0;
        
        while(1) {

          size_t s0 = (trip * nworkers + i) * chunk_size;
          size_t e0 = s0 + chunk_size;

          if(s0 >= n) {
            break;
          }

          if(e0 > n) {
            e0 = n;
          }

          I s = static_cast<I>(s0) * inc + beg;
          I e = static_cast<I>(e0) * inc + beg;

          for(I x = s; (inc < 0 ? x > e : x < e); x+= inc) {
            c(x);
          }

          if(e0 == n) {
            break;
          }
          trip++;
        }

      }).name("pfs_"s + std::to_string(i));

    }

  });  

  return task;
}




}  // end of namespace tf -----------------------------------------------------
