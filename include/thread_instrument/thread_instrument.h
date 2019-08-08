/*
 ThreadInstrument: Library to monitor thread activity
 Copyright (C) 2012-2019 Basilio B. Fraguela. Universidade da Coruna
 
 Distributed under the MIT License. (See accompanying file LICENSE)
*/

///
/// \file     thread_instrument.h
/// \brief    Library header/API
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef THREAD_INSTRUMENT_H
#define THREAD_INSTRUMENT_H

#include <fstream>
#include <iostream>
#include <chrono>
#include <map>
#include <string>

/// Contains all the library API
namespace ThreadInstrument {

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
    EventData& operator+= (const EventData& other) noexcept;
  };
  
  /// Associates each event code (an int) with its data
  using Int2EventDataMap_t = std::map<int, ThreadInstrument::EventData>;
  
  namespace internal {
    void begin_activity_inner(int activity);
  
    void end_activity_inner(int activity);
  };

  /// Reports the number of threads with some activity reported
  unsigned nThreadsWithActivity() noexcept;
  
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
  void clearAllActivity() noexcept;

  /// Print the data for the events in a ::Int2EventDataMap_t in the ostream \c s (defaults to std::cout)
  /**
    * If \c names is not provided and the activities were registered using strings, the function will
    *use the names provided during the logging.
    *
    * @param m Set of events
    * @param names names of the events or nullptr
    * @param s ostream for dumping the data
    */
  void dumpActivity(const Int2EventDataMap_t& m, const std::string *names = nullptr, std::ostream& s = std::cout);

  /// Print the data for the events in a ::Int2EventDataMap_t in the file \c filename
  /**
   * If \c names is nullptr and the activities were registered using strings, the function will
   *use the names provided during the logging.
   *
   * @param m Set of events
   * @param names names of the events or nullptr
   * @param filename name of the file for dumping the data
   */
  void dumpActivity(const Int2EventDataMap_t& m, const std::string *names, const std::string& filename);
  
  /// Facility for automatically numbering in a thread-safe way events based on their names
  int getEventNumber(const char *event);
  
  /// Gets the name of an event number collected using ::GetEventNumber
  const char *getEventName(const int event) noexcept;

  /// Records the beginning of an \c activity
  inline void beginActivity(int activity) {
#ifdef THREADINSTRUMENT
    internal::begin_activity_inner(activity);
#endif
  }
  
  /// Records the end of an \c activity
  inline void endActivity(int activity) {
#ifdef THREADINSTRUMENT
    internal::end_activity_inner(activity);
#endif
  }

  /// Records the beginning of an \c activity
  inline void beginActivity(const char *activity) {
#ifdef THREADINSTRUMENT
    internal::begin_activity_inner(getEventNumber(activity));
#endif
  }
  
  /// Records the end of an \c activity
  inline void endActivity(const char *activity) {
#ifdef THREADINSTRUMENT
    internal::end_activity_inner(getEventNumber(activity));
#endif
  }

  /////////////////////////// LOGS ///////////////////////////
  
  namespace internal {
    void log_inner(unsigned event, void *data);
  
    void log_inner(unsigned event, int data);
    
    void timed_log_inner(unsigned event, void *data);
    
    void timed_log_inner(unsigned event, int data);
  };
  
  /// Dumps the log to the ostream \c s, clearing it in the process
  void dumpLog(std::ostream& s = std::cerr);

  /// Dumps the log to the file \c filename, clearing it in the process
  void dumpLog(const std::string& filename, std::ios_base::openmode mode = std::ios_base::out);

  /// Clears the log
  void clearLog();

  /// Sets a maximum number of log entries to print
  void logLimit(unsigned nlogs);
  
  /// Transform the data associated to a log entry of a given event type into a std::string
  using LogPrinter_t = std::string (*)(void *);
  
  /// Transform the data associated to any log entry into a std::string
  using AllLogPrinter_t = std::string (*)(int, void *);
  
  /// Registers the function used to build a string that represents each event when dumpLog is invoked
  /** @param event   log event whose printing function is defined
   *  @param printer function that takes the pointer to the event data and returns a string representing it
   */
  void registerLogPrinter(int event, LogPrinter_t printer);

  /// Registers the function used to build a string that represents each event when dumpLog is invoked
  /** @param event   log event whose printing function is defined
   *  @param printer function that takes the pointer to the event data and returns a string representing it
   */
  void registerLogPrinter(const char *event, LogPrinter_t printer);
  
  /// Registers a function used to build a string that represents any event when dumpLog is invoked
  /// Specific printers take precedence on this one.
  /** @param printer function that takes the event number and the pointer to the event data and returns a string representing it */
  void registerLogPrinter(AllLogPrinter_t printer);

  /// Printer used by default for the logged events
  std::string defaultPrinter(int event, void *p);
  
  /// A printer for logged events that generates the correct output for the pictureTime application
  std::string pictureTimePrinter(int event, void *p);
  
  /// Register a inspector function that will be run when a SIGUSR1 signal is received
  /** Only a single function is run, so new each new registration overwrites the previous one */
  void registerInspector(void (*inspector)());

  /// Logs an event
  inline void log(int event, void *data = nullptr, bool is_timed = false) {
#ifdef THREADINSTRUMENT
    if (is_timed) {
      internal::timed_log_inner(event, data);
    } else {
      internal::log_inner(event, data);
    }
#endif
  }
  
  /// Logs an event
  inline void log(int event, int data, bool is_timed = false) {
#ifdef THREADINSTRUMENT
    if (is_timed) {
      internal::timed_log_inner(event, data);
    } else {
      internal::log_inner(event, data);
    }
#endif
  }

  /// Logs an event relying on ::GetEventNumber
  inline void log(const char *event, void *data = nullptr, bool is_timed = false) {
#ifdef THREADINSTRUMENT
    log(getEventNumber(event), data, is_timed);
#endif
  }
  
  /// Logs an event relying on ::GetEventNumber
  inline void log(const char *event, int data, bool is_timed = false) {
#ifdef THREADINSTRUMENT
    log(getEventNumber(event), data, is_timed);
#endif
  }

#define THREADINSTRUMENT_COMBINE1(X,Y) X##Y
#define THREADINSTRUMENT_COMBINE(X,Y) THREADINSTRUMENT_COMBINE1(X,Y)
  
#ifdef THREADINSTRUMENT

#define THREADINSTRUMENT_INTL_PROF(STR_ID, ...) {                                                                         \
    static const int THREADINSTRUMENT_COMBINE(_threadinstrument_idx,__LINE__) = ThreadInstrument::getEventNumber(STR_ID); \
    ThreadInstrument::beginActivity(THREADINSTRUMENT_COMBINE(_threadinstrument_idx,__LINE__));                            \
    __VA_ARGS__;                                                                                                          \
    ThreadInstrument::endActivity(THREADINSTRUMENT_COMBINE(_threadinstrument_idx,__LINE__));                              \
  }

#define THREADINSTRUMENT_INTL_LOG(STR_ID, DO_TIMING, ...) {                                                                \
    static const int THREADINSTRUMENT_COMBINE(_threadinstrument_idx,__LINE__) = ThreadInstrument::getEventNumber(STR_ID); \
    ThreadInstrument::log(THREADINSTRUMENT_COMBINE(_threadinstrument_idx,__LINE__), 0, DO_TIMING);                         \
    __VA_ARGS__;                                                                                                          \
    ThreadInstrument::log(THREADINSTRUMENT_COMBINE(_threadinstrument_idx,__LINE__), 1, DO_TIMING);                         \
  }
  
#else  //THREADINSTRUMENT

#define THREADINSTRUMENT_INTL_PROF(STR_ID, ...)            __VA_ARGS__
#define THREADINSTRUMENT_INTL_LOG(STR_ID, DO_TIMING, ...)  __VA_ARGS__

#endif //THREADINSTRUMENT

/// Profile the execution of the statements that follow the event name
/** It is equivalent to <tt>beginActivity(STR_ID); statements; endActivity(STR_ID);</tt> but it is more efficient
 *  thanks to the caching of the integer code associated to the event
 *
 *  @param [in] STR_ID C string identifying the event
 *  @param [in] ...       statement(s)
 *
 * Example Usage:
 * @code
 *    THREADINSTRUMENT_PROF("Initializing", initialize());
 * @endcode
 */
#define THREADINSTRUMENT_PROF(STR_ID, ...)       THREADINSTRUMENT_INTL_PROF(STR_ID, __VA_ARGS__ )
  
/// Log with timing the beginning and the end of the execution of the statements that follow the event name
/** It is equivalent to <tt>log(STR_ID, 0, true); statements; log(STR_ID, 1, true);</tt> but it is more efficient
 *  thanks to the caching of the integer code associated to the event
 *
 *  @param [in] STR_ID C string identifying the event
 *  @param [in] ...       statement(s)
 *
 * Example Usage:
 * @code
 *    THREADINSTRUMENT_TIMED_LOG("Inverse", tiles[dim*n+n] = tiles[dim*n+n].inverse() );
 * @endcode
 */
#define THREADINSTRUMENT_TIMED_LOG(STR_ID, ...) THREADINSTRUMENT_INTL_LOG(STR_ID,  true, __VA_ARGS__ )

/// Log without timing the beginning and the end of the execution of the statements that follow the event name
/** It is equivalent to <tt>log(STR_ID, 0, false); statements; log(STR_ID, 1, false);</tt> but it is more efficient
 *  thanks to the caching of the integer code associated to the event
 *
 *  @param [in] STR_ID C string identifying the event
 *  @param [in] ...       statement(s)
 *
 * Example Usage:
 * @code
 *    THREADINSTRUMENT_LOG("Initializing", initialize());
 * @endcode
*/
#define THREADINSTRUMENT_LOG(STR_ID, ...)       THREADINSTRUMENT_INTL_LOG(STR_ID, false, __VA_ARGS__ )

} //namespace ThreadInstrument

#endif
