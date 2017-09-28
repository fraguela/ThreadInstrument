/*
 ThreadInstrument: Library to monitor thread activity
 Copyright (C) 2012-2017 Basilio B. Fraguela. Universidade da Coruna
 
 Distributed under the MIT License. (See accompanying file LICENSE)
*/

///
/// \file     pforlog.cpp
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include "thread_instrument/thread_instrument.h"

#define END_EVENT ((void *)0x1)

#define N 300

static std::atomic<int> my_io_lock, ntb, nte;

float c[N][N], a[N][N], b[N][N];

//////////////////// EVENT PRINTERS ////////////////////

std::string sprintf_act_printer(void *p) {
  return std::string("SPRINTF_ACT") + (p ? " END" : " BEGIN");
}

std::string wait_act_printer(void *p) {
  return std::string("WAIT_ACT") + (p ? " END" : " BEGIN");
}

std::string paral_act_printer(void *p) {
  return std::string("PARAL_ACT") + (p ? " END" : " BEGIN");
}

std::string seq_act_printer(void *p) {
  return std::string("SEQ_ACT") + (p ? " END" : " BEGIN");
}

std::string misc_act_printer(void *p) {
  return std::string("MISC_ACT") + (p ? " END" : " BEGIN");
}

std::string generic_printer(int event, void *p) {
  return "This was " + std::string(ThreadInstrument::getEventName(event)) + "=" + (p ? " END" : " BEGIN");
}

//////////////////// END EVENT PRINTERS ////////////////////

void myfunnyinspector()
{
  puts("That's all folks!");
  exit(EXIT_SUCCESS);
}

void mx(float *c, float *a, float *b, int n)
{
  for(int i=0; i < n; ++i)
    for(int j=0; j < n; ++j)
      for(int k=0; k < n; ++k)
	c[i * n + j] += a[i * n + k] * b[k * n + j]; 
}

struct ParallelStuff {
  
  const bool silent_;
  const bool do_nice_print_;

  ParallelStuff(bool silent, bool do_nice_print) :
  silent_(silent),
  do_nice_print_(do_nice_print)
  {}
  
  void operator()(int begin, int end) const  {
    char buf[100];
    
    ntb++;
    
    ThreadInstrument::log("SPRINTF_ACT", 0, do_nice_print_);
    sprintf(buf, " [%d, %d) for thread %u\n", begin, end, ThreadInstrument::getMyThreadNumber());
    ThreadInstrument::log("SPRINTF_ACT", END_EVENT, do_nice_print_);
    
    ThreadInstrument::log("PARAL_ACT", 0, do_nice_print_);
    mx((float *)c, (float *)a, (float *)b, 300);
    ThreadInstrument::log("PARAL_ACT", END_EVENT, do_nice_print_);
    
    ThreadInstrument::log("WAIT_ACT", 0, do_nice_print_);
    int should_be = 0;
    while(!my_io_lock.compare_exchange_weak(should_be, 1)) {
      should_be = 0;
    }
    ThreadInstrument::log("WAIT_ACT", END_EVENT, do_nice_print_);
      
    ThreadInstrument::log("SEQ_ACT", 0, do_nice_print_);
    mx((float *)c, (float *)a, (float *)b, 80);
    ThreadInstrument::log("SEQ_ACT", END_EVENT, do_nice_print_);
      
    if(!silent_) {
      ThreadInstrument::log("MISC_ACT", 0, do_nice_print_);
      std::cerr << std::this_thread::get_id() << buf;
      ThreadInstrument::log("MISC_ACT", END_EVENT, do_nice_print_);
    }
    
    my_io_lock = 0;
    
    std::this_thread::yield(); //Otherwise this thread tends to make all the work
    
    nte++;
  }
  
};

void test1(int rangelim, bool do_nice_print, const char * const msg)
{ std::vector<std::thread> v;
  
  if (!do_nice_print) {
    ThreadInstrument::log("RUN_ACT", 0);
  }
  
  my_io_lock = 0;
  ntb = 0;
  nte = 0;
  
  const ParallelStuff ps(false, do_nice_print);
  
  for (int i = 0; i < rangelim; i++) {
    v.emplace_back(ps, i, i+1);
  }
  
  for (auto& t : v) {
    t.join();
  }
  
  if (!do_nice_print) {
    ThreadInstrument::log("RUN_ACT", END_EVENT);
  }

  std::cout << ntb << " tasks begun and " << nte << " tasks ended\nTest: " << msg << std::endl;
  
  ThreadInstrument::dumpLog();
}

int main(int argc, char **argv)
{ std::vector<std::thread> v;

  const int rangelim = (argc == 1) ? std::thread::hardware_concurrency() : atoi(argv[1]);
  
  std::cout << "Running " << rangelim << " tasks\n";

  // Initial test without printers

  test1(rangelim, false, "without printers");
  
   // Test user defined generic event printer
  
  ThreadInstrument::registerLogPrinter(generic_printer);
  
  test1(rangelim, false, "user-defined generic event printer");

  // Test user defined event printers

  ThreadInstrument::registerLogPrinter("SPRINTF_ACT", sprintf_act_printer);
  ThreadInstrument::registerLogPrinter("WAIT_ACT", wait_act_printer);
  ThreadInstrument::registerLogPrinter("PARAL_ACT", paral_act_printer);
  ThreadInstrument::registerLogPrinter("SEQ_ACT", seq_act_printer);
  ThreadInstrument::registerLogPrinter("MISC_ACT", misc_act_printer);
  
  test1(rangelim, true, "user defined printers per event");
  
  // Test signals
  
  ThreadInstrument::registerInspector(myfunnyinspector);
  
  printf("Now the application enters a loop until ~9s. pass by or\n");
  printf("you send a SIGUSR1 signal to retrieve further logs to PID %lu\n", (long unsigned int)getpid());
  
  // 30 * 0.3 = waits ~9 seconds.
  for (int i = 0; i < 30; i++) {
    
    v.clear();

    const ParallelStuff ps_silent(true, true);
    
    for (int i = 0; i < rangelim; i++) {
      v.emplace_back(ps_silent, i, i+1);
    }
    
    for (auto& t : v) {
      t.join();
    }

    usleep(300000);
  }

  kill(getpid(), SIGUSR1);

  return 0;
}
