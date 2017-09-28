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
   to it are compiled with the macro \c THREADINSTRUMENT defined.</b> If the macro is not defined, the library
   does not make anything, having thus no overhead.
   
   
   @section ProfilingDetail Profiling facility
   
   Profiling is performed per thread and based on events defined by the user.
   Each event is represented by an integer and the system records information about it such as the time spent in each event and the number of times it has taken place based on invocations of the user to the functions:
   - beginActivity(int activity), which records the beginnig of an activity.
   - endActivity(int activity), which indicates the thread has finished the activity specified.
   
   At any point during the program the user can request the information on the events recorded,
   which is provided by means of a ::Int2EventDataMap_t object that associates
   the event numbers to objects of the class EventData that hold the information
   associated to the event. Two functions can provide this information:
   - getActivity(unsigned n), which returns the event data for the n-th thread as a ::Int2EventDataMap_t.
   - getAllActivity(), which returns a ::Int2EventDataMap_t that summarizes the data for each event across all the threads.
   
   Finally, the library provides three helper functions:
   - nThreadsWithActivity() indicates how many threads have recorded some event.
   - getMyThreadNumber() returns the thread index for the calling thread.
   - printInt2EventDataMap(const Int2EventDataMap_t& m, const std::string *names, std::ostream& s) prints the data stored in a ::Int2EventDataMap_t. The second argument is optional, and it allows to provide a string to describe each event, so that <tt>names[i]</tt> is the name of the <tt>i</tt>-th event. The third argument
       is also optional and defaults to std::cout.
   
   
   @section LoggingDetail Logging facility
   
   Logging is also based on events specified as integers. The library has a commong log repository
   for all the threads so that the ordering among them is correctly kept. Entries are made invoking either
   log(int event, int data, bool timed) or log(int event, void *data, bool timed),
   where the \c data argument, which allows to store additional information
   with each log entry, is optional and defaults to \c nullptr. Similarly, the \c timed argument indicates
   whether the timing of the event should be recorded, and it defaults to \c false. When dumped, 
   timed entries add the number of seconds since the beginning of the program until the event
   was recorded. There are also counterparts of these log functions in which the \c event argument can be
   provided by means of a <tt>const char *</tt>. These versions rely on the thread-safe function getEventNumber(const char *event),
   which associates a unique int to each different provided string. The string associated to an integer event can be retrieved
   using getEventName(int event). It must be noted that logging based on c strings implies a somewhat larger performance penalty,
   as a map must be checked to translate the strings into integers.

   Often we want to log the beginning and the end of an activity. Two helper macros, ::THREADINSTRUMENT_TIMED_LOG and
   ::THREADINSTRUMENT_LOG are provided to simplify this task, their arguments being a c string to identify the event
   and the statements of the activity. These macros also have the advantage of caching the integer associated to the event string.
 
   The log is kept in memory and it can be cleared at any moment by means of clearLog().
   The log is printed either by 
    - calling dumpLog(std::ostream& s) 
    - calling dumpLog(const char *filename, std::ios_base::openmode mode)
    - or by sending a signal \c SIGUSR1 to the process.

   By default the output is sent to std::cerr, and the whole log is printed, although logLimit(unsigned nlogs) allows to indicate that only the most recent \c nlogs entries must be printed. In order to facilitate printing the information associated to each event type, users can register printers that transform the events into std::string. Two kinds of printers are supported:
    - a generic printer of type ::AllLogPrinter_t, which allows to print any event associated to the program, can be registered by means of registerLogPrinter(AllLogPrinter_t printer). The library provides two printers of this kind: ::defaultPrinter and ::pictureTimePrinter,
        which is designed to generate logs from the pictureTime application and supports the ::THREADINSTRUMENT_TIMED_LOG log entries.
    - specific functions of type ::LogPrinter_t to print the data associated to a given \c event type. They are registered by the function registerLogPrinter(int event, LogPrinter_t printer).
   
   Finally, the log system allows to run an arbitrary function when the process receives a \c SIGUSR1 signal.
   This function is registered by means of registerInspector() and it is responsible for doing whatever the
   user wants with the log.
 
   */
}

