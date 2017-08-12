#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <algorithm>

/* This program generates a LaTeX file that displays the execution time of a program split by
 activitied based on events generated by a tool such as ThreadInstrument. 
 
 The LaTeX file relies on the tikz-timing package and it can distinguish the activities by colors
 or patterns depending on the options provided. Run the program without argument to see the help.

 The format of each line in the input must have the form:
 [^d]* thread_number event_time event_name [BEGIN|END]
 
 Example input file:
 Th   0 0.2  COMPUTE_MATRIX BEGIN
 Th   0 2    COMPUTE_MATRIX END
 Th   0 2.0  COMPUTE_MATRIX BEGIN
 Th   0 4    COMPUTE_MATRIX END
 Th   0 4.1  DISTRIB BEGIN
 Th   0 4.5  DISTRIB END
 Th   2 0.1  DISTRIB BEGIN
 Th   2 4.2  DISTRIB END
 Th   0 4.6  COMPUTE BEGIN
 Th   2 4.4  COMPUTE BEGIN
 Th   1 0.5  DISTRIB BEGIN
 Th   1 4.4  DISTRIB END
 Th   1 4.5  COMPUTE BEGIN
 Th   1 6.6  COMPUTE END
 Th   1 6.7  GATHER BEGIN
 Th   2 6.2  COMPUTE END
 Th   2 6.3  GATHER BEGIN
 Th   0 6.6  COMPUTE END
 Th   0 6.8  GATHER BEGIN
 Th   1 6.9  GATHER END
 Th   2 6.95 GATHER END
 Th   0 7.1  GATHER END
 Th   0 7.12 PRINT_RESULTS BEGIN
 Th   0 7.5  PRINT_RESULTS END
 */

namespace  {

  const int MXBUF = 256;
  const char * const _SPCS = " \t\n";

  const char * Colors[] = {
    "red",
    "green",
    "blue",
    "cyan",
    "magenta",
    "yellow",
    "black",
    "gray",
    "white",
    "darkgray",
    "lightgray",
    "brown",
    "lime",
    "olive",
    "orange",
    "pink",
    "purple",
    "teal",
    "violet"
  };
  
  const char * Patterns[] = {
    "horizontal lines",
    "vertical lines",
    "north east lines",
    "north west lines",
    "grid",
    "crosshatch",
    "dots",
    "crosshatch dots",
    "fivepointed stars",
    "sixpointed stars",
    "bricks",
    "checkerboard"
  };
  
  double maxTime;
  
  bool AutoColorize = false;
  bool AutoPattern = false;
  bool PatternsUsed = false; //-P or -p
  bool ShowThreads = false;
  bool VerticalSlope = false;
  bool NameActivities = false;
  int Verbosity = 0;
  double RowDist = 2.0;
  unsigned NChars = 40;
  double SkipMax = 0.05;
  bool DoMerge = false;

  double Ratio;
}

struct activity_data {

  unsigned activity_;
  double begin_, end_;
  
  activity_data(unsigned activity = 0, double begin = 0.) :
  activity_(activity),
  begin_(begin),
  end_(0.)
  {}

};

struct ActivityDescription {
  
  static std::string DefaultRepr;

  const std::string name_;
  std::string color_;
  std::string pattern_;

  ActivityDescription(const std::string& name);
};

typedef std::map< unsigned, std::vector<activity_data>> Thr2ActivityMap_t;

Thr2ActivityMap_t Thr2ActivityMap;
std::vector<ActivityDescription> Activities;
std::vector<unsigned> NThreadsPerFile;
std::set<std::string> SilencedActivities;

std::string ActivityDescription::DefaultRepr = "D";

ActivityDescription::ActivityDescription(const std::string& name) :
name_(name),
color_(AutoColorize ? Colors[Activities.size() % (sizeof(Colors) / sizeof(Colors[0]))] : ""),
pattern_(AutoPattern ? Patterns[Activities.size() % (sizeof(Patterns) / sizeof(Patterns[0]))] : "")
{ }

unsigned registerActivity(const std::string& s)
{
  const unsigned sz = Activities.size();
  for (unsigned i = 0; i < sz; ++i) {
    if(Activities[i].name_ == s) {
      return i;
    }
  }

  Activities.push_back(ActivityDescription(s));
  return sz;
}

std::string escapeLatex(const std::string& s)
{ std::string::size_type cur_pos = 0;
  
  std::string ret(s);
  
  do {
    std::string::size_type n = ret.find('_', cur_pos);
    if( n == std::string::npos ) {
      break;
    }
    ret.insert(n, 1, '\\');
    cur_pos = n + 2;
  } while (true);

  return ret;
}

int classifyLabel(const char *label_str)
{
  if (!strcmp(label_str, "BEGIN")) {
    return 0;
  }
  if (!strcmp(label_str, "END")) {
    return 1;
  }
  assert(false);
  return -1;
}


void gatherStatistics()
{
  assert(!Thr2ActivityMap.empty());
  
  double minTime = Thr2ActivityMap.begin()->second.front().begin_;
  maxTime = Thr2ActivityMap.begin()->second.back().end_;
  
  for (const auto& pair : Thr2ActivityMap) {
    double firstTime = pair.second.front().begin_;
    double lastTime = pair.second.back().end_;
    minTime = std::min(firstTime, minTime);
    maxTime = std::max(lastTime, maxTime);
  }
  
  // adjust for minTime = 0;
  for (auto& pair : Thr2ActivityMap) {
    for (activity_data& ac : pair.second) {
      ac.begin_ -= minTime;
      ac.end_ -= minTime;
    }
  }
  
  maxTime -= minTime;
}

/// @internal non thread safe
const char *my_double_to_str(double d)
{ static char double_prinf_buffer[16];
  
  sprintf(double_prinf_buffer, "%.3lf", d);
  return double_prinf_buffer;
}

/// Processes potential gaps between consecutive activities
///
/// \return length of pending space not reflected in output
/// \internal If there is gap (<tt>begin > last</tt>), it is reflected in the output
/// buffer \c buf if the associated text is larger than \c SkipMax.
double inter_activity_process(std::string& buf, double last, double begin)
{
  if (begin > last) {
    const double dif = (begin - last) * Ratio;
    if (dif > SkipMax) {
      buf = std::string(my_double_to_str(dif)) + 'Z';
    } else {
      return dif;
    }
  }

  return 0.;
}

/// \pre \c it points to a valid activity mergeable with the current one and \c inter_region is empty
/// @return whether end of the vector of activities has been reached
bool merge_consecutive_activities(std::vector<activity_data>::const_iterator& it,
                                  double& prev_skip,
                                  std::string& inter_region,
                                  double& char_length,
                                  double& times_per_activity,
                                  const std::vector<activity_data>::const_iterator it_end)
{ bool is_final_chunk;

  const unsigned merged_activity = it->activity_;

  do {
    const auto& ac = *it;
    ++it;
    is_final_chunk = (it == it_end);
    const double next_begin = is_final_chunk ? maxTime : it->begin_;
    
    // skip to add because of potentially skipped Z region due to
    // distance w.r.t next activity
    double next_skip = inter_activity_process(inter_region, ac.end_, next_begin);
    if (!is_final_chunk) {
      next_skip = next_skip / 2.;
    }
    
    const double time_spent = ac.end_ - ac.begin_;
    char_length += time_spent * Ratio + prev_skip + next_skip;
    
    times_per_activity += time_spent;
    prev_skip = next_skip;
  } while (!is_final_chunk && inter_region.empty() && (it->activity_ == merged_activity));

  return is_final_chunk;
}

void print_thread_activities(std::ostream &s,
                             const std::vector<activity_data>& activity_vector,
                             double * const times_per_activity)
{ std::string buffer;
  char double_prinf_buffer[16];

  if (!activity_vector.empty()) {
    double grey_area = 0.;
    bool is_final_chunk;
    auto it = activity_vector.cbegin();
    const auto it_end = activity_vector.cend();

    double prev_skip = inter_activity_process(buffer, 0., it->begin_);

    do {
      std::string inter_region, buffer_entry;

      const auto& ac = *it;
      ++it;
      is_final_chunk = (it == it_end);
      const double next_begin = is_final_chunk ? maxTime : it->begin_;
      
      // skip to add because of potentially skipped Z region due to
      // distance w.r.t next activity
      double next_skip = inter_activity_process(inter_region, ac.end_, next_begin);
      if (!is_final_chunk) {
        next_skip = next_skip / 2.;
      }

      // style
      const std::string& required_color = Activities[ac.activity_].color_;
      const std::string& required_pattern = Activities[ac.activity_].pattern_;
      const bool style_applied = !required_color.empty() || !required_pattern.empty();
      if (style_applied) {
        if (buffer.empty()) {
          buffer = "G";
        }
        buffer_entry = ",[[timing/d/background/.style={";
        if (required_color.empty()) { // patterns are only applied if no colors are applied
          buffer_entry += "pattern=" + required_pattern;
        } else {
          buffer_entry += "fill=" + required_color;
        }
        buffer_entry += "}]]";
      }
      
      // print itself
      const double time_spent = ac.end_ - ac.begin_;
      double char_length = time_spent * Ratio + prev_skip + next_skip;
      if (DoMerge && !is_final_chunk && inter_region.empty() && (it->activity_ == ac.activity_)) {
        is_final_chunk = merge_consecutive_activities(it, next_skip, inter_region, char_length, times_per_activity[ac.activity_], it_end);
      }

      if ((char_length > SkipMax) || !inter_region.empty()) {
        if (grey_area > 0.0) {
          if (grey_area > SkipMax) {
            buffer += std::string(my_double_to_str(grey_area)) + 'U';
          } else {
            char_length += grey_area; // stolen in favor of easier representation
          }
          grey_area = 0.;
        }
        buffer += buffer_entry + my_double_to_str(char_length) + ActivityDescription::DefaultRepr;
        if (NameActivities) {
          buffer += '{' + escapeLatex(Activities[ac.activity_].name_) + '}';
        } else {
          buffer += "{}";
        }
        if (style_applied) {
          buffer += ',';
        }
        buffer += inter_region;
      } else {
        grey_area += char_length;
      }
      

      // statistics + preparation for next activity
      times_per_activity[ac.activity_] += time_spent;
      prev_skip = next_skip;
      
      if (buffer.size() > 250) {
        s << (buffer + "\n   ");
        buffer.clear();
      }
      
    } while (!is_final_chunk);

  } else {
    buffer = (std::to_string(NChars) + 'Z');
  }
  
  s << buffer << "\\\\\n";
}

void dump(std::ostream &s)
{
  const unsigned n_activities = Activities.size();
  const unsigned n_threads    = Thr2ActivityMap.size();

  double times_per_activity[n_threads][n_activities + 1]; //threads x (activit + idle)
  
  s << R"(
\documentclass[11pt]{article}
\usepackage{tikz-timing}
)";
  if (PatternsUsed) {
    s << R"(\usetikzlibrary{patterns})" << '\n';
  }
  s << R"(\begin{document})" << '\n';

  if (Verbosity) {
    std::fill(&(times_per_activity[0][0]), &(times_per_activity[0][0]) + (n_threads * (n_activities +1)), 0.);
  }

  s << "\n%" << maxTime << " s. mapped\n";
  s << "\\begin{tikztimingtable}[timing/rowdist=" << RowDist << "ex]\n";

  Ratio = NChars / maxTime;

  unsigned cur_thread = 0;
  for (auto it = Thr2ActivityMap.begin(); it != Thr2ActivityMap.end(); ++it) {
    if(ShowThreads) {
      s << 'T' << cur_thread;
    }
    s << " & ";
    if (VerticalSlope) {
      s << "G[[timing/slope=0]]";
    }

    print_thread_activities(s, it->second, times_per_activity[cur_thread]);

    cur_thread++;
  }
  
  s << "\\end{tikztimingtable}\n\n";
  

  if (AutoColorize || AutoPattern) {
    const std::string slope_string = VerticalSlope ? "timing/slope=0," : "";
    if (AutoColorize) {
      for (const auto& activity : Activities) {
        s << "\\texttiming[Z]{[[" + slope_string + "timing/d/background/.style={fill=" + activity.color_ + "}]]2D[black]0.01Z} " + escapeLatex(activity.name_) + '\n';
      }
    }
    if (AutoPattern) {
      for (const auto& activity : Activities) {
        s << "\\texttiming[Z]{[[" + slope_string + "timing/d/background/.style={pattern=" + activity.pattern_ + "}]]2D[black]0.01Z} " + escapeLatex(activity.name_) + '\n';
      }
    }
    s << "\\texttiming[Z]{[[" + slope_string + "]]2U[black]0.01Z} very small tasks\n";
  }
  
  if (Verbosity) {
    
    s << "\n% activities:\n";
    
    for (unsigned i = 0; i < n_activities; ++i) {
      s << "% " << i << ' ' << Activities[i].name_ << '\n';
    }
    
    if ((Verbosity > 1) && SilencedActivities.size()) {
      s << "% silenced:";
      for (const auto& str : SilencedActivities) {
        s << ' ' << str;
      }
    }

    for (cur_thread = 0; cur_thread < n_threads; ++cur_thread) {
      s << "\n%";
      for (unsigned i = 0; i < n_activities; ++i) {
        s << "  " << times_per_activity[cur_thread][i];
      }
    }
    
    if (Verbosity > 1) {
      s << "\n%nthreads/file:";
      for (const auto& val : NThreadsPerFile) {
        s << ' ' << val;
      }
    }
    
  }
  
  s << "\n\\end{document}\n";
}


void usage()
{
  std::cout <<
R"(pictureTime [options] <files>
-C             automatically colorize activities
-c act=color   color for activity
-f             fill activities
-l length      length in x (char size)
-m             merge consecutive activities of same kind
-n             name activities in graph
-P             automatically patterns for activities
-p act=pattern pattern for activity
-r dist        row distance in x (char size)
-S skip        do not depict activities < skip x char size
-s activity    silence activity
-t             show thread numbers
-V             vertical slopes
-v level       verbosity level
)";
  exit(EXIT_FAILURE);
}


std::pair<const char *, const char *> extractPair(char *arg)
{
  char * const equal = strchr(arg, '=');
  if (equal == nullptr) {
    std::cerr << "argument does not have the form activity=string\n";
    exit(EXIT_FAILURE);
  }
  *equal = 0;
  return std::pair<const char *, const char *>(arg, equal + 1);
}

void config(int argc, char *argv[])
{ std::pair<const char *, const char *> charpair;
  int i;
  static const char srchArgs[] =
#ifdef __linux__
  "+"
#endif
  "Cc:fl:mnPp:r:S:s:tVv:";
  
  while ((i = getopt(argc, argv, srchArgs)) != -1)
    switch(i) {
      case 'C':
        AutoColorize = true;
        AutoPattern = false;
        break;
      case 'c':
        charpair = extractPair(optarg);
        Activities[registerActivity(charpair.first)].color_ = std::string(charpair.second);
        break;
      case 'f':
        ActivityDescription::DefaultRepr = "U";
        break;
      case 'l':
        NChars = (unsigned)strtoul(optarg, (char **)NULL, 10);
        break;
      case 'm':
        DoMerge = true;
        break;
      case 'n':
        NameActivities = true;
        break;
      case 'P':
        AutoPattern = PatternsUsed = true;
        AutoColorize = false;
        break;
      case 'p':
        charpair = extractPair(optarg);
        Activities[registerActivity(charpair.first)].pattern_ = std::string(charpair.second);
        PatternsUsed = true;
        break;
      case 'r':
        RowDist = strtod(optarg, nullptr);
        break;
      case 'S':
        SkipMax = strtod(optarg, nullptr);
        break;
      case 's':
        SilencedActivities.insert(std::string{optarg});
        break;
      case 't':
        ShowThreads = true;
        break;
      case 'V':
        VerticalSlope =true;
        break;
      case 'v':
        Verbosity = std::max(1, (int)strtoul(optarg, (char **)NULL, 10));
        break;
      case '?':
      default:
        usage();
    }
  
  if(argc <= optind) {
    usage();
  }
}


int main(int argc, char **argv)
{ char *p, bufin[MXBUF];

  config(argc, argv);

  if (argc < 1) {
    std::cerr << "Missing file argument";
    exit(EXIT_FAILURE);
  }
  
  for (int narg = optind; narg < argc; narg++) {

    const char * const filename = argv[narg];
    
    FILE *fin = fopen(filename, "rt");
    if ( fin == nullptr ) {
      printf("File %s not found\n", filename);
      exit(EXIT_FAILURE);
    }
    
    // will be 0 for first file, #threads0 for second file, etc.
    const unsigned cur_base_nthread = Thr2ActivityMap.size();
    
    while( fgets(bufin, MXBUF, fin) != nullptr ) {
      // search first digit
      for (p = bufin; (*p) && ((*p < '0') || (*p > '9')); p++);
      
      if (*p) {
        p = strtok(p, _SPCS);
        unsigned nthread = (int)strtoul(p, nullptr, 0); // take thread number
        nthread += cur_base_nthread;                    // adjust thread number for previous files threads
        p = strtok(nullptr, _SPCS);
        double time_point = strtod(p, nullptr);         // take time point
        const char *act_str = strtok(nullptr, _SPCS);   // take activity
        if (!SilencedActivities.count(std::string{act_str})) {
          const char *label_str = strtok(nullptr, _SPCS); // take BEGIN or END
          
          unsigned nactivity = registerActivity(act_str); // get activity number
          int label = classifyLabel(label_str);
          assert(label >= 0); // BEGIN (0) OR END(1)
          
          // printf("%u %lf %s %s (%u %d)\n", nthread, time_point, act_str, label_str, nactivity, label);
          
          std::vector<activity_data>& vec_act_data = Thr2ActivityMap[nthread];
          if (!label) { // BEGIN
            assert( !vec_act_data.size() || (vec_act_data.back().end_ > 0.) );
            vec_act_data.push_back(activity_data(nactivity, time_point));
          } else {  // END
            assert( vec_act_data.size() && (vec_act_data.back().end_ == 0.) );
            vec_act_data.back().end_ = time_point;
          }
        }
      }
    }
    
    fclose(fin);
    
    NThreadsPerFile.push_back(Thr2ActivityMap.size() - cur_base_nthread);
  }
  
  gatherStatistics();

  dump(std::cout);

  return 0;
}
