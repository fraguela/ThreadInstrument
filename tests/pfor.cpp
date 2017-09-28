/*
 ThreadInstrument: Library to monitor thread activity
 Copyright (C) 2012-2017 Basilio B. Fraguela. Universidade da Coruna
 
 Distributed under the MIT License. (See accompanying file LICENSE)
*/

///
/// \file     pfor.cpp
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cassert>
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include "thread_instrument/thread_instrument.h"

#define RUN_ACT     0
#define SPRINTF_ACT 1
#define WAIT_ACT    2
#define PARAL_ACT   3
#define SEQ_ACT     4
#define MISC_ACT    5

const std::string activity_names [] = { "RUN_ACT", "SPRINTF_ACT", "WAIT_ACT", "PARAL_ACT", "SEQ_ACT", "MISC_ACT" };

#define N 200

static std::atomic<int> my_io_lock, ntb, nte;

float c[N][N], a[N][N], b[N][N];

void mx(float *c, float *a, float *b, int n)
{
  for(int i=0; i < n; ++i)
    for(int j=0; j < n; ++j)
      for(int k=0; k < n; ++k)
	c[i * n + j] += a[i * n + k] * b[k * n + j]; 
}

struct ParallelStuff {
  
  ParallelStuff()
  {}
  
  void operator()(int begin, int end) const  {
    char buf[100];
    
    ntb++;
    
    ThreadInstrument::beginActivity(SPRINTF_ACT);
    sprintf(buf, " [%d, %d) for thread %u\n", begin, end, ThreadInstrument::getMyThreadNumber());
    ThreadInstrument::endActivity(SPRINTF_ACT);
    
    ThreadInstrument::beginActivity(PARAL_ACT);
    mx((float *)c, (float *)a, (float *)b, 150);
    ThreadInstrument::endActivity(PARAL_ACT);
    
    ThreadInstrument::beginActivity(WAIT_ACT);
    int should_be = 0;
    while(!my_io_lock.compare_exchange_weak(should_be, 1)) {
      should_be = 0;
    }
    ThreadInstrument::endActivity(WAIT_ACT);
      
    ThreadInstrument::beginActivity(SEQ_ACT);
    mx((float *)c, (float *)a, (float *)b, 80);
    ThreadInstrument::endActivity(SEQ_ACT);
      
    ThreadInstrument::beginActivity(MISC_ACT);
    std::cerr << std::this_thread::get_id() << buf;
    my_io_lock = 0;
    ThreadInstrument::endActivity(MISC_ACT);
      
    std::this_thread::yield(); //Otherwise this thread tends to make all the work
    
    nte++;
  }
  
};


int main(int argc, char **argv)
{ std::vector<std::thread> v;
  
  ThreadInstrument::beginActivity(RUN_ACT);
  
  my_io_lock = 0;
  ntb = 0;
  nte = 0;
  
  const int rangelim = (argc == 1) ? std::thread::hardware_concurrency() : atoi(argv[1]);
  
  std::cout << "Running " << rangelim << " tasks\n";
  
  ParallelStuff ps;
  
  for (int i = 0; i < rangelim; i++) {
    v.emplace_back(ps, i, i+1);
  }
  
  for (auto& t : v) {
    t.join();
  }

  ThreadInstrument::endActivity(RUN_ACT);
  
  std::cout << ntb << " tasks begun and " << nte << " tasks ended\n";
  std::cout << ThreadInstrument::nThreadsWithActivity() << " threads with activity\n";

  for(unsigned i = 0; i < ThreadInstrument::nThreadsWithActivity(); ++i) {
    std::cout << "--------------------\n";
    std::cout << "Activity for thread " << i << " : ";
    
    const ThreadInstrument::Int2EventDataMap_t& activity = ThreadInstrument::getActivity(i);
    
#ifdef THREADINSTRUMENT
    std::cout << "activity.size() = " << activity.size();
#else
    assert(!activity.size());
#endif
  
    std::cout << '\n';
    
    ThreadInstrument::printInt2EventDataMap(activity, activity_names);
  }
  
  return 0;
}
