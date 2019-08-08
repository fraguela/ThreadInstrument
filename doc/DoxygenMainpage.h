namespace ThreadInstrument {
  /**
   @mainpage  ThreadInstrument
   
   @author   Basilio B. Fraguela <basilio.fraguela@udc.es>
   @copyright Copyright (C) 2012-2019 Basilio B. Fraguela. Universidade da Coruna. Distributed under the MIT License.
  
   A library to help analyze applications parallelized with threads.
   
   \tableofcontents
   
   @section ThreadInstrumentIntro Introduction
   
   ThreadInstrument offers two simple mechanisms to analyze a multithreaded program:
   - a profiling system based on events per-thread described in \ref ProfilingDetail.
   - a thread-safe logging utility described in \ref LoggingDetail.
   
   An important characteristic of the library is that its <b>activities only take place if the invocations
   to it are compiled with the macro \c THREADINSTRUMENT defined.</b> If the macro is not defined, the library
   does not do anything, having thus no overhead.
   
   
   @section ProfilingDetail Profiling facility
   
   Profiling is performed per thread and based on events defined by the user. <b>Events can be represented by either non-negative integers or C-style strings, but both representations should not be mixed</b> because the library internally maps the strings to integers, which are always the low-level representation.
   
   The system records information about the events such as the time spent in each one and the number of times it has taken place based on invocations of the user to the functions:
   - beginActivity(int activity) or beginActivity(const char *activity), which records the beginnig of an activity.
   - endActivity(int activity) or endActivity(const char *activity), which indicates the thread has finished the activity specified.
   
   There is also a helper macro ::THREADINSTRUMENT_PROF that allows to profile a set of statements naming the activity with a C string. This macro is also faster that using the beginActivity() and endActivity() functions based on a C string.
   
   Different threads can safely report the same activities during the same period of time, the associated reports being independently kept. Activities can also be nested, that is, an activity \a A can run inside an activity \a B. Recursivity, however, is not allowed. Thus a thread should not start the profiling of a new activity \a A until the previous one of the same kind \a A run by that thread has finished.

   At any point during the program the user can request the information on the events recorded. 
   This is provided by means of a ::Int2EventDataMap_t object that associates
   the event numbers to objects of the class EventData that hold the information
   associated to the event. Two functions can provide this information:
   - getActivity(unsigned n), which returns the event data for the n-th thread as a ::Int2EventDataMap_t.
   - getAllActivity(), which returns a ::Int2EventDataMap_t that summarizes the data for each event across all the threads.
   
   Other functions provided by this module of the library are:
   - nThreadsWithActivity() indicates how many threads have recorded some event.
   - getMyThreadNumber() returns the thread index for the calling thread.
   - dumpActivity(const Int2EventDataMap_t& m, const std::string *names, std::ostream& s) prints the data stored in a ::Int2EventDataMap_t. The second argument is optional, and it allows to provide a string to describe each event, so that <tt>names[i]</tt> is the name of the <tt>i</tt>-th event. If the pointer is <tt>nullptr</tt>, the library tries to find a C string associated to the internal event number. If such string is not found, the event number will be used to describe the event. The third argument is also optional and defaults to std::cout.
   - dumpActivity(const Int2EventDataMap_t& m, const std::string *names, const std::string& filename) does the same, but printing to the file \c filename.
   - clearAllActivity() clears all the profiling data kept by the library except the number of known threads.
   
   
   @section LoggingDetail Logging facility
   
   Logging is also based on events specified as integers or C strings. The library has a commong log repository
   for all the threads so that the ordering among them is correctly kept. Entries are made invoking any of
     - log(int event, int data, bool timed) or log(const char *event, int data, bool timed)
     - log(int event, void *data, bool timed) or log(const char * event, void *data, bool timed)
   
   where the \c data argument allows to store additional information. In the <tt>void *</tt> versions 
   this argument is optional and it defaults to \c nullptr. The \c timed argument indicates
   whether the timing of the event should be recorded, and it defaults to \c false. When dumped, 
   timed entries include the time in seconds since the beginning of the program until the event
   was recorded.

   Often we want to log the beginning and the end of an activity. Two helper macros, ::THREADINSTRUMENT_TIMED_LOG and
   ::THREADINSTRUMENT_LOG are provided to simplify this task, their arguments being a c string to identify the event
   and the statements of the activity. These macros also have the advantage of caching the integer associated to the event string.
 
   The log is kept in memory and it can be printed either by
    - calling dumpLog(std::ostream& s), whose argument is optional and defauls to std::cerr.
    - calling dumpLog(const std::string& filename, std::ios_base::openmode mode)
    - or by sending a signal \c SIGUSR1 to the process.

   In all the cases, printing is destructive, that is, the printed entries are deleted from the log.
   
   In order to facilitate printing the information associated to each event type, users can register printers that transform the events into std::string. Two kinds of printers are supported:
    - a generic printer of type ::AllLogPrinter_t, which allows to print any event associated to the program, can be registered by means of registerLogPrinter(AllLogPrinter_t printer). The library provides two printers of this kind: ::defaultPrinter and ::pictureTimePrinter,
        which is designed to generate logs from the \c pictureTime application and supports the ::THREADINSTRUMENT_TIMED_LOG log entries. Both printers try to associate events to C string event names. If the event numbers logged are not associated to C strings, a label based on the event number is used.
    - specific functions of type ::LogPrinter_t to print the data associated to a given \c event type. They are registered by the function registerLogPrinter(int event, LogPrinter_t printer).
   
   As mentioned above, the log system allows to run an arbitrary function when the process receives a \c SIGUSR1 signal.
   This function is registered by means of registerInspector() and it is responsible for doing whatever the
   user wants with the log.
 
   Other functions provided by this module of the library are:
   - clearLog() clears the logged data
   - logLimit(unsigned nlogs) indicates that only the \c nlogs most recent entries must be printed by the dumpLog() functions. The discarded entries are deleted in the next invocation to dumpLog().
   
   \section Miscelanea Miscelanea
   
   All the library functions that use C strings to represent events rely on the thread-safe function
   getEventNumber(const char *event), which associates a unique int to each different provided string.
   The string associated to an integer event can be retrieved using getEventName(int event). 
   
   It must be noted that logging based on C strings implies a somewhat larger performance penalty 
   than using integers, as a map must be checked to translate the strings into integers.
   
   \c pictureTime is an application that reads a log generated by ThreadInstrument in which
    - each entry must mark either the beginning or the end of an activity.
    - beginnings are marked with <tt>data=0</tt> and ends with <tt>data=1</tt>.
    - the entries must be timed
    - the output must be printed using the pictureTimePrinter generic printer
   
   in which the first three points are automatically provided by the ::THREADINSTRUMENT_TIMED_LOG macro.
   From this log, the application generates a graphical representation that shows what was each thread doing in each moment. The application has many flags to control the graph that are displayed when it is run without arguments. The output currently supported is a LaTeX file that relies on the \c tikz-timing package to draw the graph.
   */
}

