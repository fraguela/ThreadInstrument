/*
 ThreadInstrument: Library to monitor thread activity
 Copyright (C) 2012-2017 Basilio B. Fraguela. Universidade da Coruna
 
 Distributed under the MIT License. (See accompanying file LICENSE)
*/

///
/// \file     bench.cpp
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cassert>
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include "thread_instrument/thread_instrument.h"

unsigned NThreads, NActivities, NReps;

typedef void (*void_f_int)(unsigned);

void_f_int F2Run = nullptr;

void f(unsigned level)
{
  if (!level) {
    for(unsigned i = 0; i < NReps; i++) {
      ThreadInstrument::beginActivity(0);
      f(1);
      ThreadInstrument::endActivity(0);
    }
  } else {
    if (level < NActivities) {
      ThreadInstrument::beginActivity(level);
      f(level + 1);
      ThreadInstrument::endActivity(level);
    }
  }
}

void genericRun(unsigned i)
{
  while (F2Run == nullptr) ;
  (*F2Run)(i);
}

int main(int argc, char **argv)
{ std::vector<std::thread> v;
  
  std::cout << "[NThreads] [NActivities] [NReps per activity]\n";
  NThreads    = (argc < 2) ? std::thread::hardware_concurrency() : atoi(argv[1]);
  NActivities = (argc < 3) ? 10 : atoi(argv[2]);
  NReps       = (argc < 4) ? 1000 : atoi(argv[3]);

  std::cout << "NThreads=" << NThreads<< " NActivities=" << NActivities << " NReps=" << NReps << '\n';

  for (int i = 0; i < NThreads; i++) {
    v.emplace_back(f, 0);
  }

  ThreadInstrument::time_point_t t0 = ThreadInstrument::clock_t::now();

  F2Run = f;

  for (auto& t : v) {
    t.join();
  }

  F2Run = nullptr;

  ThreadInstrument::time_point_t t1 = ThreadInstrument::clock_t::now();
  
  std::cout << "Profiling Time=" << std::chrono::duration<double>(t1 - t0).count() << '\n';
  std::cout << "nThreadsWithActivity()=" << ThreadInstrument::nThreadsWithActivity() << '\n';


  for(unsigned i = 0; i < ThreadInstrument::nThreadsWithActivity(); ++i) {

    const ThreadInstrument::Int2EventDataMap_t& activity = ThreadInstrument::getActivity(i);
    
    unsigned n = activity.size();
    if(n != NActivities) {
      std::cerr << "activity.size()=" << n << " != " << NActivities << "!\n";
    }
    
    double last_time = 1e12;

    for (const auto& a : activity) {
      const ThreadInstrument::EventData& ev_data = a.second;
      if (ev_data.invocations != NReps) {
        std::cerr << "ev_data.invocations=" << ev_data.invocations << " != " << NReps << "!\n";
      }
      if(last_time < ev_data.time) {
        std::cerr << "last_times=" << last_time << " < ev_Data.time=" << ev_data.time << "!\n";
      }
      if (ev_data.currentlyRunning) {
        std::cerr << "ev_data.currentlyRunning!\n";
      }
      last_time = ev_data.time;
    }
  }
  
  return 0;
}
