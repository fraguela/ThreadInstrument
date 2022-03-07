/*
 ThreadInstrument: Library to monitor thread activity
 Copyright (C) 2012-2022 Basilio B. Fraguela. Universidade da Coruna
 
 Distributed under the MIT License. (See accompanying file LICENSE)
*/

///
/// \file     thread_instrument.cpp
/// \brief    Library implementation
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdio>
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <atomic>
#include "thread_instrument/thread_instrument.h"

namespace {
  
  struct IdentifiedEventData {
    
    const unsigned id_;   ///< # of the thread associated
    ThreadInstrument::Int2EventDataMap_t int2EventDataMap_;

    IdentifiedEventData(unsigned in_id) noexcept
    : id_(in_id)
    {}
    
  };

  
  /// Number of threads that have registered profiling activity
  std::atomic<unsigned> NProfiledThreads {0};

  
  /// Structure that supports a multiple-reader single-writer access protocol
  struct AccessControl_t {
    
    std::atomic<int> readers_;
    std::atomic<bool> writer_;
    
    AccessControl_t() noexcept
    //: readers_(ATOMIC_VAR_INIT(0)),
    //  writer_(ATOMIC_VAR_INIT(false)) //Raises warning: braces around scalar initializer
    {
      readers_ = ATOMIC_VAR_INIT(0);
      writer_ = ATOMIC_VAR_INIT(false);
    }
    
    ~AccessControl_t()
    {
      assert(readers_ == 0);
      assert(!writer_);
    }
    
    void reader_enter() noexcept {
      do {
        readers_++;
        
        if (writer_) {
          readers_--;
          while (writer_); // spin
        } else {
          break;
        }
      } while (1);
    }
    
    void reader_exit() noexcept {
      readers_--;
    }
    
    void writer_enter() noexcept {
      while (writer_.exchange(true)); // spin
      while (readers_); // spin
    }
    
    void writer_exit() noexcept {
      writer_ = false;
    }
    
  };

  
  /// Lock-free single-linked list that only supports push at the head and pop either from the head or the bottom
  ///plus reversals
  template<typename T>
  class ConcurrentSList {
    
    struct Node {
      Node * next_;
      T item_;
    };
    
    std::atomic<Node*> head_;
    
    void push(Node * const newp) noexcept
    {
      newp->next_ = head_.load(std::memory_order_relaxed);
      
      while(!head_.compare_exchange_weak(newp->next_, newp, std::memory_order_release, std::memory_order_relaxed));
    }
    
  public:
    
    class iterator {

      Node *pos_;
      
      void advance() { pos_ = pos_->next_; }

    public:
      
      iterator(Node *pos = nullptr) noexcept :
      pos_(pos)
      { }
      
      iterator(const iterator& other) noexcept :
      pos_(other.pos_)
      { }

      bool operator==(const iterator& other) const noexcept {
        return pos_ == other.pos_;
      }

      bool operator!=(const iterator& other) const noexcept {
        return pos_ != other.pos_;
      }
      
      /// Prefix increment
      iterator& operator++() noexcept {
        advance();
        return *this;
      }
      
      /// Postfix increment
      iterator operator++(int) noexcept {
        iterator tmp(*this);
        advance();
        return tmp;
      }
      
      T& operator*() const noexcept {
        return pos_->item_;
      }

      T* operator->() const noexcept {
        return &(pos_->item_);
      }

    };
    
    ConcurrentSList() noexcept :
    head_{nullptr}
    { }
    
    void push(T&& val) {
      push(new Node {nullptr, std::move(val) });
    }
    
    void push(const T& val) {
      pusn(new Node {nullptr, val });
    }
    
    size_t unsafe_size() const noexcept {
      size_t sz = 0;
      for (Node * p = head_.load(std::memory_order_relaxed); p != nullptr; p = p->next_) {
        sz++;
      }
      return sz;
    }
    
    void clear() noexcept {
      Node *q;
      for (Node * p = head_.load(std::memory_order_relaxed); p != nullptr;  p = q) {
        q = p->next_;
        delete p;
      }
      head_ = nullptr;
    }

    /// From bottom/end of the list
    bool try_pop(T& val) noexcept {
      Node *q, *p = head_.load(std::memory_order_relaxed);
      
      if(p == nullptr) {
        return false;
      }
      
      while (p->next_ != nullptr) {
        q = p;
        p = p->next_;
      }
      
      val = p->item_;
      
      if (p == head_) {
        head_ = nullptr;
      } else {
        q->next_ = nullptr;
      }
      
      delete p;
      
      return true;
    }
    
    void reverse() noexcept {
      Node *p = head_.load(std::memory_order_relaxed), *pp1, *pp2;
      if (p != nullptr) {
        for(pp1 = p->next_; pp1 != nullptr; pp1 = pp2) {
          pp2 = pp1->next_;
          pp1->next_ = p;
          p = pp1;
        }
        (*head_).next_ = nullptr;
        head_ = p;
      }
    }
    
    /// From top/begin of the list
    bool try_head_pop(T& val) noexcept {
      Node *p = head_.load(std::memory_order_relaxed);
      
      if(p == nullptr) {
        return false;
      }
      
      val = p->item_;
      
      head_ = p->next_;
      
      delete p;
      
      return true;
    }
    
    iterator begin() const noexcept { return iterator(head_); }
    iterator end() const noexcept { return iterator(nullptr); }

  };

  
  /// Poor man's map based on a ConcurrentSList
  template<typename Key, typename Val>
  class ConcurrentSMap {

    using value_type = typename std::pair<Key, Val>;
    
    ConcurrentSList<value_type> storage_;

  public:

    using iterator = typename ConcurrentSList<value_type>::iterator;

    ConcurrentSMap() = default;
    
    iterator begin() const noexcept { return storage_.begin(); }
    iterator end() const noexcept { return storage_.end(); }
    
    iterator find(const Key& key) noexcept {
      iterator it = begin();
      const iterator it_end = end();
      while ( (it != it_end) && ((*it).first != key) ) {
        ++it;
      }
      return it;
    }
    
    size_t unsafe_size() const noexcept { return storage_.unsafe_size(); }

    std::pair<iterator, bool> emplace(const Key& key, Val&& val) {
      storage_.push(value_type(key, std::forward<Val>(val)));
      return {find(key), true}; // Very inefficient for the general case, but ok here
    }
  };
  
  /// Stores the event data for each thread (one position per thread)
  typedef ConcurrentSMap<std::thread::id, IdentifiedEventData> Thr2Ev_t;
  
  Thr2Ev_t GlobalEventMap;

  IdentifiedEventData& GetThreadRawData(const std::thread::id this_id)
  {

    auto it = GlobalEventMap.find(this_id);
    if (it == GlobalEventMap.end()) {
      auto ins_pair = GlobalEventMap.emplace(this_id, IdentifiedEventData(NProfiledThreads++));
      assert(ins_pair.second);
      it = ins_pair.first;
    }
    
    IdentifiedEventData& ret= it->second;

    return ret;
  }
  
  IdentifiedEventData& GetMyThreadRawData()
  {
    return GetThreadRawData(std::this_thread::get_id());
  }
  
  ThreadInstrument::Int2EventDataMap_t& getMyThreadData()
  {
    return GetMyThreadRawData().int2EventDataMap_;
  }
  
  ThreadInstrument::Int2EventDataMap_t& getThreadDataByNumber(int id) noexcept
  { Thr2Ev_t::iterator it;

    for (it = GlobalEventMap.begin(); it != GlobalEventMap.end(); ++it) {
      if (it->second.id_ == id) {
        break;
      }
    }

    assert(it != GlobalEventMap.end());
    
    ThreadInstrument::Int2EventDataMap_t& ret = it->second.int2EventDataMap_;

    return ret;
  }

  /////////////////////////// LOGS ///////////////////////////

  /// Records the beginning of the execution of the program
  const ThreadInstrument::time_point_t StartExecutionTimePoint = ThreadInstrument::clock_t::now();

  struct LogEvent {

    std::thread::id id_;
    ThreadInstrument::time_point_t when_;
    void *data_;
    unsigned event_id_;

    LogEvent(ThreadInstrument::time_point_t when, unsigned event_id, void *data) noexcept
    : id_(std::this_thread::get_id()), when_(when), data_(data), event_id_(event_id)
    { }

    LogEvent(unsigned event_id, void *data) noexcept
    : id_(std::this_thread::get_id()), data_(data), event_id_(event_id)
    { }

    LogEvent()
    {}

  };

  /// Maximum number of log events to dump. By default there is no limit
  /** @internal Notice that all the event are actually logged; but only the last LogLimit ones are dumped. */
  unsigned LogLimit = 0;

  /// Log storage
  ConcurrentSList<LogEvent> Log;

  /// Control whether the Log is locked
  bool Locked_Log = false;

  /// Provides printer for each kind of event
  std::map<unsigned, ThreadInstrument::LogPrinter_t> LogPrinters;
  
  /// User function to run when SIGUSR1 is received
  void (*Inspector)();
  
  /// Generic printer for all events
  ThreadInstrument::AllLogPrinter_t AllLogPrinter = ThreadInstrument::defaultPrinter;
  
  /// Dumps ::Log when a SIGUSR1 is received 
  void catch_function(int signal)
  { 
    //fprintf(stderr, "Signal %d catched by ThreadInstrument\n", signal);
  
    if(Inspector) {
      // It is the inspector's job to dump the data if it wishes
      (*Inspector)();
    } else {
      ThreadInstrument::dumpLog();
    }
  }
  
  /// Contains code to be run statically at program initialization
  struct RunThisStatically {
    RunThisStatically() {
      Inspector = nullptr;
      if (signal(SIGUSR1, catch_function) == SIG_ERR) {
	fputs("An error occurred while setting a signal handler.\n", stderr);
      }
    }
  };
  
  const RunThisStatically __A__;
  
  /// Collects and numbers event names in a thread safe way
  /// \internal the names are not copied, thus the object does not ownn them
  class SafeEventCollector {
    
    struct ltstr {
      bool operator()(const char* s1, const char* s2) const noexcept
      {
        return strcmp(s1, s2) < 0;
      }
    };
    
    AccessControl_t access_control_;  ///< Controls parallel accesses to the map
    std::map<const char *, int, ltstr> eventNames_;
    
  public:
    
    SafeEventCollector() = default;
    
    /// Registers the event if not registered and returns its code
    int registerEvent(const char *name)
    { int num;
      
      access_control_.reader_enter();

      auto it = eventNames_.find(name);
      const bool found = (it != eventNames_.end());
      
      access_control_.reader_exit();

      if (found) {
        num = it->second;
      } else {
        access_control_.writer_enter();
        it = eventNames_.find(name); // may be the name was inserted in the meantime
        if(it != eventNames_.end()) {
          num = it->second;
        } else {
          num = eventNames_.size();
          eventNames_.insert({name, num});
        }
        access_control_.writer_exit();
      }
      //fprintf(stderr, "%s->%d %p\n", name, num);
      return num;
    }

    /// Get the string associated to an event number
    const char *name(unsigned event) const noexcept
    {
      for(const auto& pairs : eventNames_) {
        if (pairs.second == event) {
          return pairs.first;
        }
      }
      
      return nullptr;
    }

  };
  
  SafeEventCollector& TheSafeEventCollector()
  { static SafeEventCollector SEC;
    
    return SEC;
  }
  
  // Used by the event printers provided
  static const std::string C_Event_Str("Event");

} // anonymous namespace


namespace ThreadInstrument {

  EventData& EventData::operator+= (const EventData& other) noexcept {
    time += other.time;
    invocations += other.invocations;
    currentlyRunning = currentlyRunning || other.currentlyRunning;
    return *this;
  }

namespace internal {

  void begin_activity_inner(int activity)
  {
    Int2EventDataMap_t& m = getMyThreadData();
    EventData& ed = m[activity];
    
    assert(!ed.currentlyRunning);
    
    ed.invocations++;
    ed.lastInvocation = clock_t::now();
    ed.currentlyRunning = true;
  }
  
  void end_activity_inner(int activity)
  {
    const auto t = clock_t::now();
    Int2EventDataMap_t& m = getMyThreadData();
    Int2EventDataMap_t::iterator it = m.find(activity);
    assert(it != m.end());
    EventData& ed = (*it).second;
    
    assert(ed.currentlyRunning);
    
    ed.time += std::chrono::duration<double>(t - ed.lastInvocation).count();
    ed.lastInvocation = t;
    ed.currentlyRunning = false;
  }

}; // internal

  unsigned nThreadsWithActivity() noexcept
  {
    return GlobalEventMap.unsafe_size();
  }
  
  unsigned getMyThreadNumber()
  {
    return GetMyThreadRawData().id_;
  }
  
  Int2EventDataMap_t& getActivity(unsigned n)
  {
    assert(n < nThreadsWithActivity());
    return getThreadDataByNumber(n);
  }
  
  Int2EventDataMap_t getAllActivity()
  {
    const unsigned n = nThreadsWithActivity();
    if(!n) return Int2EventDataMap_t();
    
    Int2EventDataMap_t m(getThreadDataByNumber(0));
    
    for(unsigned i = 1; i < n; ++i) {
      const Int2EventDataMap_t& m2 = getThreadDataByNumber(i);
      Int2EventDataMap_t::const_iterator itend = m2.end();
      for(Int2EventDataMap_t::const_iterator it = m2.begin(); it != itend; ++it) {
	EventData& r = m[(*it).first];
	r += (*it).second;
      }
    }

    return m;
  }
  
  void clearAllActivity() noexcept
  {
    for (Thr2Ev_t::iterator it = GlobalEventMap.begin(); it != GlobalEventMap.end(); ++it) {
      it->second.int2EventDataMap_.clear();
    }
  }

  void dumpActivity(const Int2EventDataMap_t& m, const std::string *names, std::ostream& s)
  { char buf_final[256];

    const Int2EventDataMap_t::const_iterator itend = m.end();
    for(Int2EventDataMap_t::const_iterator it = m.begin(); it != itend; ++it) {
      int activity = (*it).first;
      const char * const activity_name = ((names != nullptr) && (!names[activity].empty())) ? names[activity].c_str() : getEventName(activity);
      if(activity_name != nullptr)
	sprintf(buf_final, "Event %16s : %lf seconds %u invocations\n", activity_name, (*it).second.time, (*it).second.invocations);
      else
	sprintf(buf_final, "Event %u : %lf seconds %u invocations\n", activity, (*it).second.time, (*it).second.invocations);
      s << buf_final;
    }
  }

  void dumpActivity(const Int2EventDataMap_t& m, const std::string *names, const std::string& filename)
  {
    std::ofstream outfile(filename.c_str(), std::ios_base::out | std::ios_base::app);
    dumpActivity(m, names, outfile);
    outfile.close();
  }
  
  int getEventNumber(const char *event)
  {
    return TheSafeEventCollector().registerEvent(event);
  }
  
  const char *getEventName(const int event) noexcept
  {
    return TheSafeEventCollector().name(event);
  }

  /////////////////////////// LOGS ///////////////////////////
  
namespace internal {
  
  void log_inner(unsigned event, void *data)
  {
    if (!Locked_Log) {
      Log.push(LogEvent(event, data));
    }
  }

  void log_inner(unsigned event, int data)
  {
    if (!Locked_Log) {
      Log.push(LogEvent(event, reinterpret_cast<void*>(data)));
    }
  }

  void timed_log_inner(unsigned event, void *data)
  {
    if (!Locked_Log) {
      Log.push(LogEvent(ThreadInstrument::clock_t::now(), event, data));
    }
  }

  void timed_log_inner(unsigned event, int data)
  {
    if (!Locked_Log) {
      Log.push(LogEvent(ThreadInstrument::clock_t::now(), event, reinterpret_cast<void*>(data)));
    }
  }

}; // internal


  void dumpLog(std::ostream& s)
  { char buf_final[256];
    LogEvent l;

    const std::map<unsigned, LogPrinter_t>::const_iterator itend = LogPrinters.end();
    
    Log.reverse();

    if (LogLimit) {
      const size_t cur_size= Log.unsafe_size();
      while (LogLimit < cur_size) {
	unsigned how_many = cur_size - LogLimit;
	//s << "Removing " << how_many << " log entries\n";
	while(how_many--)
	  Log.try_head_pop(l);
      } 
    }

    //s << "*** DUMPING ThreadInstrument LOG ***\n";
    while (Log.try_head_pop(l)) {
      const unsigned thread_num = GetThreadRawData(l.id_).id_;
      const double when = std::chrono::duration<double>(l.when_ - StartExecutionTimePoint).count();
      const std::map<unsigned, LogPrinter_t>::const_iterator it = LogPrinters.find(l.event_id_);
      std::string event_representation = (it != itend) ? (*((*it).second))(l.data_) : (*AllLogPrinter)(l.event_id_, l.data_);

      if (when > 0.) {
        sprintf(buf_final, "Th %3u %lf %s\n", thread_num, when, event_representation.c_str());
      } else {
        sprintf(buf_final, "Th %3u %s\n", thread_num, event_representation.c_str());
      }
    
      s << buf_final;
    }
    //s << "*** END ThreadInstrument LOG ***\n";
  }
  
  void dumpLog(const std::string& filename, std::ios_base::openmode mode)
  {
    std::ofstream myfile(filename.c_str(), mode);
    if(!myfile.is_open()) {
      std::cerr << "Unable to open file " << filename << '\n';
      exit(EXIT_FAILURE);
    }
    dumpLog(myfile);
  }
  
  void clearLog()
  {
    Log.clear();
  }
  
  void LockLog()
  {
    Locked_Log = true;
  }

  void UnlockLog()
  {
    Locked_Log = false;
  }

  void logLimit(unsigned nlogs)
  {
    LogLimit = nlogs;
  }
  
  void registerLogPrinter(int event, LogPrinter_t printer)
  {
    if (printer == nullptr) {
      LogPrinters.erase(event);
    } else {
      LogPrinters[event] = printer;
    }
  }
  
  void registerLogPrinter(const char *event, LogPrinter_t printer)
  {
    registerLogPrinter(getEventNumber(event), printer);
  }
  
  void registerLogPrinter(AllLogPrinter_t printer)
  {
    AllLogPrinter = (printer == nullptr) ? defaultPrinter : printer;

    assert(AllLogPrinter != nullptr);
  }

  std::string pictureTimePrinter(int event, void *p)
  { static const std::string labels[] = { " BEGIN", " END" };

    const std::string& label = labels[(p == nullptr) ? 0 : 1];

    const char * const event_name = getEventName(event);

    return (event_name == nullptr) ? (C_Event_Str + std::to_string(event) + label) : (event_name + label);
  }

  std::string defaultPrinter(int event, void *p)
  {
    const std::string label = std::to_string(reinterpret_cast<unsigned long long>(p));
    
    const char * const event_name = getEventName(event);
    
    return (event_name == nullptr) ? (C_Event_Str + std::to_string(event) + label) : (event_name + label);
  }
  
  void registerInspector(void (*inspector)())
  {
    Inspector = inspector;
  }

} //namespace ThreadInstrument
