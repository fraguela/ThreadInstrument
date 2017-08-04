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
#include <thread> //for id and yield
#include <omp.h>
#include "thread_instrument/thread_instrument.h"

#define RUN_ACT     0
#define SPRINTF_ACT 1
#define WAIT_ACT    2
#define PARAL_ACT   3
#define SEQ_ACT     4
#define MISC_ACT    5

#define END_EVENT ((void *)0x1)

const char *activity_names [] = { "RUN_ACT", "SPRINTF_ACT", "WAIT_ACT", "PARAL_ACT", "SEQ_ACT", "MISC_ACT" };

#define N 200

int ntb, nte;

float c[N][N], a[N][N], b[N][N];

//////////////////// EVENT PRINTERS ////////////////////

std::string sprintf_act_printer(void *p) {
  return std::string(activity_names[SPRINTF_ACT]) + (p ? " END" : " BEGIN");
}

std::string wait_act_printer(void *p) {
  return std::string(activity_names[WAIT_ACT]) + (p ? " END" : " BEGIN");
}

std::string paral_act_printer(void *p) {
  return std::string(activity_names[PARAL_ACT]) + (p ? " END" : " BEGIN");
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
  
  bool silent_;
  
  ParallelStuff(bool silent) : silent_(silent)
  {}
  
  void operator()(int begin, int end) const  {
    char buf[100];
    
#pragma omp atomic
    ntb++;
    
    ThreadInstrument::log(SPRINTF_ACT, 0);
    sprintf(buf, " [%d, %d) for thread %u\n", begin, end, ThreadInstrument::getMyThreadNumber());
    ThreadInstrument::log(SPRINTF_ACT, END_EVENT);
    
    ThreadInstrument::log(PARAL_ACT, 0);
    mx((float *)c, (float *)a, (float *)b, 150);
    ThreadInstrument::log(PARAL_ACT, END_EVENT);
    
    ThreadInstrument::log(WAIT_ACT, 0);
#pragma omp critical
 {
    ThreadInstrument::log(WAIT_ACT, 101);
      
    ThreadInstrument::log(SEQ_ACT, 0);
    mx((float *)c, (float *)a, (float *)b, 80);
    ThreadInstrument::log(SEQ_ACT, 101);
      
    if(!silent_) {
      ThreadInstrument::log(MISC_ACT, 0);
      std::cerr << std::this_thread::get_id() << buf;
      ThreadInstrument::log(MISC_ACT, END_EVENT);
    }
    
 }
    
    std::this_thread::yield(); //Otherwise this thread tends to make all the work

#pragma omp atomic
    nte++;
  }
  
};


int main(int argc, char **argv)
{
  ThreadInstrument::log(RUN_ACT, 0);
  
  ntb = 0;
  nte = 0;
  
  const int rangelim = (argc == 1) ? omp_get_max_threads() : atoi(argv[1]);
  
  std::cout << "Running " << rangelim << " tasks\n";

  const ParallelStuff ps(false);
  
#pragma omp parallel
  {
#pragma omp for schedule(static, 1)
    for (int i = 0; i < rangelim; i++) {
      ps(i, i+1);
    }
  }

  ThreadInstrument::log(RUN_ACT, END_EVENT);
  
  std::cout << ntb << " tasks begun and " << nte << " tasks ended\n";
  
  ThreadInstrument::dumpLog();
  
  // Now we test signals + user defined event printers

  ThreadInstrument::registerLogPrinter(SPRINTF_ACT, sprintf_act_printer);
  ThreadInstrument::registerLogPrinter(WAIT_ACT, wait_act_printer);
  ThreadInstrument::registerLogPrinter(PARAL_ACT, paral_act_printer);
  
  ThreadInstrument::registerInspector(myfunnyinspector);
  
  printf("Now the application enters a loop until ~9s. pass by or\n");
  printf("you send a SIGUSR1 signal to retrieve further logs to PID %lu\n", (long unsigned int)getpid());
  
  // 30 * 0.3 = waits ~9 seconds.
  for (int i = 0; i < 30; i++) {

    const ParallelStuff ps_silent(true);
    
#pragma omp parallel
  {
#pragma omp for schedule(static, 1)
      for (int i = 0; i < rangelim; i++) {
      ps_silent(i, i+1);
    }
  }

    usleep(300000);
  }

  kill(getpid(), SIGUSR1);

  return 0;
}
