/*
 ThreadInstrument: Library to monitor thread activity
 Copyright (C) 2012-2018 Basilio B. Fraguela. Universidade da Coruna
 
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

unsigned NThreads, NActivities, NReps, Case;

typedef void (*void_f_int)(unsigned);

void_f_int F2Run = nullptr;

/* Progresses from level 0 -> level 1 -> ... -> level NActivities
   Level 0 is the one that makes NReps iterations
 */
void f(unsigned level)
{
  if (!level) {
    switch (Case) {
        
      case 0:
        for(unsigned i = 0; i < NReps; i++) {
          ThreadInstrument::beginActivity(level);
          f(level + 1);
          ThreadInstrument::endActivity(level);
        }
        break;
        
      case 1:
        for(unsigned i = 0; i < NReps; i++) {
          ThreadInstrument::beginActivity("MYTASK");
          f(level + 1);
          ThreadInstrument::endActivity("MYTASK");
        }
        break;
        
      case 2:
        for(unsigned i = 0; i < NReps; i++) {
          THREADINSTRUMENT_PROF("MYTASK",
                                f(level + 1);
                                );
        }
        break;
        
      default:
        std::cerr << "Unknown test " << Case << '\n';
        exit(EXIT_FAILURE);
        break;
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

double runtest(int ncase)
{ std::vector<std::thread> v;
  
  Case = ncase;

  for (int i = 0; i < NThreads; i++) {
    v.emplace_back(f, 0);
  }
  
  // Notice that threads may have been already running at this point
  
  ThreadInstrument::time_point_t t0 = ThreadInstrument::clock_t::now();
  
  F2Run = f;
  
  for (auto& t : v) {
    t.join();
  }
  
  F2Run = nullptr;
  
  ThreadInstrument::time_point_t t1 = ThreadInstrument::clock_t::now();

  return std::chrono::duration<double>(t1 - t0).count();
}

int main(int argc, char **argv)
{
  
  std::cout << "[NThreads] [NActivities] [NReps per activity]\n";
  NThreads    = (argc < 2) ? std::thread::hardware_concurrency() : atoi(argv[1]);
  NActivities = (argc < 3) ? 10 : atoi(argv[2]);
  NReps       = (argc < 4) ? 1000 : atoi(argv[3]);

  const size_t total_activities = size_t(NThreads) * size_t(NReps) * size_t(NActivities);
  
  std::cout << "NThreads=" << NThreads<< " NActivities=" << NActivities << " NReps=" << NReps << '\n';
  std::cout << "Each thread runs " << NReps << " iterations, each one profiling " << NActivities << " different activities\n";
  std::cout << " => a total of " << NThreads << " * (" << NReps << " * " << NActivities << ") = " << total_activities << " activity periods are measured\n";
  double time_0 = runtest(0);
  
  std::cout << "Profiling Time=" << time_0 << "s. or " << (time_0/(total_activities / NThreads)) << "s. per activity period\n";
  std::cout << "nThreadsWithActivity()=" << ThreadInstrument::nThreadsWithActivity() << '\n';


  // verify correctness
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
  
  std::cout << "=================\nCompare performance of profiling APIs:\n";
  
  NReps      *= NActivities;
  NActivities = 1;
  
  std::cout << "NThreads=" << NThreads<< " NActivities=" << NActivities << " NReps=" << NReps << '\n';
  std::cout << "Profiling Time using int   =" << runtest(0) << '\n';
  std::cout << "Profiling Time using char *=" << runtest(1) << '\n';
  std::cout << "Profiling Time using macro =" << runtest(2) << '\n';

  return 0;
}
