/*
 ThreadInstrument: Library to monitor thread activity
 Copyright (C) 2012-2017 Basilio B. Fraguela. Universidade da Coruna
 
 Distributed under the MIT License. (See accompanying file LICENSE)
*/

///
/// \file     ThreadInstrument.cpp
/// \brief    Library implementation
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdio>
#include <cassert>
#include <csignal>
#include <iostream>
#include <thread>
#include <atomic>
#include <unordered_map>
#include "thread_instrument/thread_instrument.h"

namespace {
  
  struct IdentifiedEventData {
    
    const unsigned id_;   ///< # of the thread associated
    ThreadInstrument::Int2EventDataMap_t int2EventDataMap_;

    IdentifiedEventData(unsigned in_id)
    : id_(in_id)
    {}
    
  };

  /// Number of threads that have registered profiling activity
  std::atomic<unsigned> NProfiledThreads {0};

  template<typename T>
  class ConcurrentSList {
    
    struct Node {
      Node * next_;
      T item_;
    };
    
    std::atomic<Node*> head_;
    
    void push(Node * const newp)
    {
      newp->next_ = head_.load(std::memory_order_relaxed);
      
      while(!head_.compare_exchange_weak(newp->next_, newp, std::memory_order_release, std::memory_order_relaxed));
    }
    
  public:
    
    class iterator {

      Node *pos_;
      
      void advance() { pos_ = pos_->next_; }

    public:
      
      iterator(Node *pos = nullptr) :
      pos_(pos)
      { }
      
      iterator(const iterator& other) :
      pos_(other.pos_)
      { }

      bool operator==(const iterator& other) const {
        return pos_ == other.pos_;
      }

      bool operator!=(const iterator& other) const {
        return pos_ != other.pos_;
      }
      
      /// Prefix increment
      iterator& operator++() {
        advance();
        return *this;
      }
      
      /// Postfix increment
      iterator operator++(int) {
        iterator tmp(*this);
        advance();
        return tmp;
      }
      
      T& operator*() const {
        return pos_->item_;
      }

      T* operator->() const {
        return &(pos_->item_);
      }

    };
    
    ConcurrentSList() :
    head_{nullptr}
    { }
    
    void push(T&& val) {
      push(new Node {nullptr, std::move(val) });
    }
    
    void push(const T& val) {
      pusn(new Node {nullptr, val });
    }
    
    size_t unsafe_size() const {
      size_t sz = 0;
      for (Node * p = head_; p != nullptr; p = p->next_) {
        sz++;
      }
      return sz;
    }
    
    void clear() {
      Node *q;
      for (Node * p = head_; p != nullptr;  p = q) {
        q = p->next_;
        delete p;
      }
      head_ = nullptr;
    }

    bool try_pop(T& val) {
      Node *q;
      Node *p = head_.load(std::memory_order_relaxed);
      
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
    
    iterator begin() const { return iterator(head_); }
    iterator end() const { return iterator(nullptr); }

  };

  template<typename Key, typename Val>
  class ConcurrentSMap {

    using value_type = typename std::pair<Key, Val>;
    
    ConcurrentSList<value_type> storage_;
    
    std::atomic_flag lock_;
    
  public:

    using iterator = typename ConcurrentSList<value_type>::iterator;

    ConcurrentSMap() :
    lock_{ATOMIC_FLAG_INIT}
    {}
    
    iterator begin() const { return storage_.begin(); }
    iterator end() const { return storage_.end(); }
    
    iterator find(const Key& key) {
      iterator it = begin();
      const iterator it_end = end();
      while ( (it != it_end) && ((*it).first != key) ) {
        ++it;
      }
      return it;
    }
    
    size_t unsafe_size() const { return storage_.unsafe_size(); }

    std::pair<iterator,bool> emplace(const Key& key, Val&& val) {
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
  
  ThreadInstrument::Int2EventDataMap_t& getThreadDataByNumber(int id)
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
    
    LogEvent(ThreadInstrument::time_point_t when, unsigned event_id, void *data)
    : id_(std::this_thread::get_id()), when_(when), data_(data), event_id_(event_id)
    { }
    
    LogEvent(unsigned event_id, void *data)
    : id_(std::this_thread::get_id()), data_(data), event_id_(event_id)
    { }
    
    LogEvent() {}

  };
  
  unsigned LogLimit = 0;  ///< Default is no limit

  ConcurrentSList<LogEvent> Log;

  std::map<unsigned, ThreadInstrument::LogPrinter_t> LogPrinters;
  
  /// User function to run when SIGUSR1 is received
  void (*Inspector)();
  
  /// Generic printer for all events, if provided by user
   ThreadInstrument::AllLogPrinter_t AllLogPrinter = nullptr;
  
  /// Dumps ::Log when a SIGUSR1 is received 
  void catch_function(int signal)
  { 
    //fprintf(stderr, "Signal %d catched by ThreadInstrument\n", signal);
    
    ThreadInstrument::dumpLog();
    if(Inspector)
      (*Inspector)();
  }
  

  struct RunThisStatically {
    RunThisStatically() {
      Inspector = nullptr;
      if (signal(SIGUSR1, catch_function) == SIG_ERR) {
	fputs("An error occurred while setting a signal handler.\n", stderr);
      }
    }
  };
  
  RunThisStatically A;
  
} // anonymous namespace

namespace ThreadInstrument {

  EventData& EventData::operator+= (const EventData& other) {
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

  unsigned nThreadsWithActivity()
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
    unsigned n = nThreadsWithActivity();
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
  
  void clearAllActivity()
  {
    for (Thr2Ev_t::iterator it = GlobalEventMap.begin(); it != GlobalEventMap.end(); ++it) {
      it->second.int2EventDataMap_.clear();
    }
  }

  void printInt2EventDataMap(const Int2EventDataMap_t& m, const char **names, std::ostream& s)
  { char buf_final[256];

    Int2EventDataMap_t::const_iterator itend = m.end();
    for(Int2EventDataMap_t::const_iterator it = m.begin(); it != itend; ++it) {
      int activity = (*it).first;
      if((names != 0) && (names[activity] != 0))
	sprintf(buf_final, "Event %16s : %lf seconds %u invocations\n", names[activity], (*it).second.time, (*it).second.invocations);
      else
	sprintf(buf_final, "Event %u : %lf seconds %u invocations\n", activity, (*it).second.time, (*it).second.invocations);
      s << buf_final;
    }
  }
  
  /////////////////////////// LOGS ///////////////////////////
  
namespace internal {
  
  void log_inner(unsigned event, void *data)
  {
    Log.push(LogEvent(event, data));
  }
  
  void log_inner(unsigned event, int data)
  {
    Log.push(LogEvent(event, reinterpret_cast<void*>(data)));
  }
  
  void timed_log_inner(unsigned event, void *data)
  {
    Log.push(LogEvent(ThreadInstrument::clock_t::now(), event, data));
  }
  
  void timed_log_inner(unsigned event, int data)
  {
    Log.push(LogEvent(ThreadInstrument::clock_t::now(), event, reinterpret_cast<void*>(data)));
  }

}; // internal

  void dumpLog(std::ostream& s)
  { char buf_final[256];
    LogEvent l;

    const std::map<unsigned, LogPrinter_t>::const_iterator itend = LogPrinters.end();
    
    if (LogLimit) {
      while (LogLimit < Log.unsafe_size()) {
	unsigned how_many = Log.unsafe_size() - LogLimit;
	//s << "Removing " << how_many << " log entries\n";
	while(how_many--)
	  Log.try_pop(l);
      } 
    }

    //s << "*** DUMPING ThreadInstrument LOG ***\n";
    while (Log.try_pop(l)) {
      const unsigned thread_num = GetThreadRawData(l.id_).id_;
      const double when = std::chrono::duration<double>(l.when_ - StartExecutionTimePoint).count();
      const std::map<unsigned, LogPrinter_t>::const_iterator it = LogPrinters.find(l.event_id_);
      std::string event_representation =
      (it != itend) ? (*((*it).second))(l.data_) :
                      (AllLogPrinter == nullptr) ? std::to_string(l.event_id_) + ' ' + std::to_string(reinterpret_cast<unsigned long long>(l.data_)):
                                                   (*AllLogPrinter)(l.event_id_, l.data_);
      if (when > 0.) {
        sprintf(buf_final, "Th %3u %lf %s\n", thread_num, when, event_representation.c_str());
      } else {
        sprintf(buf_final, "Th %3u %s\n", thread_num, event_representation.c_str());
      }
    
      s << buf_final;
    }
    //s << "*** END ThreadInstrument LOG ***\n";
  }
  
  void clearLog()
  {
    Log.clear();
  }
  
  void logLimit(unsigned nlogs)
  {
    LogLimit = nlogs;
  }
  
  void registerLogPrinter(unsigned event, LogPrinter_t printer)
  {
    LogPrinters[event] = printer;
  }
  
  void registerLogPrinter(AllLogPrinter_t printer)
  {
    AllLogPrinter = printer;
  }

  void registerInspector(void (*inspector)())
  {
    Inspector = inspector;
  }
  
} //namespace ThreadInstrument
