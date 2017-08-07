/*
 ThreadInstrument: Library to monitor thread activity
 Copyright (C) 2012-2017 Basilio B. Fraguela. Universidade da Coruna
 
 Distributed under the MIT License. (See accompanying file LICENSE)
*/

///
/// \file     pfor_tbb.cpp
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cassert>
#include <iostream>
#include <tbb/task_scheduler_init.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/atomic.h>
#include <tbb/compat/thread>
#include "thread_instrument/thread_instrument.h"

#define RUN_ACT     0
#define SPRINTF_ACT 1
#define WAIT_ACT    2
#define PARAL_ACT   3
#define SEQ_ACT     4
#define MISC_ACT    5

const char *activity_names [] = { "RUN_ACT", "SPRINTF_ACT", "WAIT_ACT", "PARAL_ACT", "SEQ_ACT", "MISC_ACT" };

#define N 200

static tbb::atomic<int> my_io_lock, ntb, nte;

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
  
  void operator()(const tbb::blocked_range<int>& r ) const  {
    char buf[100];
    
    ntb++;
    
    ThreadInstrument::beginActivity(SPRINTF_ACT);
    sprintf(buf, " [%d, %d) for thread %u\n", r.begin(), r.end(), ThreadInstrument::getMyThreadNumber());
    ThreadInstrument::endActivity(SPRINTF_ACT);
    
    ThreadInstrument::beginActivity(PARAL_ACT);
    mx((float *)c, (float *)a, (float *)b, 150);
    ThreadInstrument::endActivity(PARAL_ACT);
    
    ThreadInstrument::beginActivity(WAIT_ACT);
    while(my_io_lock.compare_and_swap(1,0) != 0);
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
{
  tbb::task_scheduler_init init;
  
  ThreadInstrument::beginActivity(RUN_ACT);
  
  my_io_lock = 0;
  ntb = 0;
  nte = 0;
  
  const int rangelim = (argc == 1) ? std::thread::hardware_concurrency() : atoi(argv[1]);
  
  std::cout << "Running " << rangelim << " tasks\n";
  
  tbb::blocked_range<int> arange(0, rangelim, 1);
  
  ParallelStuff ps;
  
  tbb::parallel_for(arange, ps);
  
  ThreadInstrument::endActivity(RUN_ACT);
  
  std::cout << ntb << " tasks begun and " << nte << " tasks ended\n";
  std::cout << ThreadInstrument::nThreadsWithActivity() << " threads with activity\n";
  
  for(unsigned i = 0; i < ThreadInstrument::nThreadsWithActivity(); ++i) {
    std::cout << "--------------------\n";
    std::cout << "Activity for thread " << i << " :\n";
    
    const ThreadInstrument::Int2EventDataMap_t& activity = ThreadInstrument::getActivity(i);
    
#ifdef THREAD_INSTRUMENT
    assert(activity.size() == (i ? 5 : 6));
#else
    assert(!activity.size());
#endif
    
    ThreadInstrument::printInt2EventDataMap(activity, activity_names);
  }
  
  return 0;
}
