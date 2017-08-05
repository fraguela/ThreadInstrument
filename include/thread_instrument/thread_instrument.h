/*
 ThreadInstrument: Library to monitor thread activity
 Copyright (C) 2012-2017 Basilio B. Fraguela. Universidade da Coruna
 
 Distributed under the MIT License. (See accompanying file LICENSE)
*/

///
/// \file     ThreadInstrument.h
/// \brief    Library header/API
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef THREAD_INSTRUMENT_H
#define THREAD_INSTRUMENT_H

#include <chrono>
#include <map>
#include <string>

/// Contains all the library API
namespace ThreadInstrument {
  
  /**
   @mainpage  ThreadInstrument
   
   @author   Basilio B. Fraguela <basilio.fraguela@udc.es>
   
   A library to help analyze applications parallelized with threads
   
   \tableofcontents
   
   @section ThreadInstrumentIntro Introduction
   
   ThreadInstrument offers two simple mechanisms to analyze a multithreaded program:
   - a profiling system based on events per-thread described in \ref ProfilingDetail.
   - a thread-safe logging utility described in \ref LoggingDetail.
   
   An important characteristic of the library is that its <b>activities only take place if the invocations
   to it are compiled with the macro \c THREAD_INSTRUMENT defined.</b> If the macro is not defined, the library
   does not make anything, having thus no overhead.
   
   
   @section ProfilingDetail Profiling facility
   
   Profiling is performed per thread and based on events defined by the user.
   Each event is represented by an integer and the system records information about it such as the time spent in each event and the number of times it has taken place based on invocations of the user to the functions:
   - begin_activity(int activity), which records the beginnig of an activity.
   - end_activity(int activity), which indicates the thread has finished the activity specified.
   
   At any point during the program the user can request the information on the events recorded,
   which is provided by means of a ::Int2EventDataMap_t object that associates
   the event numbers to objects of the class EventData that hold the information
   associated to the event. Two functions can provide this information:
   - getActivity(unsigned n), which returns the event data for the n-th thread.
   - getAllActivity(), which returns a ::Int2EventDataMap_t that summarizes the data for each event across all the threads.
   
   Finally, the library provides three helper functions:
   - nThreadsWithActivity() indicates how many threads have recorded some event.
   - getMyThreadNumber() returns the thread index for the calling thread.
   - printInt2EventDataMap(const Int2EventDataMap_t& m, const char **names, std::ostream& s) prints the data stored in a ::Int2EventDataMap_t. The second argument is optional, and it allows to provide a string to describe each event, so that <tt>names[i]</tt> is the name of the <tt>i</tt>-th event. The third argument
       is also optional and defaults to std::cout.
   
   
   @section LoggingDetail Logging facility
   
   Logging is also based on events specified as integers. The library has a commong log repository
   for all the threads so that the ordering among them is correctly kept. Entries are made invoking either log(unsigned event, int data, bool timed) or log(unsigned event, void *data, bool timed), 
   where the \c data argument, which allows to store additional information
   with each log entry, is optional and defaults to \c nullptr. Similarly, the \c timed argument indicates
   whether the timing of the event should be recorded, and it defaults to \c false. When dumped, 
   timed entries add the number of seconds since the beginning of the program until the event
   was recorded.

   The log is kept in memory and it can be cleared at any moment by means of clearLog().
   The log is printed either by calling dumpLog(std::ostream& s) or by sending a signal \c SIGUSR1 to the process, the output being sent by default to std:cerr.
   By default the whole log is printed, although logLimit(unsigned nlogs) allows to indicate that only
   the most recent \c nlogs entries must be printed. In order to facilitate printing the information associated
   to each event type, users can register printers that transform the events into std::string. Two kinds of printers are supported:
    - a generic printer of type ::AllLogPrinter_t, which allows to print any event associated to the program, can be registered by means of registerLogPrinter(AllLogPrinter_t printer).
    - specific functions of type ::LogPrinter_t to print the data associated to a given \c event type. They are registered by the function registerLogPrinter(unsigned event, LogPrinter_t printer).
   
   Finally, the log system allows to run an arbitrary function when the process receives a \c SIGUSR1 signal.
   This function is registered by means of registerInspector().
   */

  /// Clock used for profiling
  using clock_t      = std::chrono::high_resolution_clock;
  
  /// A time point for profiling
  using time_point_t = clock_t::time_point;

  /// Records the activity data for an event
  struct EventData {
    
    double time;                    ///< Time spent in this kind of event
    time_point_t lastInvocation;    ///< Last moment this event was running
    unsigned invocations;           ///< Number of times this event took place
    bool currentlyRunning;          ///< Whether the activity is now running or not

    EventData()
    : time(0.0), invocations(0), currentlyRunning(false)
    {}
    
    /// Adds the data of another EventData to this one
    EventData& operator+= (const EventData& other);
  };
  
  /// Associates each event code (an int) with its data
  typedef std::map<int, ThreadInstrument::EventData> Int2EventDataMap_t;
  
  namespace internal {
    void begin_activity_inner(int activity);
  
    void end_activity_inner(int activity);
  };

  /// Reports the number of threads with some activity reported
  unsigned nThreadsWithActivity();
  
  /// Get the number for the calling thread
  unsigned getMyThreadNumber();
  
  /// Get the activity for the \n th thread
  /// Notice that the returned value is a non-const reference,
  ///thus this can be used for example to clear the activity of the thread,
  ///modify the statistics of some activity etc.
  Int2EventDataMap_t& getActivity(unsigned n);
  
  /// Get the activity added for all the threads
  Int2EventDataMap_t getAllActivity();

  /// Clears all the activity statistics, although the numbering of the known threads will be remembered
  void clearAllActivity();

  /// Print the data for the events in a ::Int2EventDataMap_t in the ostream \c s (defaults to std::cout)
  /**
    * @param m Set of events
    * @param names optional names of the events
    * @param s ostream for dumping the data
    */
  void printInt2EventDataMap(const Int2EventDataMap_t& m, const char **names = 0, std::ostream& s = std::cout);

  /// Records the beginning of an \c activity
  inline void begin_activity(int activity) {
#ifdef THREAD_INSTRUMENT
    internal::begin_activity_inner(activity);
#endif
  }
  
  /// Records the end of an \c activity
  inline void end_activity(int activity) {
#ifdef THREAD_INSTRUMENT
    internal::end_activity_inner(activity);
#endif
  }
 
  /////////////////////////// LOGS ///////////////////////////
  
  namespace internal {
    void log_inner(unsigned event, void *data);
  
    void log_inner(unsigned event, int data);
    
    void timed_log_inner(unsigned event, void *data);
    
    void timed_log_inner(unsigned event, int data);
  };
  
  /// Dumps the log to the ostream s, clearing it in the process
  void dumpLog(std::ostream& s = std::cerr);

  /// Clears the log
  void clearLog();

  /// Sets a maximum number of log entries to print
  void logLimit(unsigned nlogs);
  
  /// Transform the data associated to a log entry of a given event type into a std::string
  using LogPrinter_t = std::string (*)(void *);
  
  /// Transform the data associated to any log entry into a std::string
  using AllLogPrinter_t = std::string (*)(unsigned, void *);
  
  /// Registers the function used to build a string that represents each event when dumpLog is invoked
  /** @param event   log event whose printing function is defind
   *  @param printer function that takes the pointer to the event data and returns a string representing it
   */
  void registerLogPrinter(unsigned event, LogPrinter_t printer);

  /// Registers a function used to build a string that represents any event when dumpLog is invoked
  /// Specific printers take precedence on this one.
  /** @param printer function that takes the event number and the pointer to the event data and returns a string representing it */
  void registerLogPrinter(AllLogPrinter_t printer);

  /// Register a inspector function that will be run when a SIGUSR1 signal is received
  /** Only a single function is run, so new each new registration overwrites the previous one */
  void registerInspector(void (*inspector)());
  
  
  /// Logs an event
  inline void log(unsigned event, void *data = nullptr, bool is_timed = false) {
#ifdef THREAD_INSTRUMENT
    if (is_timed) {
      internal::timed_log_inner(event, data);
    } else {
      internal::log_inner(event, data);
    }
#endif
  }
  
  /// Logs an event
  inline void log(unsigned event, int data, bool is_timed = false) {
#ifdef THREAD_INSTRUMENT
    if (is_timed) {
      internal::timed_log_inner(event, data);
    } else {
      internal::log_inner(event, data);
    }
#endif
  }

} //namespace ThreadInstrument

#endif
