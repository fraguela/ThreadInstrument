/*
 ThreadInstrument: Library to monitor thread activity
 Copyright (C) 2012-2022 Basilio B. Fraguela. Universidade da Coruna

 Distributed under the MIT License. (See accompanying file LICENSE)
*/

///
/// \file     string_log.cpp
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <thread>
#include "thread_instrument/thread_instrument.h"

constexpr int NPerThread = 20;

std::atomic<int> CommonI;
volatile bool All_Ready{false};
volatile int NThreads;
volatile bool TimeLogs;

void log_helper(const int id, const int log_i)
{
  char * const tmp = (char *)malloc(20);
  sprintf(tmp, "T%d V=%d", id, log_i);
  ThreadInstrument::log(0, tmp, TimeLogs);
}

void thread_func(const int id)
{ int tmp;

  // Wait for everyone to start
  tmp = CommonI.fetch_add(1);
  while (!All_Ready) {
    if (tmp == (NThreads - 1)) {
      CommonI.store(0);
      All_Ready = true;
    }
  }

  while(1) {
    tmp = CommonI.fetch_add(1);
    if (tmp >= NThreads * NPerThread) {
      break;
    }
    log_helper(id, tmp);
  }
}

void do_test(const bool time_logs)
{ std::vector<std::thread> threads;

  TimeLogs = time_logs;

  CommonI.store(0);
  
  for (int i = 0; i < NThreads; i++) {
    threads.push_back(std::thread(thread_func, i));
  }

  for (int i = 0; i < NThreads; i++) {
    threads[i].join();
  }

  ThreadInstrument::dumpLog();
}

int main(int argc, char **argv)
{
  NThreads = (argc == 1) ? std::thread::hardware_concurrency() : atoi(argv[1]);
  
  printf("Using %d threads. %d values/thread\n", NThreads, NPerThread);

  ThreadInstrument::registerLogPrinter([](int event, void *p){
    std::string tmp((char*)p);
    free(p);
    return tmp;
  });
  
  puts("Untimed log");
  do_test(false);

  puts("Timed log");
  do_test(true);

  return EXIT_SUCCESS;
}
