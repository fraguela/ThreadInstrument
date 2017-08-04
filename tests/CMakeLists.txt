# ThreadInstrument: Library to monitor thread activity
# Copyright (C) 2012-2017 Basilio B. Fraguela. Universidade da Coruna
#
# Distributed under the MIT License. (See accompanying file LICENSE)

# \file     CMakeLists.txt
# \author   Basilio B. Fraguela <basilio.fraguela@udc.es>

cmake_minimum_required( VERSION 2.8 )

include_directories( ${PROJECT_SOURCE_DIR}/include )
add_definitions( -DTHREAD_INSTRUMENT )
link_libraries( thread_instrument )

# Tests based on C++11 threads
set(tests pfor pforlog bench )

foreach(test ${tests})
  add_executable( ${test} ${test}.cpp )
endforeach(test)

#Tests based on OpenMP
find_package( OpenMP QUIET )

if( OPENMP_FOUND OR OpenMP_CXX_FLAGS )
  message(STATUS "Found OpenMP")
  if( NOT OPENMP_FOUND )
    message(STATUS " although OPENMP_FOUND is not set...")
  endif(  NOT OPENMP_FOUND )
  set( tests_openmp pfor_openmp pforlog_openmp )
  foreach(test ${tests_openmp})
    add_executable( ${test} ${test}.cpp )
    target_compile_options( ${test} PUBLIC ${OpenMP_CXX_FLAGS} )
    target_link_libraries( ${test} ${OpenMP_CXX_FLAGS} )
    #set( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}" )
  endforeach(test)
else( OPENMP_FOUND OR OpenMP_CXX_FLAGS )
  message(STATUS "OpenMP not found. Tests skipped...")
endif( OPENMP_FOUND OR OpenMP_CXX_FLAGS )

# Tests based on Intel TBB
find_library( TBB_LIBRARY tbb HINTS $ENV{HOME}/tbb/lib $ENV{HOME}/local/lib )

mark_as_advanced( TBB_LIBRARY )

if( TBB_LIBRARY )
  message(STATUS "Found tbb lib: ${TBB_LIBRARY}")
  get_filename_component( TBB_LIB_DIR ${TBB_LIBRARY} DIRECTORY )
  get_filename_component( TBB_INCL_LIB_DIR ${TBB_LIB_DIR}/../include ABSOLUTE)
  find_path(TBB_INCLUDE_DIR tbb/tbb.h HINTS $ENV{HOME}/tbb/include $ENV{HOME}/local/include ${TBB_INCL_LIB_DIR} )
  message(STATUS "Found tbb include dir: ${TBB_INCLUDE_DIR}")
  mark_as_advanced( TBB_INCLUDE_DIR )

  set( tests_tbb pfor_tbb pforlog_tbb )
  foreach(test ${tests_tbb})
    add_executable( ${test} ${test}.cpp )
    target_include_directories( ${test} PUBLIC ${TBB_INCLUDE_DIR} )
    target_link_libraries( ${test} ${TBB_LIBRARY} )
  endforeach(test)
else( TBB_LIBRARY )
  message(STATUS "TBB not found. Tests skipped..")
endif( TBB_LIBRARY )

list( APPEND tests ${tests_openmp} )
list( APPEND tests ${tests_tbb} )

add_custom_target(check)
foreach(test ${tests})
  add_custom_command(TARGET check PRE_BUILD
                     COMMAND ${CMAKE_COMMAND} -E echo test ${test} : -------------
                     COMMAND ${test} || exit 1
                     COMMAND ${CMAKE_COMMAND} -E echo ------------------------
                    )
endforeach(test)
add_custom_command(TARGET check PRE_BUILD COMMAND echo ${tests} TESTS DONE. SUCCESSFUL)
